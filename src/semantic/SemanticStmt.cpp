#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif
#include <memory>
#include <string>

#include "DMZPCH.hpp"
#include "Utils.hpp"
#include "driver/Driver.hpp"
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
    if (auto *forStmt = dynamic_cast<const ForStmt *>(&stmt)) {
        return resolve_for_stmt(*forStmt);
    }
    if (auto *breakStmt = dynamic_cast<const BreakStmt *>(&stmt)) {
        return resolve_break_stmt(*breakStmt);
    }
    if (auto *continueStmt = dynamic_cast<const ContinueStmt *>(&stmt)) {
        return resolve_continue_stmt(*continueStmt);
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

    if (fnType->returnType->kind == ResolvedTypeKind::Void && returnStmt.expr &&
        !fnType->returnType->equal(*ResolvedTypeOptional::voidOptional(fnType->returnType->location)))
        return report(returnStmt.location, "unexpected return value in void function");

    if (fnType->returnType->kind != ResolvedTypeKind::Void && !returnStmt.expr &&
        !fnType->returnType->equal(*ResolvedTypeOptional::voidOptional(fnType->returnType->location)))
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
        if (dynamic_cast<Decoration *>(stmt.get())) continue;
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

    condition->set_constant_value(cee.evaluate(*condition, false));
    if (ifStmt.isInline && !condition->get_constant_value()) {
        return report(condition->location, "inline if condition must be a constant value");
    }

    ptr<ResolvedBlock> resolvedTrueBlock;
    ptr<ResolvedBlock> resolvedFalseBlock;

    if (ifStmt.isInline) {
        bool condVal = condition->get_constant_value().value() != 0;
        if (condVal) {
            varOrReturn(trueBlock, resolve_block(*ifStmt.trueBlock));
            resolvedTrueBlock = std::move(trueBlock);
            if (ifStmt.falseBlock) {
                resolvedFalseBlock =
                    makePtr<ResolvedBlock>(ifStmt.falseBlock->location, std::vector<ptr<ResolvedStmt>>{},
                                           std::vector<ptr<ResolvedDeferRefStmt>>{});
            }
        } else {
            resolvedTrueBlock = makePtr<ResolvedBlock>(ifStmt.trueBlock->location, std::vector<ptr<ResolvedStmt>>{},
                                                       std::vector<ptr<ResolvedDeferRefStmt>>{});
            if (ifStmt.falseBlock) {
                varOrReturn(falseBlock, resolve_block(*ifStmt.falseBlock));
                resolvedFalseBlock = std::move(falseBlock);
            }
        }
    } else {
        varOrReturn(trueBlock, resolve_block(*ifStmt.trueBlock));
        resolvedTrueBlock = std::move(trueBlock);

        if (ifStmt.falseBlock) {
            resolvedFalseBlock = resolve_block(*ifStmt.falseBlock);
            if (!resolvedFalseBlock) return nullptr;
        }
    }

    return makePtr<ResolvedIfStmt>(ifStmt.location, std::move(condition), std::move(resolvedTrueBlock),
                                   std::move(resolvedFalseBlock), ifStmt.isInline);
}

ptr<ResolvedWhileStmt> Sema::resolve_while_stmt(const WhileStmt &whileStmt) {
    debug_func(whileStmt.location);
    varOrReturn(condition, resolve_expr(*whileStmt.condition));

    auto typeToCompare = ResolvedTypeBool{SourceLocation{}};
    if (!typeToCompare.compare(*condition->type)) {
        return report(condition->location, "unexpected type in condition '" + condition->type->to_str() + "'");
    }

    m_loopDepth++;
    varOrReturn(body, resolve_block(*whileStmt.body));
    m_loopDepth--;

    condition->set_constant_value(cee.evaluate(*condition, false));

    return makePtr<ResolvedWhileStmt>(whileStmt.location, std::move(condition), std::move(body));
}

