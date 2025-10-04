#include <llvm-20/llvm/IR/Constants.h>
#include <llvm-20/llvm/IR/Value.h>

#include <charconv>
#include <string>

#include "Debug.hpp"
#include "Utils.hpp"
#include "codegen/Codegen.hpp"
#include "semantic/SemanticSymbols.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

void Codegen::generate_block(const ResolvedBlock &block) {
    debug_func("");
    for (auto &stmt : block.statements) {
        generate_stmt(*stmt);

        // We exited the current basic block for some reason, so there is
        // no need for generating the remaining instructions.
        if (!m_builder.GetInsertBlock()) break;
    }
    for (auto &&d : block.defers) {
        generate_block(*d->resolvedDefer.block);
    }
}

llvm::Value *Codegen::generate_stmt(const ResolvedStmt &stmt) {
    debug_func("");
    if (auto *expr = dynamic_cast<const ResolvedExpr *>(&stmt)) {
        return generate_expr(*expr);
    }
    if (auto *returnStmt = dynamic_cast<const ResolvedReturnStmt *>(&stmt)) {
        return generate_return_stmt(*returnStmt);
    }
    if (auto *ifStmt = dynamic_cast<const ResolvedIfStmt *>(&stmt)) {
        return generate_if_stmt(*ifStmt);
    }
    if (auto *whileStmt = dynamic_cast<const ResolvedWhileStmt *>(&stmt)) {
        return generate_while_stmt(*whileStmt);
    }
    if (auto *forStmt = dynamic_cast<const ResolvedForStmt *>(&stmt)) {
        return generate_for_stmt(*forStmt);
    }
    if (auto *declStmt = dynamic_cast<const ResolvedDeclStmt *>(&stmt)) {
        return generate_decl_stmt(*declStmt);
    }
    if (auto *assignment = dynamic_cast<const ResolvedAssignment *>(&stmt)) {
        return generate_assignment(*assignment);
    }
    if (auto *block = dynamic_cast<const ResolvedBlock *>(&stmt)) {
        generate_block(*block);
        return nullptr;
    }
    if (dynamic_cast<const ResolvedDeferStmt *>(&stmt)) {
        return nullptr;
    }
    if (auto *switchStmt = dynamic_cast<const ResolvedSwitchStmt *>(&stmt)) {
        return generate_switch_stmt(*switchStmt);
    }
    stmt.dump();
    dmz_unreachable("unknown statement");
}

llvm::Value *Codegen::generate_return_stmt(const ResolvedReturnStmt &stmt) {
    debug_func(stmt.expr->type->to_str());
    if (stmt.expr) {
        auto retType = m_currentFunction->getFnType()->returnType.get();
        if (stmt.expr->type->kind == ResolvedTypeKind::Error) {
            llvm::Value *dst = m_builder.CreateStructGEP(generate_type(*retType), retVal, 1);
            store_value(generate_expr(*stmt.expr), dst, *stmt.expr->type, *stmt.expr->type);
        } else {
            if (auto fnTypeOptional = dynamic_cast<const ResolvedTypeOptional *>(retType)) {
                store_value(generate_expr(*stmt.expr), retVal, *stmt.expr->type, *fnTypeOptional->optionalType);
            } else {
                store_value(generate_expr(*stmt.expr), retVal, *stmt.expr->type,
                            *m_currentFunction->getFnType()->returnType);
            }
        }
    }

    for (auto &&d : stmt.defers) {
        generate_block(*d->resolvedDefer.block);
    }

    assert(retBB && "function with return stmt doesn't have a return block");
    break_into_bb(retBB);
    return nullptr;
}

llvm::Value *Codegen::generate_if_stmt(const ResolvedIfStmt &stmt) {
    debug_func("");
    llvm::Function *function = get_current_function();

    auto *trueBB = llvm::BasicBlock::Create(*m_context, "if.true");
    auto *exitBB = llvm::BasicBlock::Create(*m_context, "if.exit");

    llvm::BasicBlock *elseBB = exitBB;
    if (stmt.falseBlock) elseBB = llvm::BasicBlock::Create(*m_context, "if.false");

    llvm::Value *cond = generate_expr(*stmt.condition);
    m_builder.CreateCondBr(to_bool(cond, *stmt.condition->type), trueBB, elseBB);

    trueBB->insertInto(function);
    m_builder.SetInsertPoint(trueBB);
    generate_block(*stmt.trueBlock);
    break_into_bb(exitBB);

    if (stmt.falseBlock) {
        elseBB->insertInto(function);
        m_builder.SetInsertPoint(elseBB);
        generate_block(*stmt.falseBlock);
        break_into_bb(exitBB);
    }

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);
    return nullptr;
}

