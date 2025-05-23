#include <map>
#include <stack>
#include <unordered_set>

#include "semantic/Semantic.hpp"

namespace DMZ {

std::unique_ptr<ResolvedStmt> Sema::resolve_stmt(const Stmt &stmt) {
    if (auto *expr = dynamic_cast<const Expr *>(&stmt)) {
        return resolve_expr(*expr);
    }
    if (auto *ifStmt = dynamic_cast<const IfStmt *>(&stmt)) {
        return resolve_if_stmt(*ifStmt);
    }
    if (auto *assignment = dynamic_cast<const Assignment *>(&stmt)) {
        return resolve_assignment(*assignment);
    }
    if (auto *declStmt = dynamic_cast<const DeclStmt *>(&stmt)) {
        return resolve_decl_stmt(*declStmt);
    }
    if (auto *whileStmt = dynamic_cast<const WhileStmt *>(&stmt)) {
        return resolve_while_stmt(*whileStmt);
    }
    if (auto *returnStmt = dynamic_cast<const ReturnStmt *>(&stmt)) {
        return resolve_return_stmt(*returnStmt);
    }
    if (auto *deferStmt = dynamic_cast<const DeferStmt *>(&stmt)) {
        return resolve_defer_stmt(*deferStmt);
    }
    if (auto *block = dynamic_cast<const Block *>(&stmt)) {
        return resolve_block(*block);
    }

    dmz_unreachable("unexpected statement");
}

std::unique_ptr<ResolvedReturnStmt> Sema::resolve_return_stmt(const ReturnStmt &returnStmt) {
    if (!m_currentFunction) {
        return report(returnStmt.location, "unexpected return stmt outside a function");
    }

    if (m_currentFunction->type.kind == Type::Kind::Void && returnStmt.expr)
        return report(returnStmt.location, "unexpected return value in void function");

    if (m_currentFunction->type.kind != Type::Kind::Void && !returnStmt.expr)
        return report(returnStmt.location, "expected a return value");

    std::unique_ptr<ResolvedExpr> resolvedExpr;
    if (returnStmt.expr) {
        resolvedExpr = resolve_expr(*returnStmt.expr);
        if (!resolvedExpr) return nullptr;

        if (!Type::compare(m_currentFunction->type, resolvedExpr->type))
            return report(resolvedExpr->location, "unexpected return type");

        resolvedExpr->set_constant_value(cee.evaluate(*resolvedExpr, false));
    }

    return std::make_unique<ResolvedReturnStmt>(returnStmt.location, std::move(resolvedExpr));
}

std::unique_ptr<ResolvedBlock> Sema::resolve_block(const Block &block) {
    std::vector<std::unique_ptr<ResolvedStmt>> resolvedStatements;

    bool error = false;
    int reportUnreachableCount = 0;

    ScopeRAII blockScope(*this);
    for (auto &&stmt : block.statements) {
        auto resolvedStmt = resolve_stmt(*stmt);
        if (dynamic_cast<ResolvedDeferStmt *>(resolvedStmt.get())) {
            m_defers.back().emplace_back(std::move(resolvedStmt));
        } else {
            error |= !resolvedStatements.emplace_back(std::move(resolvedStmt));
            if (error) continue;
        }

        if (reportUnreachableCount == 1) {
            report(stmt->location, "unreachable statement", true);
            ++reportUnreachableCount;
        }

        if (dynamic_cast<ReturnStmt *>(stmt.get())) {
            ++reportUnreachableCount;
            // Traversing in reverse the defers vector
            for (int i = m_defers.size() - 1; i >= 0; --i) {
                for (int j = m_defers[i].size() - 1; j >= 0; --j) {
                    auto deferStmt = dynamic_cast<const ResolvedDeferStmt *>(m_defers[i][j].get());
                    if (!deferStmt) dmz_unreachable("internal error in defers can only be ResolvedDeferStmt");
                    resolvedStatements.insert(resolvedStatements.end() - 1,
                                              std::make_unique<ResolvedDeferStmt>(*deferStmt));
                }
            }
        }
    }

    if (error) return nullptr;

    if (resolvedStatements.size() == 0 || !dynamic_cast<ResolvedReturnStmt *>(resolvedStatements.back().get())) {
        // Traversing in reverse the defers vector
        for (int i = m_defers.back().size() - 1; i >= 0; --i) {
            auto deferStmt = dynamic_cast<const ResolvedDeferStmt *>(m_defers.back()[i].get());
            if (!deferStmt) dmz_unreachable("internal error in defers can only be ResolvedDeferStmt");
            resolvedStatements.emplace_back(std::make_unique<ResolvedDeferStmt>(*deferStmt));
        }
    }

    return std::make_unique<ResolvedBlock>(block.location, std::move(resolvedStatements));
}

std::unique_ptr<ResolvedIfStmt> Sema::resolve_if_stmt(const IfStmt &ifStmt) {
    varOrReturn(condition, resolve_expr(*ifStmt.condition));

    if (condition->type.kind != Type::Kind::Int && condition->type.kind != Type::Kind::Bool) {
        return report(condition->location, "expected int in condition");
    }
    varOrReturn(resolvedTrueBlock, resolve_block(*ifStmt.trueBlock));

    std::unique_ptr<ResolvedBlock> resolvedFalseBlock;
    if (ifStmt.falseBlock) {
        resolvedFalseBlock = resolve_block(*ifStmt.falseBlock);
        if (!resolvedFalseBlock) return nullptr;
    }

    condition->set_constant_value(cee.evaluate(*condition, false));

    return std::make_unique<ResolvedIfStmt>(ifStmt.location, std::move(condition), std::move(resolvedTrueBlock),
                                            std::move(resolvedFalseBlock));
}

std::unique_ptr<ResolvedWhileStmt> Sema::resolve_while_stmt(const WhileStmt &whileStmt) {
    varOrReturn(condition, resolve_expr(*whileStmt.condition));

    if (condition->type.kind != Type::Kind::Int && condition->type.kind != Type::Kind::Bool) {
        return report(condition->location, "expected int in condition");
    }

    varOrReturn(body, resolve_block(*whileStmt.body));

    condition->set_constant_value(cee.evaluate(*condition, false));

    return std::make_unique<ResolvedWhileStmt>(whileStmt.location, std::move(condition), std::move(body));
}

std::unique_ptr<ResolvedDeclStmt> Sema::resolve_decl_stmt(const DeclStmt &declStmt) {
    varOrReturn(resolvedVarDecl, resolve_var_decl(*declStmt.varDecl));

    if (!insert_decl_to_current_scope(*resolvedVarDecl)) return nullptr;

    return std::make_unique<ResolvedDeclStmt>(declStmt.location, std::move(resolvedVarDecl));
}

std::unique_ptr<ResolvedAssignment> Sema::resolve_assignment(const Assignment &assignment) {
    varOrReturn(resolvedRHS, resolve_expr(*assignment.expr));
    varOrReturn(resolvedLHS, resolve_assignable_expr(*assignment.assignee));

    if (resolvedLHS->type.kind == Type::Kind::Void) {
        return report(resolvedLHS->location, "reference to void declaration in assignment LHS");
    }
    if (!Type::compare(resolvedLHS->type, resolvedRHS->type)) {
        return report(resolvedRHS->location, "assigned value type '" + resolvedRHS->type.to_str() +
                                                 "' doesn't match variable type '" + resolvedLHS->type.to_str() + "'");
    }

    resolvedRHS->set_constant_value(cee.evaluate(*resolvedRHS, false));

    return std::make_unique<ResolvedAssignment>(assignment.location, std::move(resolvedLHS), std::move(resolvedRHS));
}

std::unique_ptr<ResolvedDeferStmt> Sema::resolve_defer_stmt(const DeferStmt &deferStmt) {
    varOrReturn(block, resolve_block(*deferStmt.block));
    std::shared_ptr<ResolvedBlock> sharedBlock = std::move(block);
    return std::make_unique<ResolvedDeferStmt>(deferStmt.location, sharedBlock);
}
}  // namespace DMZ