ptr<ResolvedBreakStmt> Sema::resolve_break_stmt(const BreakStmt &breakStmt) {
    debug_func(breakStmt.location);

    ptr<ResolvedExpr> resolvedExpr = nullptr;
    ResolvedCatchErrorExpr *targetCatch = nullptr;

    if (breakStmt.expr) {
        if (m_catchStack.empty()) {
            return report(breakStmt.location, "unexpected break with value outside a catch block");
        }
        resolvedExpr = resolve_expr(*breakStmt.expr);
        if (!resolvedExpr) return nullptr;

        targetCatch = m_catchStack.back();
        if (!targetCatch->type->compare(*resolvedExpr->type)) {
            return report(breakStmt.location, "unexpected break value type, expected '" + targetCatch->type->to_str() +
                                                  "' actual '" + resolvedExpr->type->to_str() + "'");
        }
    } else {
        if (m_loopDepth <= 0) {
            return report(breakStmt.location, "unexpected break statement outside a loop");
        }
    }

    auto defers = resolve_defer_ref_stmt(true, false);
    return makePtr<ResolvedBreakStmt>(breakStmt.location, std::move(defers), std::move(resolvedExpr), targetCatch);
}

ptr<ResolvedContinueStmt> Sema::resolve_continue_stmt(const ContinueStmt &continueStmt) {
    debug_func(continueStmt.location);
    if (m_loopDepth <= 0) {
        return report(continueStmt.location, "unexpected continue statement outside a loop");
    }
    auto defers = resolve_defer_ref_stmt(true, false);
    return makePtr<ResolvedContinueStmt>(continueStmt.location, std::move(defers));
}

