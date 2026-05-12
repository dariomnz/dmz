#ifdef DEBUG_CODEGEN
#ifndef DEBUG
#define DEBUG
#endif
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/Module.h>
#pragma GCC diagnostic pop

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
    set_debug_location(stmt.location);
    defer([&]() { unset_debug_location(); });

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
    if (auto *breakStmt = dynamic_cast<const ResolvedBreakStmt *>(&stmt)) {
        return generate_break_stmt(*breakStmt);
    }
    if (auto *continueStmt = dynamic_cast<const ResolvedContinueStmt *>(&stmt)) {
        return generate_continue_stmt(*continueStmt);
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
    dmz_unreachable(stmt.location, "unknown statement");
}

llvm::Value *Codegen::generate_return_stmt(const ResolvedReturnStmt &stmt) {
    debug_func(stmt.location);
    if (stmt.expr) {
        auto retType = m_currentFunction->getFnType()->returnType.get();
        if (stmt.expr->type->kind == ResolvedTypeKind::Error) {
            generate_error_trace_push(stmt.location);

            llvm::Value *dst = m_builder.CreateStructGEP(generate_type(*retType), retVal, 1);
            store_value(generate_expr(*stmt.expr), dst, *stmt.expr->type, *stmt.expr->type);
        } else {
            auto fnTypeOptional = dynamic_cast<const ResolvedTypeOptional *>(retType);
            if (fnTypeOptional && stmt.expr->type->kind != ResolvedTypeKind::Optional) {
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
    m_loopExitStack.push(exit);
    m_loopContinueStack.push(header);
    generate_block(*stmt.body);
    m_loopContinueStack.pop();
    m_loopExitStack.pop();
    break_into_bb(header);

    m_builder.SetInsertPoint(exit);
    return nullptr;
}
llvm::Value *Codegen::generate_for_stmt(const ResolvedForStmt &stmt) {
    debug_func("");
    if (stmt.isInline) {
        generate_block(*stmt.body);
        return nullptr;
    }

    llvm::Function *function = get_current_function();
    auto isize = castPtr<ResolvedTypeNumber>(ResolvedTypeNumber::isize(stmt.location));

    // Bloques básicos
    auto *header = llvm::BasicBlock::Create(*m_context, "for.cond", function);
    auto *body = llvm::BasicBlock::Create(*m_context, "for.body", function);
    auto *increment = llvm::BasicBlock::Create(*m_context, "for.increment", function);
    auto *exit = llvm::BasicBlock::Create(*m_context, "for.exit", function);

    // Contador de iteraciones (independiente de la dirección)
    llvm::Value *counter = allocate_stack_variable(stmt.location, "for.counter", *isize);
    store_value(m_builder.getIntN(isize->bitSize, 0), counter, *isize, *isize);

    struct LoopCapture {
        llvm::Value *varAddr;
        llvm::Value *step;  // 1 o -1
        llvm::Value *length;
        bool isSlice;
    };
    std::vector<LoopCapture> caps;

    for (size_t i = 0; i < stmt.captures.size(); i++) {
        if (auto rangeExpr = dynamic_cast<ResolvedRangeExpr *>(stmt.conditions[i].get())) {
            auto startVal = cast_to(generate_expr(*rangeExpr->startExpr), *rangeExpr->startExpr->type, *isize);
            auto endVal = cast_to(generate_expr(*rangeExpr->endExpr), *rangeExpr->endExpr->type, *isize);

            // Dirección dinámica: step = (start <= end) ? 1 : -1
            auto isForward = m_builder.CreateICmpSLE(startVal, endVal);
            auto step = m_builder.CreateSelect(isForward, m_builder.getIntN(isize->bitSize, 1),
                                               m_builder.getIntN(isize->bitSize, -1));

            // length = abs(end - start)
            auto diff = m_builder.CreateSub(endVal, startVal);
            auto absDiff = m_builder.CreateSelect(isForward, diff,
                                                  m_builder.CreateSub(m_builder.getIntN(isize->bitSize, 0), diff));

            auto varAddr = allocate_stack_variable(stmt.location, "for.capture." + stmt.captures[i]->name(), *isize);
            store_value(startVal, varAddr, *isize, *isize);
            m_declarations[stmt.captures[i].get()] = varAddr;

            caps.push_back({varAddr, step, absDiff, false});

        } else if (auto sliceType = dynamic_cast<ResolvedTypeSlice *>(stmt.conditions[i]->type.get())) {
            auto genSlice = generate_expr(*stmt.conditions[i], true);
            auto ptrType = ResolvedTypePointer::opaquePtr(sliceType->location);

            auto ptrLoad = load_value(m_builder.CreateStructGEP(generate_type(*sliceType), genSlice, 0), *ptrType);
            auto lenLoad = load_value(m_builder.CreateStructGEP(generate_type(*sliceType), genSlice, 1), *isize);

            auto varAddr = allocate_stack_variable(stmt.location, "for.capture.slice", *ptrType);
            store_value(ptrLoad, varAddr, *ptrType, *ptrType);
            m_declarations[stmt.captures[i].get()] = varAddr;

            caps.push_back({varAddr, m_builder.getIntN(isize->bitSize, 1), lenLoad, true});
        }
    }

    // Header: check counter < length
    break_into_bb(header);
    m_builder.SetInsertPoint(header);
    auto currCount = load_value(counter, *isize);
    auto hasMore = m_builder.CreateICmpSLT(currCount, caps[0].length);
    m_builder.CreateCondBr(hasMore, body, exit);

    // Body
    m_builder.SetInsertPoint(body);
    m_loopExitStack.push(exit);
    m_loopContinueStack.push(increment);
    generate_block(*stmt.body);
    m_loopContinueStack.pop();
    m_loopExitStack.pop();
    break_into_bb(increment);

    // Increment
    m_builder.SetInsertPoint(increment);
    store_value(m_builder.CreateAdd(currCount, m_builder.getIntN(isize->bitSize, 1)), counter, *isize, *isize);

    for (size_t i = 0; i < caps.size(); i++) {
        if (!caps[i].isSlice) {
            auto val = load_value(caps[i].varAddr, *isize);
            store_value(m_builder.CreateAdd(val, caps[i].step), caps[i].varAddr, *isize, *isize);
        } else {
            auto sliceT = static_cast<ResolvedTypeSlice *>(stmt.conditions[i]->type.get());
            auto ptr = load_value(caps[i].varAddr, *ResolvedTypePointer::opaquePtr(sliceT->location));
            auto nextPtr = m_builder.CreateGEP(generate_type(*sliceT->sliceType), ptr, m_builder.getInt32(1));
            store_value(nextPtr, caps[i].varAddr, *isize, *isize);  // Simplificado: asume opaque
        }
    }
    m_builder.CreateBr(header);

    m_builder.SetInsertPoint(exit);
    return nullptr;
}

llvm::Value *Codegen::generate_decl_stmt(const ResolvedDeclStmt &stmt) {
    debug_func(stmt.type->to_str());

    if (stmt.type->kind == ResolvedTypeKind::Module || stmt.type->kind == ResolvedTypeKind::Function ||
        stmt.type->kind == ResolvedTypeKind::StructDecl || stmt.type->kind == ResolvedTypeKind::Type) {
        return nullptr;
    }

    const auto *decl = stmt.varDecl.get();
    llvm::AllocaInst *var = allocate_stack_variable(stmt.location, decl->identifier, *decl->type);

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
    if (stmt.isInline) {
        int condVal = stmt.condition->get_constant_value()->getInt();
        for (auto &&cas : stmt.cases) {
            for (auto &&cond : cas->conditions) {
                int caseVal = cond->get_constant_value()->getInt();
                if (condVal == caseVal) {
                    generate_block(*cas->block);
                    return nullptr;
                }
            }
        }
        generate_block(*stmt.elseBlock);
        return nullptr;
    }

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

        for (auto &&cond : cas->conditions) {
            llvm::ConstantInt *val = static_cast<llvm::ConstantInt *>(generate_expr(*cond));
            generatedSwitch->addCase(val, caseBB);
        }
    }

    elseBB->insertInto(function);
    m_builder.SetInsertPoint(elseBB);
    generate_block(*stmt.elseBlock);
    break_into_bb(exitBB);

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);
    return nullptr;
}

llvm::Value *Codegen::generate_break_stmt(const ResolvedBreakStmt &stmt) {
    debug_func(stmt.location);

    if (stmt.expr) {
        auto it = m_catchBreakTargets.find(stmt.targetCatch);
        assert(it != m_catchBreakTargets.end());

        llvm::Value *val = generate_expr(*stmt.expr, stmt.targetCatch->type->generate_struct());
        if (it->second.valueAddr) {
            store_value(val, it->second.valueAddr, *stmt.expr->type, *stmt.targetCatch->type);
        }

        for (auto &&d : stmt.defers) {
            generate_stmt(*d);
        }

        m_builder.CreateBr(it->second.exitBB);
        return nullptr;
    }

    if (m_loopExitStack.empty()) dmz_unreachable(stmt.location, "unexpected break statement outside a loop");

    for (auto &&d : stmt.defers) {
        generate_stmt(*d);
    }

    m_builder.CreateBr(m_loopExitStack.top());
    return nullptr;
}

llvm::Value *Codegen::generate_continue_stmt(const ResolvedContinueStmt &stmt) {
    debug_func(stmt.location);
    for (auto &&d : stmt.defers) {
        generate_block(*d->resolvedDefer.block);
    }
    assert(!m_loopContinueStack.empty() && "continue statement outside a loop");
    break_into_bb(m_loopContinueStack.top());
    return nullptr;
}
}  // namespace DMZ