llvm::Value *Codegen::generate_while_stmt(const ResolvedWhileStmt &stmt) {
    debug_func("");
    llvm::Function *function = get_current_function();

    auto *header = llvm::BasicBlock::Create(*m_context, "while.cond", function);
    auto *body = llvm::BasicBlock::Create(*m_context, "while.body", function);
    auto *exit = llvm::BasicBlock::Create(*m_context, "while.exit", function);

    m_builder.CreateBr(header);

    m_builder.SetInsertPoint(header);
    llvm::Value *cond = generate_expr(*stmt.condition);
    m_builder.CreateCondBr(to_bool(cond, *stmt.condition->type), body, exit);

    m_builder.SetInsertPoint(body);
    generate_block(*stmt.body);
    break_into_bb(header);

    m_builder.SetInsertPoint(exit);
    return nullptr;
}

llvm::Value *Codegen::generate_for_stmt(const ResolvedForStmt &stmt) {
    debug_func("");
    for (auto &&cond : stmt.conditions) {
        if (cond->type->kind != ResolvedTypeKind::Range && cond->type->kind != ResolvedTypeKind::Slice) {
            cond->type->dump();
            dmz_unreachable("TODO");
        }
    }
    llvm::Function *function = get_current_function();

    auto *header = llvm::BasicBlock::Create(*m_context, "for.cond", function);
    auto *increment = llvm::BasicBlock::Create(*m_context, "for.increment", function);

    auto isize = ResolvedTypeNumber::isize(stmt.location);
    auto llvmisize = generate_type(*isize);
    llvm::Value *counter = allocate_stack_variable("for.counter", *isize);
    std::vector<llvm::Value *> startCaptures(stmt.captures.size(), nullptr);
    std::vector<llvm::Value *> endCaptures(stmt.captures.size(), nullptr);
    std::vector<llvm::Value *> lenghtCaptures(stmt.captures.size(), nullptr);
    for (size_t i = 0; i < stmt.captures.size(); i++) {
        if (auto rangeExpr = dynamic_cast<ResolvedRangeExpr *>(stmt.conditions[i].get())) {
            startCaptures[i] =
                allocate_stack_variable("for.capture." + stmt.captures[i]->name(), *stmt.captures[i]->type);
            auto aux_start = cast_to(generate_expr(*rangeExpr->startExpr), *rangeExpr->startExpr->type, *isize);
            store_value(aux_start, startCaptures[i], *isize, *stmt.captures[i]->type);
            m_declarations[stmt.captures[i].get()] = startCaptures[i];

            endCaptures[i] = cast_to(generate_expr(*rangeExpr->endExpr), *rangeExpr->endExpr->type, *isize);
            lenghtCaptures[i] = m_builder.CreateSub(endCaptures[i], aux_start);
        } else if (auto sliceType = dynamic_cast<ResolvedTypeSlice *>(stmt.conditions[i]->type.get())) {
            startCaptures[i] = allocate_stack_variable("for.capture." + stmt.captures[i]->name(),
                                                       *ResolvedTypePointer::opaquePtr(stmt.captures[i]->location));
            m_declarations[stmt.captures[i].get()] = startCaptures[i];
            auto generatedCond = generate_expr(*stmt.conditions[i], true);
            auto ptrOfPtrInSlice = m_builder.CreateStructGEP(generate_type(*sliceType), generatedCond, 0);
            auto opaquePtrType = ResolvedTypePointer::opaquePtr(sliceType->location);
            auto ptrInSlice = load_value(ptrOfPtrInSlice, *opaquePtrType);
            store_value(ptrInSlice, startCaptures[i], *opaquePtrType, *opaquePtrType);

            auto ptrOfLenInSlice = m_builder.CreateStructGEP(generate_type(*sliceType), generatedCond, 1);
            lenghtCaptures[i] = load_value(ptrOfLenInSlice, *isize);

        } else {
            dmz_unreachable("TODO");
        }
    }

    // Check lenghts
    llvm::BasicBlock *check_length = nullptr;
    if (stmt.captures.size() > 1) {
        auto *not_equal_length = llvm::BasicBlock::Create(*m_context, "for.not.equal.length", function);
        check_length = llvm::BasicBlock::Create(*m_context, "for.check_length", function);
        break_into_bb(check_length);
        for (size_t i = 1; i < stmt.captures.size(); i++) {
            m_builder.SetInsertPoint(check_length);
            auto cond = m_builder.CreateICmpNE(lenghtCaptures[0], lenghtCaptures[i]);
            check_length = llvm::BasicBlock::Create(*m_context, "for.check_length", function);
            m_builder.CreateCondBr(cond, not_equal_length, check_length);
        }
        m_builder.SetInsertPoint(check_length);
        break_into_bb(header);

        // In case not equal at run time
        m_builder.SetInsertPoint(not_equal_length);
        auto fmt = m_builder.CreateGlobalString(stmt.location.to_string() +
                                                ": Aborted: for loop over objects with non-equal lengths\n");
        auto printf_func = m_module->getOrInsertFunction(
            "printf", llvm::FunctionType::get(m_builder.getInt32Ty(), m_builder.getPtrTy(), true));
        m_builder.CreateCall(printf_func, {fmt});
        llvm::Function *trapIntrinsic = llvm::Intrinsic::getOrInsertDeclaration(m_module.get(), llvm::Intrinsic::trap);
        m_builder.CreateCall(trapIntrinsic, {});
    }

    auto *body = llvm::BasicBlock::Create(*m_context, "for.body", function);
    auto *exit = llvm::BasicBlock::Create(*m_context, "for.exit", function);
    // Header to check exit
    break_into_bb(header);
    m_builder.SetInsertPoint(header);
    auto loaded_counter = load_value(counter, *isize);
    auto cond = m_builder.CreateICmpSLT(loaded_counter, lenghtCaptures[0]);
    m_builder.CreateCondBr(cond, body, exit);

    // Body
    m_builder.SetInsertPoint(body);
    generate_block(*stmt.body);
    break_into_bb(increment);

    // Increment counter
    m_builder.SetInsertPoint(increment);
    auto sum = m_builder.CreateAdd(load_value(counter, *isize), llvm::ConstantInt::get(llvmisize, 1));
    store_value(sum, counter, *isize, *isize);
    for (size_t i = 0; i < stmt.captures.size(); i++) {
        if (dynamic_cast<ResolvedRangeExpr *>(stmt.conditions[i].get())) {
            auto added_capture = m_builder.CreateAdd(load_value(startCaptures[i], *stmt.captures[i]->type),
                                                     llvm::ConstantInt::get(llvmisize, 1));
            store_value(added_capture, startCaptures[i], *isize, *stmt.captures[i]->type);
        } else if (auto sliceType = dynamic_cast<ResolvedTypeSlice *>(stmt.conditions[i]->type.get())) {
            auto opaquePtrType = ResolvedTypePointer::opaquePtr(sliceType->location);
            auto added_capture =
                m_builder.CreateGEP(generate_type(*sliceType->sliceType), load_value(startCaptures[i], *opaquePtrType),
                                    m_builder.getInt32(1));
            store_value(added_capture, startCaptures[i], *opaquePtrType, *opaquePtrType);
        } else {
            dmz_unreachable("TODO");
        }
    }
    break_into_bb(header);

    m_builder.SetInsertPoint(exit);
    return nullptr;
}

