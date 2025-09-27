// #define DEBUG
#include "DMZPCH.hpp"
#include "Utils.hpp"
#include "parser/ParserSymbols.hpp"
#include "semantic/Semantic.hpp"
#include "semantic/SemanticSymbols.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

ptr<ResolvedStmt> Sema::resolve_stmt(const Stmt &stmt) {
    debug_func(stmt.location);
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
        auto ret = resolve_decl_stmt(*declStmt);
        if (ret && !resolve_decl_stmt_initialize(*ret)) return nullptr;
        return ret;
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
    if (auto *switchStmt = dynamic_cast<const SwitchStmt *>(&stmt)) {
        return resolve_switch_stmt(*switchStmt);
    }

    stmt.dump();
    dmz_unreachable("unexpected statement");
}

ptr<ResolvedReturnStmt> Sema::resolve_return_stmt(const ReturnStmt &returnStmt) {
    debug_func(returnStmt.location);
    if (!m_currentFunction) {
        return report(returnStmt.location, "unexpected return stmt outside a function");
    }
    auto fnType = m_currentFunction->getFnType();

    if (fnType->returnType->kind == ResolvedTypeKind::Void && returnStmt.expr)
        if (fnType->returnType->kind != ResolvedTypeKind::Optional)
            return report(returnStmt.location, "unexpected return value in void function");

    if (fnType->returnType->kind != ResolvedTypeKind::Void && !returnStmt.expr)
        return report(returnStmt.location, "expected a return value");

    ptr<ResolvedExpr> resolvedExpr;
    if (returnStmt.expr) {
        resolvedExpr = resolve_expr(*returnStmt.expr);
        if (!resolvedExpr) return nullptr;

        if (!fnType->returnType->compare(*resolvedExpr->type))
            return report(resolvedExpr->location, "unexpected return type, expected '" + fnType->returnType->to_str() +
                                                      "' actual '" + resolvedExpr->type->to_str() + "'");

        resolvedExpr->set_constant_value(cee.evaluate(*resolvedExpr, false));
    }
    bool isError = resolvedExpr && resolvedExpr->type->kind == ResolvedTypeKind::Error;
    auto defers = resolve_defer_ref_stmt(false, isError);

    return makePtr<ResolvedReturnStmt>(returnStmt.location, std::move(resolvedExpr), std::move(defers));
}

ptr<ResolvedBlock> Sema::resolve_block(const Block &block) {
    debug_func(block.location);
    std::vector<ptr<ResolvedStmt>> resolvedStatements;

    bool error = false;
    int reportUnreachableCount = 0;

    ScopeRAII blockScope(*this);
    for (auto &&stmt : block.statements) {
        auto resolvedStmt = resolve_stmt(*stmt);
        error |= !resolvedStatements.emplace_back(std::move(resolvedStmt));
        if (error) continue;

        if (reportUnreachableCount == 1) {
            report(stmt->location, "unreachable statement", true);
            ++reportUnreachableCount;
        }

        if (dynamic_cast<ReturnStmt *>(stmt.get())) {
            ++reportUnreachableCount;
        }
    }

    if (error) return nullptr;

    std::vector<ptr<ResolvedDeferRefStmt>> defers;
    // Only if not finish in return, return handle that part
    if (resolvedStatements.size() == 0 || !dynamic_cast<ResolvedReturnStmt *>(resolvedStatements.back().get())) {
        defers = resolve_defer_ref_stmt(true, false);
    }

    return makePtr<ResolvedBlock>(block.location, std::move(resolvedStatements), std::move(defers));
}

ptr<ResolvedIfStmt> Sema::resolve_if_stmt(const IfStmt &ifStmt) {
    debug_func(ifStmt.location);
    varOrReturn(condition, resolve_expr(*ifStmt.condition));

    auto typeToCompare = ResolvedTypeBool{SourceLocation{}};
    if (!typeToCompare.compare(*condition->type)) {
        return report(condition->location, "unexpected type in condition '" + condition->type->to_str() + "'");
    }

    varOrReturn(resolvedTrueBlock, resolve_block(*ifStmt.trueBlock));

    ptr<ResolvedBlock> resolvedFalseBlock;
    if (ifStmt.falseBlock) {
        resolvedFalseBlock = resolve_block(*ifStmt.falseBlock);
        if (!resolvedFalseBlock) return nullptr;
    }

    condition->set_constant_value(cee.evaluate(*condition, false));

    return makePtr<ResolvedIfStmt>(ifStmt.location, std::move(condition), std::move(resolvedTrueBlock),
                                   std::move(resolvedFalseBlock));
}

ptr<ResolvedWhileStmt> Sema::resolve_while_stmt(const WhileStmt &whileStmt) {
    debug_func(whileStmt.location);
    varOrReturn(condition, resolve_expr(*whileStmt.condition));

    auto typeToCompare = ResolvedTypeBool{SourceLocation{}};
    if (!typeToCompare.compare(*condition->type)) {
        return report(condition->location, "unexpected type in condition '" + condition->type->to_str() + "'");
    }

    varOrReturn(body, resolve_block(*whileStmt.body));

    condition->set_constant_value(cee.evaluate(*condition, false));

    return makePtr<ResolvedWhileStmt>(whileStmt.location, std::move(condition), std::move(body));
}

