#include "semantic/CFG.hpp"

namespace DMZ {
CFG CFGBuilder::build(const ResolvedFunctionDecl &fn) {
    ScopedTimer st(Stats::type::CFGTime);
    cfg = {};
    cfg.exit = cfg.insert_new_block();

    int body = insert_block(*fn.body, cfg.exit);

    cfg.entry = cfg.insert_new_block_before(body, true);
    return cfg;
}

static inline bool is_terminator(const ResolvedStmt &stmt) {
    return dynamic_cast<const ResolvedIfStmt *>(&stmt) || dynamic_cast<const ResolvedWhileStmt *>(&stmt) ||
           dynamic_cast<const ResolvedReturnStmt *>(&stmt);
}

int CFGBuilder::insert_block(const ResolvedBlock &block, int succ) {
    const auto &stmts = block.statements;
    for (auto &&d : block.defers) {
        succ = insert_block(*d->resolvedDefer.block, succ);
    }

    bool insertNewBlock = true;
    for (auto it = stmts.rbegin(); it != stmts.rend(); ++it) {
        if (insertNewBlock && !is_terminator(**it)) {
            succ = cfg.insert_new_block_before(succ, true);
        }

        insertNewBlock = dynamic_cast<const ResolvedWhileStmt *>(it->get());
        succ = insert_stmt(**it, succ);
    }

    return succ;
}

int CFGBuilder::insert_stmt(const ResolvedStmt &stmt, int block) {
    if (auto *ifStmt = dynamic_cast<const ResolvedIfStmt *>(&stmt)) {
        return insert_if_stmt(*ifStmt, block);
    }
    if (auto *whileStmt = dynamic_cast<const ResolvedWhileStmt *>(&stmt)) {
        return insert_while_stmt(*whileStmt, block);
    }
    if (auto *expr = dynamic_cast<const ResolvedExpr *>(&stmt)) {
        return insert_expr(*expr, block);
    }
    if (auto *assignment = dynamic_cast<const ResolvedAssignment *>(&stmt)) {
        return insert_assignment(*assignment, block);
    }
    if (auto *declStmt = dynamic_cast<const ResolvedDeclStmt *>(&stmt)) {
        return insert_decl_stmt(*declStmt, block);
    }
    if (auto *returnStmt = dynamic_cast<const ResolvedReturnStmt *>(&stmt)) {
        return insert_return_stmt(*returnStmt, block);
    }
    if (auto *fieldInit = dynamic_cast<const ResolvedFieldInitStmt *>(&stmt)) {
        return insert_expr(*fieldInit->initializer, block);
    }
    if (auto *blockStmt = dynamic_cast<const ResolvedBlock *>(&stmt)) {
        return insert_block(*blockStmt, block);
    }
    if (dynamic_cast<const ResolvedDeferStmt *>(&stmt)) {
        return block;
    }
    if (auto *switchStmt = dynamic_cast<const ResolvedSwitchStmt *>(&stmt)) {
        return insert_switch_stmt(*switchStmt, block);
    }
    stmt.dump();
    dmz_unreachable("unexpected expression");
}

int CFGBuilder::insert_return_stmt(const ResolvedReturnStmt &stmt, int block) {
    for (auto &&d : stmt.defers) {
        block = insert_block(*d->resolvedDefer.block, block);
    }

    block = cfg.insert_new_block_before(cfg.exit, true);

    cfg.insert_stmt(&stmt, block);
    if (stmt.expr) return insert_expr(*stmt.expr, block);

    return block;
}

int CFGBuilder::insert_expr(const ResolvedExpr &expr, int block) {
    cfg.insert_stmt(&expr, block);

    if (const auto *call = dynamic_cast<const ResolvedCallExpr *>(&expr)) {
        for (auto it = call->arguments.rbegin(); it != call->arguments.rend(); ++it) {
            insert_expr(**it, block);
        }
        return block;
    }
    if (const auto *memberExpr = dynamic_cast<const ResolvedMemberExpr *>(&expr)) {
        return insert_expr(*memberExpr->base, block);
    }
    if (const auto *grouping = dynamic_cast<const ResolvedGroupingExpr *>(&expr)) {
        return insert_expr(*grouping->expr, block);
    }
    if (const auto *binop = dynamic_cast<const ResolvedBinaryOperator *>(&expr)) {
        return insert_expr(*binop->rhs, block), insert_expr(*binop->lhs, block);
    }
    if (const auto *unop = dynamic_cast<const ResolvedUnaryOperator *>(&expr)) {
        return insert_expr(*unop->operand, block);
    }
    if (const auto *refPtrExpr = dynamic_cast<const ResolvedRefPtrExpr *>(&expr)) {
        return insert_expr(*refPtrExpr->expr, block);
    }
    if (const auto *refPtrExpr = dynamic_cast<const ResolvedDerefPtrExpr *>(&expr)) {
        return insert_expr(*refPtrExpr->expr, block);
    }
    if (const auto *structInst = dynamic_cast<const ResolvedStructInstantiationExpr *>(&expr)) {
        for (auto it = structInst->fieldInitializers.rbegin(); it != structInst->fieldInitializers.rend(); ++it)
            insert_stmt(**it, block);
        return block;
    }
    if (const auto *catchErrExpr = dynamic_cast<const ResolvedCatchErrExpr *>(&expr)) {
        if (catchErrExpr->errToCatch) {
            return insert_expr(*catchErrExpr->errToCatch, block);
        } else if (catchErrExpr->declaration) {
            return insert_stmt(*catchErrExpr->declaration, block);
        } else {
            dmz_unreachable("malformed CatchErrExpr");
        }
    }
    if (const auto *tryErrExpr = dynamic_cast<const ResolvedTryErrExpr *>(&expr)) {
        if (tryErrExpr->errToTry) {
            return insert_expr(*tryErrExpr->errToTry, block);
        } else if (tryErrExpr->declaration) {
            return insert_stmt(*tryErrExpr->declaration, block);
        } else {
            dmz_unreachable("malformed TryErrExpr");
        }
    }
    if (const auto *unwrapExpr = dynamic_cast<const ResolvedErrUnwrapExpr *>(&expr)) {
        for (auto &&d : unwrapExpr->defers) {
            block = insert_block(*d->resolvedDefer.block, block);
        }
        return insert_expr(*unwrapExpr->errToUnwrap, block);
    }

    return block;
}

int CFGBuilder::insert_if_stmt(const ResolvedIfStmt &stmt, int exit) {
    int falseBlock = exit;
    if (stmt.falseBlock) {
        falseBlock = insert_block(*stmt.falseBlock, exit);
    }

    int trueBlock = insert_block(*stmt.trueBlock, exit);
    int entry = cfg.insert_new_block();

    std::optional<int> val = cee.evaluate(*stmt.condition, true);
    cfg.insert_edge(entry, trueBlock, ConstantExpressionEvaluator::to_bool(val) != false);
    cfg.insert_edge(entry, falseBlock, ConstantExpressionEvaluator::to_bool(val).value_or(false) == false);

    cfg.insert_stmt(&stmt, entry);
    return insert_expr(*stmt.condition, entry);
}

int CFGBuilder::insert_switch_stmt(const ResolvedSwitchStmt &stmt, int exit) {
    // TODO: revise
    std::optional<int> val = cee.evaluate(*stmt.condition, true);
    std::vector<int> casesBlocks(stmt.cases.size() + 1);
    for (size_t i = 0; i < stmt.cases.size(); i++) {
        casesBlocks[i] = insert_block(*stmt.cases[i]->block, exit);
    }
    casesBlocks[stmt.cases.size()] = insert_block(*stmt.elseBlock, exit);

    int entry = cfg.insert_new_block();

    size_t rechableIndex = -1;
    for (size_t i = 0; i < stmt.cases.size(); i++) {
        std::optional<int> case_val = cee.evaluate(*stmt.cases[i]->condition, true);
        if (val && case_val && val == case_val) {
            rechableIndex = i;
        }
    }

    for (size_t i = 0; i < stmt.cases.size(); i++) {
        std::optional<int> case_val = cee.evaluate(*stmt.cases[i]->condition, true);
        cfg.insert_edge(entry, casesBlocks[i], !val || !case_val || rechableIndex == i);
    }

    cfg.insert_edge(entry, casesBlocks[stmt.cases.size()], !val || rechableIndex == -1u);

    cfg.insert_stmt(&stmt, entry);
    return insert_expr(*stmt.condition, entry);
}

int CFGBuilder::insert_while_stmt(const ResolvedWhileStmt &stmt, int exit) {
    int latch = cfg.insert_new_block();
    int body = insert_block(*stmt.body, latch);

    int header = cfg.insert_new_block();
    cfg.insert_edge(latch, header, true);

    std::optional<int> val = cee.evaluate(*stmt.condition, true);
    cfg.insert_edge(header, body, ConstantExpressionEvaluator::to_bool(val) != false);
    cfg.insert_edge(header, exit, ConstantExpressionEvaluator::to_bool(val).value_or(false) == false);

    cfg.insert_stmt(&stmt, header);
    insert_expr(*stmt.condition, header);

    return header;
}

int CFGBuilder::insert_decl_stmt(const ResolvedDeclStmt &stmt, int block) {
    cfg.insert_stmt(&stmt, block);

    if (const auto &init = stmt.varDecl->initializer) {
        return insert_expr(*init, block);
    }

    return block;
}

int CFGBuilder::insert_assignment(const ResolvedAssignment &stmt, int block) {
    cfg.insert_stmt(&stmt, block);

    if (!dynamic_cast<const ResolvedDeclRefExpr *>(stmt.assignee.get())) {
        block = insert_expr(*stmt.assignee, block);
    }

    return insert_expr(*stmt.expr, block);
}

void CFG::dump() const {
    for (int i = m_basicBlocks.size() - 1; i >= 0; --i) {
        std::cerr << '[' << i;
        if (i == entry)
            std::cerr << " (entry)";
        else if (i == exit)
            std::cerr << " (exit)";
        std::cerr << ']' << '\n';

        std::cerr << "  preds: ";
        for (auto &&[id, reachable] : m_basicBlocks[i].predecessors) std::cerr << id << ((reachable) ? " " : "(U) ");
        std::cerr << '\n';

        std::cerr << "  succs: ";
        for (auto &&[id, reachable] : m_basicBlocks[i].successors) std::cerr << id << ((reachable) ? " " : "(U) ");
        std::cerr << '\n';

        const auto &statements = m_basicBlocks[i].statements;
        for (auto it = statements.rbegin(); it != statements.rend(); ++it) {
            (*it)->dump(1, true);
        }
        std::cerr << '\n';
    }
}
}  // namespace DMZ
