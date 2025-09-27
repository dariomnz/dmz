#include "codegen/Codegen.hpp"
#include "semantic/SemanticSymbols.hpp"

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

llvm::Value *Codegen::generate_decl_stmt(const ResolvedDeclStmt &stmt) {
    debug_func(stmt.type->to_str());
    const auto *decl = stmt.varDecl.get();
    llvm::AllocaInst *var = allocate_stack_variable(decl->identifier, *decl->type);

    if (const auto &init = decl->initializer) {
        auto tmpArray = dynamic_cast<ResolvedArrayInstantiationExpr *>(decl->initializer.get());
        if (!tmpArray || (tmpArray && tmpArray->type->kind != ResolvedTypeKind::Void))
            store_value(generate_expr(*init), var, *init->type, *decl->type);
    }
    m_declarations[decl] = var;
    return var;
}

llvm::Value *Codegen::generate_assignment(const ResolvedAssignment &stmt) {
    debug_func("");
    llvm::Value *val = generate_expr(*stmt.expr);
    llvm::Value *assignee =  generate_expr(*stmt.assignee, true);
    // if (auto assigmentOperator = dynamic_cast<const ResolvedAssignmentOperator *>(&stmt)) {
    //     if (auto typeNum = dynamic_cast<const ResolvedTypeNumber *>(stmt.assignee->type.get())) {
    //         llvm::Value *ret = nullptr;
    //         auto rhs_value = load_value(rhs, *typeNum);
    //         if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt) {
    //             ret = m_builder.CreateAdd(assignee, val);
    //         } else if (typeNum->numberKind == ResolvedNumberKind::Float) {
    //             ret = m_builder.CreateFAdd(assignee, val);
    //         } else {
    //             dmz_unreachable("not expected type in op_plusplus");
    //         }
    //         store_value(ret, rhs, *typeNum, *typeNum);
    //         return ret;
    //     } else {
    //         dmz_unreachable("not expected type in assigmentOperator " + stmt.assignee->type->to_str());
    //     }
    // } else {
    // }
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