ptr<ResolvedStmt> Sema::resolve_for_stmt(const ForStmt &forStmt) {
    debug_func(forStmt.location);

    if (forStmt.isInline) {
        if (forStmt.conditions.size() != 1 || forStmt.captures.size() != 1)
            return report(forStmt.location, "inline for expects exactly 1 condition and 1 capture");

        varOrReturn(condTypeCheck, resolve_expr(*forStmt.conditions[0]));
        auto structType = dynamic_cast<const ResolvedTypeStruct *>(condTypeCheck->type.get());
        if (!structType) {
            if (condTypeCheck->type->kind == ResolvedTypeKind::Generic) {
                ScopeRAII iterationScope(*this);
                auto captureType = condTypeCheck->type->clone();
                auto resolvedCapture = makePtr<ResolvedCaptureDecl>(
                    forStmt.captures[0]->location, forStmt.captures[0]->identifier, std::move(captureType));
                if (!insert_decl_to_current_scope(*resolvedCapture)) return nullptr;

                varOrReturn(resolvedBody, resolve_block(*forStmt.body));

                std::vector<ptr<ResolvedExpr>> conditions;
                conditions.emplace_back(std::move(condTypeCheck));
                std::vector<ptr<ResolvedCaptureDecl>> captures;
                captures.emplace_back(std::move(resolvedCapture));

                return makePtr<ResolvedForStmt>(forStmt.location, std::move(conditions), std::move(captures),
                                                std::move(resolvedBody), true);
            }
            return report(condTypeCheck->location,
                          "inline for requires a tuple or struct iteration, but got: " + condTypeCheck->type->to_str());
        }

        auto stDecl = structType->decl;
        std::vector<ptr<ResolvedStmt>> unrolledBody;

        for (size_t i = 0; i < stDecl->fields.size(); i++) {
            ScopeRAII iterationScope(*this);
            varOrReturn(iterCond, resolve_expr(*forStmt.conditions[0]));

            auto fieldType = stDecl->fields[i]->type->clone();
            auto fieldAccess = makePtr<ResolvedMemberExpr>(iterCond->location, std::move(iterCond), *stDecl->fields[i]);

            auto varDecl =
                makePtr<ResolvedVarDecl>(forStmt.captures[0]->location, nullptr, false, forStmt.captures[0]->identifier,
                                         fieldType->clone(), false, std::move(fieldAccess));
            if (!insert_decl_to_current_scope(*varDecl)) return nullptr;

            auto resolvedDeclStmt = makePtr<ResolvedDeclStmt>(forStmt.captures[0]->location, fieldType->clone(),
                                                              std::move(varDecl), m_currentModule, m_currentStruct);
            resolvedDeclStmt->initialized = true;

            varOrReturn(resolvedIteration, resolve_block(*forStmt.body));
            resolvedIteration->statements.insert(resolvedIteration->statements.begin(), std::move(resolvedDeclStmt));

            unrolledBody.emplace_back(std::move(resolvedIteration));
        }

        auto resolvedBody =
            makePtr<ResolvedBlock>(forStmt.location, std::move(unrolledBody), std::vector<ptr<ResolvedDeferRefStmt>>{});
        auto captureType = makePtr<ResolvedTypeGeneric>(forStmt.captures[0]->location, nullptr);
        auto resolvedCapture = makePtr<ResolvedCaptureDecl>(forStmt.captures[0]->location,
                                                            forStmt.captures[0]->identifier, std::move(captureType));

        std::vector<ptr<ResolvedExpr>> conditions;
        conditions.emplace_back(std::move(condTypeCheck));
        std::vector<ptr<ResolvedCaptureDecl>> captures;
        captures.emplace_back(std::move(resolvedCapture));

        return makePtr<ResolvedForStmt>(forStmt.location, std::move(conditions), std::move(captures),
                                        std::move(resolvedBody), true);
    }

    ScopeRAII forCapturesScope(*this);
    std::vector<ptr<ResolvedExpr>> conditions;
    std::vector<ptr<ResolvedCaptureDecl>> captures;

    if (forStmt.conditions.size() != forStmt.captures.size()) {
        return report(forStmt.location, "different number of conditions '" + std::to_string(forStmt.conditions.size()) +
                                            "' and captures '" + std::to_string(forStmt.captures.size()) + "' in for");
    }

    bool error = false;
    int size_of_forloop = -1;
    for (size_t i = 0; i < forStmt.conditions.size(); i++) {
        ptr<ResolvedType> captureType = nullptr;
        varOrReturn(resolvedCond, resolve_expr(*forStmt.conditions[i]));
        if (auto rangeExpr = dynamic_cast<ResolvedRangeExpr *>(resolvedCond.get())) {
            auto startValue = rangeExpr->startExpr->get_constant_value();
            auto endValue = rangeExpr->endExpr->get_constant_value();
            if (startValue && endValue) {
                auto currentSize = endValue.value() - startValue.value();
                if (size_of_forloop != -1) {
                    if (size_of_forloop != currentSize) {
                        error = true;
                        report(rangeExpr->location, "range length '" + std::to_string(currentSize) +
                                                        "' not match with the others '" +
                                                        std::to_string(size_of_forloop) + "'");
                        continue;
                    }
                }
                size_of_forloop = currentSize;
            }
            captureType = makePtr<ResolvedTypeNumber>(forStmt.captures[i]->location, ResolvedNumberKind::Int,
                                                      Driver::instance().ptrBitSize());
        } else if (auto sliceExpr = dynamic_cast<ResolvedTypeSlice *>(resolvedCond->type.get())) {
            captureType = makePtr<ResolvedTypePointer>(forStmt.captures[i]->location, sliceExpr->sliceType->clone());
        } else {
            return report(resolvedCond->location,
                          "not supported type of condition '" + resolvedCond->type->to_str() + "'");
        }

        auto resolvedCapture = makePtr<ResolvedCaptureDecl>(forStmt.captures[i]->location,
                                                            forStmt.captures[i]->identifier, std::move(captureType));

        if (!insert_decl_to_current_scope(*resolvedCapture)) {
            error = true;
            continue;
        }
        conditions.emplace_back(std::move(resolvedCond));
        captures.emplace_back(std::move(resolvedCapture));
    }

    if (error) return nullptr;

    m_loopDepth++;
    varOrReturn(resolvedBody, resolve_block(*forStmt.body));
    m_loopDepth--;

    return makePtr<ResolvedForStmt>(forStmt.location, std::move(conditions), std::move(captures),
                                    std::move(resolvedBody));
}

ptr<ResolvedDeclStmt> Sema::resolve_decl_stmt(const DeclStmt &declStmt) {
    debug_func(declStmt.location);
    varOrReturn(resolvedVarDecl, resolve_var_decl(*declStmt.varDecl));

    if (!insert_decl_to_current_scope(*resolvedVarDecl)) return nullptr;

    return makePtr<ResolvedDeclStmt>(declStmt.location, nullptr, std::move(resolvedVarDecl), m_currentModule,
                                     m_currentStruct);
}

