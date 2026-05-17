#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif

#include "Debug.hpp"
#include "Utils.hpp"
#include "codegen/CodegenUtils.hpp"
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
    dmz_unreachable(stmt.location, "unexpected statement");
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

        if (!perform_implicit_cast(resolvedExpr, *fnType->returnType)) return nullptr;
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

    bool error = false;
    int reportUnreachableCount = 0;

    ScopeRAII blockScope(*this);
    auto ret = makePtr<ResolvedBlock>(block.location, std::vector<ptr<ResolvedStmt>>{},
                                      std::vector<ptr<ResolvedDeferRefStmt>>{}, blockScope.takeScope());

    for (auto &&stmt : block.statements) {
        if (dynamic_cast<Decoration *>(stmt.get())) continue;
        auto resolvedStmt = resolve_stmt(*stmt);
        error |= !ret->statements.emplace_back(std::move(resolvedStmt));
        if (error) continue;

        if (reportUnreachableCount == 1) {
            report(stmt->location, "unreachable statement", ReportLevel::Warning);
            ++reportUnreachableCount;
        }

        if (dynamic_cast<ReturnStmt *>(stmt.get())) {
            ++reportUnreachableCount;
        }
    }

    if (error) return nullptr;

    // Only if not finish in return, return handle that part
    if (ret->statements.size() == 0 || !dynamic_cast<ResolvedReturnStmt *>(ret->statements.back().get())) {
        ret->defers = resolve_defer_ref_stmt(true, false);
    }

    return ret;
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
        if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
            return report(condition->location, "inline if condition must be a constant value");
        }
    }

    ptr<ResolvedBlock> resolvedTrueBlock;
    ptr<ResolvedBlock> resolvedFalseBlock;

    if (ifStmt.isInline) {
        bool condVal = ConstantExpressionEvaluator::to_bool(condition->get_constant_value()).value_or(false);
        if (condVal) {
            varOrReturn(trueBlock, resolve_block(*ifStmt.trueBlock));
            resolvedTrueBlock = std::move(trueBlock);
            if (ifStmt.falseBlock) {
                resolvedFalseBlock = makePtr<ResolvedBlock>(
                    ifStmt.falseBlock->location, std::vector<ptr<ResolvedStmt>>{},
                    std::vector<ptr<ResolvedDeferRefStmt>>{}, makePtr<ResolvedScope>(m_currentScope));
            }
        } else {
            resolvedTrueBlock = makePtr<ResolvedBlock>(ifStmt.trueBlock->location, std::vector<ptr<ResolvedStmt>>{},
                                                       std::vector<ptr<ResolvedDeferRefStmt>>{},
                                                       makePtr<ResolvedScope>(m_currentScope));
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

ptr<ResolvedType> Sema::determine_for_range_capture_type(const ResolvedRangeExpr &rangeExpr,
                                                         const SourceLocation &location) {
    auto startValue = rangeExpr.startExpr->get_constant_value();
    auto endValue = rangeExpr.endExpr->get_constant_value();
    const auto &startType = rangeExpr.startExpr->type;
    const auto &endType = rangeExpr.endExpr->type;

    if (startType->equal(*endType)) {
        return startType->clone();
    }

    bool startCanBeNegative = true;
    if (startValue) {
        startCanBeNegative = startValue->getInt() < 0;
    } else if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(startType.get())) {
        if (numType->numberKind == ResolvedNumberKind::UInt) {
            startCanBeNegative = false;
        }
    }

    bool endCanBeNegative = true;
    if (endValue) {
        endCanBeNegative = endValue->getInt() < 0;
    } else if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(endType.get())) {
        if (numType->numberKind == ResolvedNumberKind::UInt) {
            endCanBeNegative = false;
        }
    }

    if (!startCanBeNegative && !endCanBeNegative) {
        return ResolvedTypeNumber::usize(location);
    }

    return ResolvedTypeNumber::isize(location);
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
        if (!perform_implicit_cast(resolvedExpr, *targetCatch->type)) return nullptr;
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
        ScopeRAII forScope(*this);
        auto takenForScope = forScope.takeScope();
        varOrReturn(condTypeCheck, resolve_expr(*forStmt.conditions[0]));
        auto structType = dynamic_cast<const ResolvedTypeStruct *>(condTypeCheck->type.get());

        if (auto rangeExpr = dynamic_cast<ResolvedRangeExpr *>(condTypeCheck.get())) {
            rangeExpr->startExpr->set_constant_value(cee.evaluate(*rangeExpr->startExpr, false));
            auto startValue = rangeExpr->startExpr->get_constant_value();
            if (!startValue) {
                return report(rangeExpr->startExpr->location, "inline for range start must be compile time known");
            }
            rangeExpr->endExpr->set_constant_value(cee.evaluate(*rangeExpr->endExpr, false));
            auto endValue = rangeExpr->endExpr->get_constant_value();
            if (!endValue) {
                return report(rangeExpr->endExpr->location, "inline for range end must be compile time known");
            }

            std::vector<ptr<ResolvedStmt>> unrolledBody;
            ScopeRAII forIterationScope(*this);
            auto takenForIterationScope = forIterationScope.takeScope();

            for (long long i = startValue->getInt(); i < endValue->getInt(); i++) {
                ScopeRAII iterationScope(*this);
                auto takenIterationScope = iterationScope.takeScope();

                auto captureType = determine_for_range_capture_type(*rangeExpr, forStmt.captures[0]->location);
                auto captureTypeExpr = makePtr<ResolvedTypeExpr>(forStmt.captures[0]->location, captureType->clone());
                auto initializer = makePtr<ResolvedIntLiteral>(forStmt.captures[0]->location, (int)i);
                initializer->type = captureType->clone();
                initializer->set_constant_value(ComptimeValue((int64_t)i));

                auto varDecl = makePtr<ResolvedVarDecl>(forStmt.captures[0]->location, nullptr, false,
                                                        forStmt.captures[0]->identifier, std::move(captureTypeExpr),
                                                        false, std::move(takenIterationScope), std::move(initializer));
                if (!insert_decl_to_current_scope(*varDecl)) return nullptr;

                varDecl->state = ResolvedState::FullyResolved;
                auto resolvedDeclStmt =
                    makePtr<ResolvedDeclStmt>(forStmt.captures[0]->location, captureType->clone(), std::move(varDecl));
                resolvedDeclStmt->initialized = true;
                resolvedDeclStmt->state = ResolvedState::FullyResolved;

                varOrReturn(resolvedIteration, resolve_block(*forStmt.body));
                resolvedIteration->statements.insert(resolvedIteration->statements.begin(),
                                                     std::move(resolvedDeclStmt));
                unrolledBody.emplace_back(std::move(resolvedIteration));
            }

            auto resolvedBody =
                makePtr<ResolvedBlock>(forStmt.location, std::move(unrolledBody),
                                       std::vector<ptr<ResolvedDeferRefStmt>>{}, std::move(takenForIterationScope));
            auto captureType = makePtr<ResolvedTypeAnyType>(forStmt.captures[0]->location);
            auto resolvedCapture = makePtr<ResolvedCaptureDecl>(
                forStmt.captures[0]->location, forStmt.captures[0]->identifier, std::move(captureType));

            std::vector<ptr<ResolvedExpr>> conditions;
            conditions.emplace_back(std::move(condTypeCheck));
            std::vector<ptr<ResolvedCaptureDecl>> captures;
            captures.emplace_back(std::move(resolvedCapture));

            return makePtr<ResolvedForStmt>(forStmt.location, std::move(conditions), std::move(captures),
                                            std::move(resolvedBody), std::move(takenForScope), true);
        }

        if (!structType) {
            if (condTypeCheck->type->kind == ResolvedTypeKind::AnyType) {
                ScopeRAII iterationScope(*this);
                auto takenIterationScope = iterationScope.takeScope();
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
                                                std::move(resolvedBody), std::move(takenIterationScope), true);
            }
            return report(condTypeCheck->location,
                          "inline for requires a tuple or struct iteration, but got: " + condTypeCheck->type->to_str());
        }

        auto stDecl = structType->decl;
        std::vector<ptr<ResolvedStmt>> unrolledBody;
        ScopeRAII forIterationScope(*this);
        auto takenForIterationScope = forIterationScope.takeScope();

        for (size_t i = 0; i < stDecl->fields.size(); i++) {
            ScopeRAII iterationScope(*this);
            auto takenIterationScope = iterationScope.takeScope();
            varOrReturn(iterCond, resolve_expr(*forStmt.conditions[0]));

            auto fieldType = stDecl->fields[i]->type->clone();
            auto fieldAccess = makePtr<ResolvedMemberExpr>(iterCond->location, std::move(iterCond), *stDecl->fields[i]);
            auto fieldTypeExpr = makePtr<ResolvedTypeExpr>(forStmt.captures[0]->location, fieldType->clone());

            auto varDecl = makePtr<ResolvedVarDecl>(forStmt.captures[0]->location, nullptr, false,
                                                    forStmt.captures[0]->identifier, std::move(fieldTypeExpr), false,
                                                    std::move(takenIterationScope), std::move(fieldAccess));
            if (!insert_decl_to_current_scope(*varDecl)) return nullptr;

            varDecl->state = ResolvedState::FullyResolved;
            auto resolvedDeclStmt =
                makePtr<ResolvedDeclStmt>(forStmt.captures[0]->location, fieldType->clone(), std::move(varDecl));
            resolvedDeclStmt->initialized = true;
            resolvedDeclStmt->state = ResolvedState::FullyResolved;
            varOrReturn(resolvedIteration, resolve_block(*forStmt.body));
            resolvedIteration->statements.insert(resolvedIteration->statements.begin(), std::move(resolvedDeclStmt));
            unrolledBody.emplace_back(std::move(resolvedIteration));
        }

        auto resolvedBody =
            makePtr<ResolvedBlock>(forStmt.location, std::move(unrolledBody), std::vector<ptr<ResolvedDeferRefStmt>>{},
                                   std::move(takenForIterationScope));
        auto captureType = makePtr<ResolvedTypeAnyType>(forStmt.captures[0]->location);
        auto resolvedCapture = makePtr<ResolvedCaptureDecl>(forStmt.captures[0]->location,
                                                            forStmt.captures[0]->identifier, std::move(captureType));

        std::vector<ptr<ResolvedExpr>> conditions;
        conditions.emplace_back(std::move(condTypeCheck));
        std::vector<ptr<ResolvedCaptureDecl>> captures;
        captures.emplace_back(std::move(resolvedCapture));

        return makePtr<ResolvedForStmt>(forStmt.location, std::move(conditions), std::move(captures),
                                        std::move(resolvedBody), std::move(takenForScope), true);
    }

    ScopeRAII forCapturesScope(*this);
    auto forCapturesScopeTaken = forCapturesScope.takeScope();
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
                auto currentSize = endValue->getInt() - startValue->getInt();
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
            captureType = determine_for_range_capture_type(*rangeExpr, forStmt.captures[i]->location);
        } else if (auto sliceExpr = dynamic_cast<ResolvedTypeSlice *>(resolvedCond->type.get())) {
            captureType = makePtr<ResolvedTypePointer>(forStmt.captures[i]->location, sliceExpr->sliceType->clone());
        } else if (resolvedCond->type->kind == ResolvedTypeKind::AnyType) {
            captureType = resolvedCond->type->clone();
        } else {
            return report(resolvedCond->location,
                          "not supported type of condition '" + resolvedCond->type->to_str() + "'");
        }

        auto resolvedCapture = makePtr<ResolvedCaptureDecl>(forStmt.captures[i]->location,
                                                            forStmt.captures[i]->identifier, std::move(captureType));
        resolvedCapture->state = ResolvedState::FullyResolved;

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

    auto ret = makePtr<ResolvedForStmt>(forStmt.location, std::move(conditions), std::move(captures),
                                        std::move(resolvedBody), std::move(forCapturesScopeTaken));
    return ret;
}