llvm::Value *Codegen::generate_decl_stmt(const ResolvedDeclStmt &stmt) {
    debug_func(stmt.type->to_str());
    const auto *decl = stmt.varDecl.get();
    llvm::AllocaInst *var = allocate_stack_variable(decl->identifier, *decl->type);

    if (const auto &init = decl->initializer) {
        if (init->type->kind != ResolvedTypeKind::DefaultInit)
            store_value(generate_expr(*init), var, *init->type, *decl->type);
    }
    m_declarations[decl] = var;
    return var;
}

llvm::Value *Codegen::generate_assignment(const ResolvedAssignment &stmt) {
    debug_func("");
    llvm::Value *val = generate_expr(*stmt.expr);
    llvm::Value *assignee = generate_expr(*stmt.assignee, true);
    return store_value(val, assignee, *stmt.expr->type, *stmt.assignee->type);
}

llvm::Value *Codegen::generate_switch_stmt(const ResolvedSwitchStmt &stmt) {
    debug_func("");
    llvm::Function *function = get_current_function();
    auto *elseBB = llvm::BasicBlock::Create(*m_context, "switch.else");
    auto *exitBB = llvm::BasicBlock::Create(*m_context, "switch.exit");

    auto condition = generate_expr(*stmt.condition);

    auto generatedSwitch = m_builder.CreateSwitch(condition, elseBB, stmt.cases.size());
    for (auto &&cas : stmt.cases) {
        auto *caseBB = llvm::BasicBlock::Create(*m_context, "switch.case");
        caseBB->insertInto(function);
        m_builder.SetInsertPoint(caseBB);
        generate_block(*cas->block);
        break_into_bb(exitBB);

        llvm::ConstantInt *cond = static_cast<llvm::ConstantInt *>(generate_expr(*cas->condition));
        generatedSwitch->addCase(cond, caseBB);
    }

    elseBB->insertInto(function);
    m_builder.SetInsertPoint(elseBB);
    generate_block(*stmt.elseBlock);
    break_into_bb(exitBB);

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);
    return nullptr;
}
}  // namespace DMZ