ptr<ResolvedDeclStmt> Sema::resolve_decl_stmt(const DeclStmt &declStmt) {
    debug_func(declStmt.location);
    varOrReturn(resolvedVarDecl, resolve_var_decl(*declStmt.varDecl));

    if (!insert_decl_to_current_scope(*resolvedVarDecl)) return nullptr;

    return makePtr<ResolvedDeclStmt>(declStmt.location, nullptr, std::move(resolvedVarDecl));
}

bool Sema::resolve_decl_stmt_initialize(ResolvedDeclStmt &declStmt) {
    debug_func(declStmt.location);
    if (declStmt.initialized) return true;

    if (!resolve_var_decl_initialize(*declStmt.varDecl)) return false;
    declStmt.type = declStmt.varDecl->type->clone();
    declStmt.initialized = true;
    return true;
}

ptr<ResolvedAssignment> Sema::resolve_assignment(const Assignment &assignment) {
    debug_func(assignment.location);
    varOrReturn(resolvedRHS, resolve_expr(*assignment.expr));
    varOrReturn(resolvedLHS, resolve_assignable_expr(*assignment.assignee));

    if (auto declRef = dynamic_cast<const ResolvedDeclRefExpr *>(resolvedLHS.get())) {
        if (!declRef->decl.isMutable) {
            return report(resolvedLHS->location, "'" + declRef->decl.identifier + "' cannot be mutated");
        }
    }
    if (resolvedLHS->type->kind == ResolvedTypeKind::Void) {
        return report(resolvedLHS->location, "reference to void declaration in assignment LHS");
    }
    if (!resolvedLHS->type->compare(*resolvedRHS->type)) {
        return report(resolvedRHS->location, "assigned value type '" + resolvedRHS->type->to_str() +
                                                 "' doesn't match variable type '" + resolvedLHS->type->to_str() + "'");
    }

    resolvedRHS->set_constant_value(cee.evaluate(*resolvedRHS, false));

    if (auto assigmentOperator = dynamic_cast<const AssignmentOperator *>(&assignment)) {
        if (resolvedLHS->type->kind != ResolvedTypeKind::Number) {
            return report(resolvedLHS->location, "cannot use operator '" + get_op_str(assigmentOperator->op) +
                                                     "' in type '" + resolvedLHS->type->to_str() + "'");
        }
        varOrReturn(resolvedLHS2, resolve_assignable_expr(*assignment.assignee));
        resolvedRHS = makePtr<ResolvedBinaryOperator>(assignment.location, assigmentOperator->op,
                                                      std::move(resolvedLHS2), std::move(resolvedRHS));
    }
    return makePtr<ResolvedAssignment>(assignment.location, std::move(resolvedLHS), std::move(resolvedRHS));
}

ptr<ResolvedDeferStmt> Sema::resolve_defer_stmt(const DeferStmt &deferStmt) {
    debug_func(deferStmt.location);
    varOrReturn(block, resolve_block(*deferStmt.block));
    auto resolvedDeferStmt = makePtr<ResolvedDeferStmt>(deferStmt.location, std::move(block), deferStmt.isErrDefer);
    m_defers.back().emplace_back(resolvedDeferStmt.get());
    return resolvedDeferStmt;
}

std::vector<ptr<ResolvedDeferRefStmt>> Sema::resolve_defer_ref_stmt(bool isScope, bool isError) {
    debug_func("");
    std::vector<ptr<ResolvedDeferRefStmt>> defers;
    // Traversing in reverse the defers vector
    for (int i = m_defers.size() - 1; i >= 0; --i) {
        for (int j = m_defers[i].size() - 1; j >= 0; --j) {
            auto deferStmt = m_defers[i][j];
            if (!isError && deferStmt->isErrDefer) continue;
            defers.emplace_back(makePtr<ResolvedDeferRefStmt>(deferStmt->location, *deferStmt));
        }
        if (isScope) break;
    }
    return defers;
}

ptr<ResolvedSwitchStmt> Sema::resolve_switch_stmt(const SwitchStmt &switchStmt) {
    debug_func(switchStmt.location);
    varOrReturn(condition, resolve_expr(*switchStmt.condition));

    auto typeToCompare = ResolvedTypeBool{SourceLocation{}};
    if (!typeToCompare.compare(*condition->type)) {
        return report(condition->location, "unexpected type in condition '" + condition->type->to_str() + "'");
    }

    std::vector<ptr<ResolvedCaseStmt>> cases;

    for (auto &&cas : switchStmt.cases) {
        varOrReturn(c, resolve_case_stmt(*cas));
        cases.emplace_back(std::move(c));
    }

    varOrReturn(resolvedElseBlock, resolve_block(*switchStmt.elseBlock));

    condition->set_constant_value(cee.evaluate(*condition, false));

    return makePtr<ResolvedSwitchStmt>(switchStmt.location, std::move(condition), std::move(cases),
                                       std::move(resolvedElseBlock));
}

ptr<ResolvedCaseStmt> Sema::resolve_case_stmt(const CaseStmt &caseStmt) {
    debug_func(caseStmt.location);
    varOrReturn(condition, resolve_expr(*caseStmt.condition));

    varOrReturn(block, resolve_block(*caseStmt.block));

    condition->set_constant_value(cee.evaluate(*condition, false));
    if (!condition->get_constant_value()) {
        return report(condition->location, "condition in case must be a constant value");
    }

    return makePtr<ResolvedCaseStmt>(caseStmt.location, std::move(condition), std::move(block));
}
}  // namespace DMZ