ptr<ResolvedDeclStmt> Sema::resolve_decl_stmt(const DeclStmt &declStmt) {
    debug_func(declStmt.location);
    varOrReturn(resolvedVarDecl, resolve_var_decl(*declStmt.varDecl));

    if (!insert_decl_to_current_scope(*resolvedVarDecl)) return nullptr;

    auto ret = makePtr<ResolvedDeclStmt>(declStmt.location, nullptr, std::move(resolvedVarDecl));
    ret->varDecl->parentDeclStmt = ret.get();
    ret->symbolName = resolve_decl_name(ret->identifier);
    ret->varDecl->symbolName = resolve_decl_name(ret->varDecl->identifier);
    return ret;
}

bool Sema::resolve_decl_stmt_initialize(ResolvedDeclStmt &declStmt) {
    debug_func(declStmt.location);
    if (declStmt.state == ResolvedState::FullyResolved) return debug_ret(true);
    if (declStmt.state == ResolvedState::Error) return debug_ret(false);
    if (declStmt.state == ResolvedState::InProgress && declStmt.initialized) return debug_ret(true);  // Cycle detected

    declStmt.state = ResolvedState::InProgress;

    if (!resolve_var_decl_initialize(*declStmt.varDecl)) {
        remove_decl_to_current_scope(*declStmt.varDecl);
        declStmt.state = ResolvedState::Error;
        return debug_ret(false);
    }
    declStmt.set_constant_value(declStmt.varDecl->get_constant_value());
    declStmt.type = declStmt.varDecl->type->clone();
    declStmt.state = ResolvedState::FullyResolved;
    declStmt.initialized = true;
    return debug_ret(true);
}