bool Sema::resolve_decl_stmt_initialize(ResolvedDeclStmt &declStmt) {
    debug_func(declStmt.location);
    if (declStmt.initialized) return true;

    auto prevModule = m_currentModule;
    defer([&]() { m_currentModule = prevModule; });
    m_currentModule = declStmt.saveCurrentModule;
    auto prevStruct = m_currentStruct;
    defer([&]() { m_currentStruct = prevStruct; });
    m_currentStruct = declStmt.saveCurrentStruct;

    if (!resolve_var_decl_initialize(*declStmt.varDecl)) {
        remove_decl_to_current_scope(*declStmt.varDecl);
        return false;
    }
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
        if (resolvedLHS->type->kind != ResolvedTypeKind::Number &&
            resolvedLHS->type->kind != ResolvedTypeKind::Generic) {
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

    condition->set_constant_value(cee.evaluate(*condition, false));
    if (switchStmt.isInline && !condition->get_constant_value()) {
        return report(condition->location, "inline switch condition must be a constant value");
    }

    bool caseMatched = false;
    std::vector<ptr<ResolvedCaseStmt>> cases;
    for (auto &&cas : switchStmt.cases) {
        std::vector<ptr<ResolvedExpr>> resolvedConditions;
        bool caseMatchedInInline = false;
        for (auto &&cond : cas->conditions) {
            varOrReturn(tempCond, resolve_expr(*cond));
            tempCond->set_constant_value(cee.evaluate(*tempCond, false));
            if (!tempCond->get_constant_value()) {
                return report(tempCond->location, "condition in case must be a constant value");
            }
            if (switchStmt.isInline && tempCond->get_constant_value() == condition->get_constant_value()) {
                caseMatchedInInline = true;
                caseMatched = true;
            }
            resolvedConditions.emplace_back(std::move(tempCond));
        }

        if (switchStmt.isInline) {
            if (caseMatchedInInline) {
                varOrReturn(block, resolve_block(*cas->block));
                cases.emplace_back(makePtr<ResolvedCaseStmt>(cas->location, std::move(resolvedConditions), std::move(block)));
            } else {
                auto emptyBlock = makePtr<ResolvedBlock>(cas->location, std::vector<ptr<ResolvedStmt>>{},
                                                         std::vector<ptr<ResolvedDeferRefStmt>>{});
                cases.emplace_back(
                    makePtr<ResolvedCaseStmt>(cas->location, std::move(resolvedConditions), std::move(emptyBlock)));
            }
        } else {
            varOrReturn(block, resolve_block(*cas->block));
            cases.emplace_back(makePtr<ResolvedCaseStmt>(cas->location, std::move(resolvedConditions), std::move(block)));
        }
    }

    ptr<ResolvedBlock> resolvedElseBlock = nullptr;
    if (switchStmt.isInline && caseMatched) {
        // Ignore else
        resolvedElseBlock = makePtr<ResolvedBlock>(switchStmt.elseBlock->location, std::vector<ptr<ResolvedStmt>>{},
                                                   std::vector<ptr<ResolvedDeferRefStmt>>{});
    } else {
        resolvedElseBlock = resolve_block(*switchStmt.elseBlock);
        if (!resolvedElseBlock) return nullptr;
    }

    return makePtr<ResolvedSwitchStmt>(switchStmt.location, std::move(condition), std::move(cases),
                                       std::move(resolvedElseBlock), switchStmt.isInline);
}

ptr<ResolvedCaseStmt> Sema::resolve_case_stmt(const CaseStmt &caseStmt, std::optional<int> constant_value,
                                              bool isInline) {
    debug_func(caseStmt.location);
    std::vector<ptr<ResolvedExpr>> resolvedConditions;
    bool caseMatchedInInline = false;
    for (auto &&cond : caseStmt.conditions) {
        varOrReturn(resolvedCond, resolve_expr(*cond));
        resolvedCond->set_constant_value(cee.evaluate(*resolvedCond, false));
        if (!resolvedCond->get_constant_value()) {
            return report(resolvedCond->location, "condition in case must be a constant value");
        }
        if (isInline && resolvedCond->get_constant_value().value() == constant_value.value()) {
            caseMatchedInInline = true;
        }
        resolvedConditions.emplace_back(std::move(resolvedCond));
    }

    auto block = resolve_block(*caseStmt.block);
    if (!block) {
        if (isInline && !caseMatchedInInline) {
            block = makePtr<ResolvedBlock>(caseStmt.location, std::vector<ptr<ResolvedStmt>>{},
                                           std::vector<ptr<ResolvedDeferRefStmt>>{});
        } else {
            return nullptr;
        }
    }

    return makePtr<ResolvedCaseStmt>(caseStmt.location, std::move(resolvedConditions), std::move(block));
}
}  // namespace DMZ