ptr<ResolvedAssignment> Sema::resolve_assignment(const Assignment &assignment) {
    debug_func(assignment.location);
    varOrReturn(resolvedRHS, resolve_expr(*assignment.expr));
    debug_msg("resolvedRHS: " << resolvedRHS->className() << " " << resolvedRHS->type->className() << " '"
                              << resolvedRHS->type->to_str() << "'");
    varOrReturn(resolvedLHS, resolve_assignable_expr(*assignment.assignee, false));
    debug_msg("resolvedLHS: " << resolvedLHS->className() << " " << resolvedLHS->type->className() << " '"
                              << resolvedLHS->type->to_str() << "'");

    if (auto declRef = dynamic_cast<const ResolvedDeclRefExpr *>(resolvedLHS.get())) {
        if (!declRef->decl.isMutable) {
            return report(resolvedLHS->location, "'" + declRef->decl.identifier + "' cannot be mutated");
        }
    }
    if (resolvedLHS->type->kind == ResolvedTypeKind::Void) {
        return report(resolvedLHS->location, "reference to void declaration in assignment LHS");
    }
    if (!perform_implicit_cast(resolvedRHS, *resolvedLHS->type)) return nullptr;
    if (!resolvedLHS->type->compare(*resolvedRHS->type)) {
        debug_msg("assigned value type " << resolvedRHS->type->className()
                                         << " '" + resolvedRHS->type->to_str() + "' doesn't match variable type "
                                         << resolvedLHS->type->className()
                                         << " '" + resolvedLHS->type->to_str() + "'\n");
        return report(resolvedRHS->location, "assigned value type '" + resolvedRHS->type->to_str() +
                                                 "' doesn't match variable type '" + resolvedLHS->type->to_str() + "'");
    }

    resolvedRHS->set_constant_value(cee.evaluate(*resolvedRHS, false));

    if (auto assigmentOperator = dynamic_cast<const AssignmentOperator *>(&assignment)) {
        if (resolvedLHS->type->kind != ResolvedTypeKind::Number &&
            resolvedLHS->type->kind != ResolvedTypeKind::AnyType) {
            return report(resolvedLHS->location, "cannot use operator '" + get_op_str(assigmentOperator->op) +
                                                     "' in type '" + resolvedLHS->type->to_str() + "'");
        }
        varOrReturn(resolvedLHS2, resolve_assignable_expr(*assignment.assignee, false));
        resolvedRHS = makePtr<ResolvedBinaryOperator>(assignment.location, assigmentOperator->op,
                                                      std::move(resolvedLHS2), std::move(resolvedRHS));
    }
    return makePtr<ResolvedAssignment>(assignment.location, std::move(resolvedLHS), std::move(resolvedRHS));
}

ptr<ResolvedDeferStmt> Sema::resolve_defer_stmt(const DeferStmt &deferStmt) {
    debug_func(deferStmt.location);
    varOrReturn(block, resolve_block(*deferStmt.block));
    auto resolvedDeferStmt = makePtr<ResolvedDeferStmt>(deferStmt.location, std::move(block), deferStmt.isErrDefer);
    m_currentScope->defers.emplace_back(resolvedDeferStmt.get());
    return resolvedDeferStmt;
}

std::vector<ptr<ResolvedDeferRefStmt>> Sema::resolve_defer_ref_stmt(bool isScope, bool isError) {
    debug_func("");
    std::vector<ptr<ResolvedDeferRefStmt>> defers;
    // Traversing the scope tree to collect defers
    for (auto scope = m_currentScope; scope; scope = scope->parent) {
        for (auto it = scope->defers.rbegin(); it != scope->defers.rend(); ++it) {
            auto deferStmt = *it;
            if (!isError && deferStmt->isErrDefer) continue;
            defers.emplace_back(makePtr<ResolvedDeferRefStmt>(deferStmt->location, *deferStmt));
        }
        if (isScope) break;
        if (scope->parent && scope->parent->currentFunction != scope->currentFunction) break;
    }
    return defers;
}

ptr<ResolvedSwitchStmt> Sema::resolve_switch_stmt(const SwitchStmt &switchStmt) {
    debug_func(switchStmt.location);
    varOrReturn(condition, resolve_expr(*switchStmt.condition));

    condition->set_constant_value(cee.evaluate(*condition, false));
    if (switchStmt.isInline && !condition->get_constant_value()) {
        if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
            return report(condition->location, "inline switch condition must be a constant value");
        }
    }

    bool caseMatched = false;
    std::vector<ptr<ResolvedCaseStmt>> cases;
    for (auto &&cas : switchStmt.cases) {
        std::vector<ptr<ResolvedExpr>> resolvedConditions;
        bool caseMatchedInInline = false;
        for (auto &&cond : cas->conditions) {
            varOrReturn(tempCond, resolve_expr(*cond));
            if (!perform_implicit_cast(tempCond, *condition->type)) return nullptr;
            if (!condition->type->equal(*tempCond->type)) {
                return report(tempCond->location, "condition in case type '" + tempCond->type->to_str() +
                                                      "' doesn't match switch condition type '" +
                                                      condition->type->to_str() + "'");
            }
            tempCond->set_constant_value(cee.evaluate(*tempCond, false));
            if (!tempCond->get_constant_value()) {
                if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
                    return report(tempCond->location, "condition in case must be a constant value");
                }
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
                cases.emplace_back(
                    makePtr<ResolvedCaseStmt>(cas->location, std::move(resolvedConditions), std::move(block)));
            } else {
                auto emptyBlock = makePtr<ResolvedBlock>(cas->location, std::vector<ptr<ResolvedStmt>>{},
                                                         std::vector<ptr<ResolvedDeferRefStmt>>{},
                                                         makePtr<ResolvedScope>(m_currentScope));
                cases.emplace_back(
                    makePtr<ResolvedCaseStmt>(cas->location, std::move(resolvedConditions), std::move(emptyBlock)));
            }
        } else {
            varOrReturn(block, resolve_block(*cas->block));
            cases.emplace_back(
                makePtr<ResolvedCaseStmt>(cas->location, std::move(resolvedConditions), std::move(block)));
        }
    }

    ptr<ResolvedBlock> resolvedElseBlock = nullptr;
    if (switchStmt.isInline && caseMatched) {
        // Ignore else
        resolvedElseBlock =
            makePtr<ResolvedBlock>(switchStmt.elseBlock->location, std::vector<ptr<ResolvedStmt>>{},
                                   std::vector<ptr<ResolvedDeferRefStmt>>{}, makePtr<ResolvedScope>(m_currentScope));
    } else {
        resolvedElseBlock = resolve_block(*switchStmt.elseBlock);
        if (!resolvedElseBlock) return nullptr;
    }

    return makePtr<ResolvedSwitchStmt>(switchStmt.location, std::move(condition), std::move(cases),
                                       std::move(resolvedElseBlock), switchStmt.isInline);
}

ptr<ResolvedCaseStmt> Sema::resolve_case_stmt(const CaseStmt &caseStmt, std::optional<ComptimeValue> constant_value,
                                              bool isInline) {
    debug_func(caseStmt.location);
    std::vector<ptr<ResolvedExpr>> resolvedConditions;
    bool caseMatchedInInline = false;
    for (auto &&cond : caseStmt.conditions) {
        varOrReturn(resolvedCond, resolve_expr(*cond));
        resolvedCond->set_constant_value(cee.evaluate(*resolvedCond, false));
        if (!resolvedCond->get_constant_value()) {
            if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
                return report(resolvedCond->location, "condition in case must be a constant value");
            }
        }
        if (isInline && resolvedCond->get_constant_value() == constant_value) {
            caseMatchedInInline = true;
        }
        resolvedConditions.emplace_back(std::move(resolvedCond));
    }

    auto block = resolve_block(*caseStmt.block);
    if (!block) {
        if (isInline && !caseMatchedInInline) {
            block = makePtr<ResolvedBlock>(caseStmt.location, std::vector<ptr<ResolvedStmt>>{},
                                           std::vector<ptr<ResolvedDeferRefStmt>>{},
                                           makePtr<ResolvedScope>(m_currentScope));
        } else {
            return nullptr;
        }
    }

    return makePtr<ResolvedCaseStmt>(caseStmt.location, std::move(resolvedConditions), std::move(block));
}
}  // namespace DMZ