#include "semantic/Semantic.hpp"

#include <map>
#include <stack>

namespace DMZ {

bool Sema::insert_decl_to_current_scope(ResolvedDecl &decl) {
    const auto &[foundDecl, scopeIdx] = lookup_decl<ResolvedDecl>(decl.identifier);

    if (foundDecl && scopeIdx == 0) {
        report(decl.location, "redeclaration of '" + std::string(decl.identifier) + '\'');
        return false;
    }

    m_scopes.back().emplace_back(&decl);
    return true;
}

template <typename T>
std::pair<T *, int> Sema::lookup_decl(const std::string_view id) {
    int scopeIdx = 0;
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        for (auto &&decl : *it) {
            auto *correctDecl = dynamic_cast<T *>(decl);

            if (!correctDecl) continue;

            if (decl->identifier != id) continue;

            return {correctDecl, scopeIdx};
        }

        ++scopeIdx;
    }

    return {nullptr, -1};
}

std::unique_ptr<ResolvedFunctionDecl> Sema::create_builtin_println() {
    SourceLocation loc{"<builtin>", 0, 0};

    auto param = std::make_unique<ResolvedParamDecl>(loc, "n", Type::builtinInt(), false);

    std::vector<std::unique_ptr<ResolvedParamDecl>> params;
    params.emplace_back(std::move(param));

    auto block = std::make_unique<ResolvedBlock>(loc, std::vector<std::unique_ptr<ResolvedStmt>>());

    return std::make_unique<ResolvedFunctionDecl>(loc, "println", Type::builtinVoid(), std::move(params),
                                                  std::move(block));
};

std::optional<Type> Sema::resolve_type(Type parsedType) {
    if (parsedType.kind == Type::Kind::Custom) {
        if (lookup_decl<ResolvedStructDecl>(parsedType.name).first) {
            return Type::structType(parsedType);
        }

        return std::nullopt;
    }

    return parsedType;
}

std::unique_ptr<ResolvedDeclRefExpr> Sema::resolve_decl_ref_expr(const DeclRefExpr &declRefExpr, bool isCallee) {
    ResolvedDecl *decl = lookup_decl<ResolvedDecl>(declRefExpr.identifier).first;
    if (!decl) return report(declRefExpr.location, "symbol '" + std::string(declRefExpr.identifier) + "' not found");

    if (!isCallee && dynamic_cast<ResolvedFuncDecl *>(decl))
        return report(declRefExpr.location, "expected to call function '" + std::string(declRefExpr.identifier) + "'");

    if (dynamic_cast<ResolvedStructDecl *>(decl))
        return report(declRefExpr.location, "expected an instance of '" + std::string(decl->type.name) + '\'');

    return std::make_unique<ResolvedDeclRefExpr>(declRefExpr.location, *decl);
}

std::unique_ptr<ResolvedCallExpr> Sema::resolve_call_expr(const CallExpr &call) {
    const auto *dre = dynamic_cast<const DeclRefExpr *>(call.callee.get());
    if (!dre) return report(call.location, "expression cannot be called as a function");

    varOrReturn(resolvedCallee, resolve_decl_ref_expr(*dre, true));

    const auto *resolvedFuncDecl = dynamic_cast<const ResolvedFuncDecl *>(&resolvedCallee->decl);

    if (!resolvedFuncDecl) return report(call.location, "calling non-function symbol");

    bool isVararg = (resolvedFuncDecl->params.size() != 0) ? resolvedFuncDecl->params.back()->isVararg : false;
    size_t funcDeclArgs = isVararg ? (resolvedFuncDecl->params.size() - 1) : resolvedFuncDecl->params.size();
    if (call.arguments.size() != resolvedFuncDecl->params.size()) {
        if (!isVararg || (isVararg && call.arguments.size() < funcDeclArgs))
            return report(call.location, "argument count mismatch in function call");
    }

    std::vector<std::unique_ptr<ResolvedExpr>> resolvedArguments;
    size_t idx = 0;
    for (auto &&arg : call.arguments) {
        varOrReturn(resolvedArg, resolve_expr(*arg));
        // Only check until vararg
        if (idx < funcDeclArgs) {
            if (!Type::compare(resolvedFuncDecl->params[idx]->type, resolvedArg->type)) {
                return report(resolvedArg->location, "unexpected type of argument '" + resolvedArg->type.to_str() +
                                                         "' expected '" + resolvedFuncDecl->params[idx]->type.to_str() +
                                                         "'");
            }
            if (resolvedFuncDecl->params[idx]->type.isRef) {
                auto unary = dynamic_cast<const ResolvedUnaryOperator *>(resolvedArg.get());
                if (!unary || (unary && unary->op != TokenType::amp)) {
                    return report(resolvedArg->location, "expected to reference the value with '&'");
                }
            }
        }
        resolvedArg->set_constant_value(cee.evaluate(*resolvedArg, false));

        ++idx;
        resolvedArguments.emplace_back(std::move(resolvedArg));
    }

    return std::make_unique<ResolvedCallExpr>(call.location, *resolvedFuncDecl, std::move(resolvedArguments));
}

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
    if (!currentFunction) {
        return report(returnStmt.location, "unexpected return stmt outside a function");
    }

    if (currentFunction->type.kind == Type::Kind::Void && returnStmt.expr)
        return report(returnStmt.location, "unexpected return value in void function");

    if (currentFunction->type.kind != Type::Kind::Void && !returnStmt.expr)
        return report(returnStmt.location, "expected a return value");

    std::unique_ptr<ResolvedExpr> resolvedExpr;
    if (returnStmt.expr) {
        resolvedExpr = resolve_expr(*returnStmt.expr);
        if (!resolvedExpr) return nullptr;

        if (!Type::compare(currentFunction->type, resolvedExpr->type))
            return report(resolvedExpr->location, "unexpected return type");

        resolvedExpr->set_constant_value(cee.evaluate(*resolvedExpr, false));
    }

    return std::make_unique<ResolvedReturnStmt>(returnStmt.location, std::move(resolvedExpr));
}

std::unique_ptr<ResolvedExpr> Sema::resolve_expr(const Expr &expr) {
    if (const auto *number = dynamic_cast<const IntLiteral *>(&expr)) {
        return std::make_unique<ResolvedIntLiteral>(number->location, std::stod(std::string(number->value)));
    }
    if (const auto *character = dynamic_cast<const CharLiteral *>(&expr)) {
        if (auto c = str_from_source(character->value.substr(1, character->value.size() - 1))) {
            return std::make_unique<ResolvedCharLiteral>(character->location, (*c)[0]);
        } else {
            return report(character->location, "malformed char");
        }
    }
    if (const auto *boolean = dynamic_cast<const BoolLiteral *>(&expr)) {
        return std::make_unique<ResolvedBoolLiteral>(boolean->location, boolean->value == "true");
    }
    if (const auto *str = dynamic_cast<const StringLiteral *>(&expr)) {
        if (auto c = str_from_source(str->value.substr(1, str->value.size() - 2))) {
            return std::make_unique<ResolvedStringLiteral>(str->location, *c);
        } else {
            return report(str->location, "malformed string");
        }
    }
    if (const auto *errRef = dynamic_cast<const ErrDeclRefExpr *>(&expr)) {
        return resolve_err_decl_ref_expr(*errRef);
    }
    if (const auto *declRefExpr = dynamic_cast<const DeclRefExpr *>(&expr)) {
        return resolve_decl_ref_expr(*declRefExpr);
    }
    if (const auto *callExpr = dynamic_cast<const CallExpr *>(&expr)) {
        return resolve_call_expr(*callExpr);
    }
    if (const auto *groupingExpr = dynamic_cast<const GroupingExpr *>(&expr)) {
        return resolve_grouping_expr(*groupingExpr);
    }
    if (const auto *binaryOperator = dynamic_cast<const BinaryOperator *>(&expr)) {
        return resolve_binary_operator(*binaryOperator);
    }
    if (const auto *unaryOperator = dynamic_cast<const UnaryOperator *>(&expr)) {
        return resolve_unary_operator(*unaryOperator);
    }
    if (const auto *structInstantiation = dynamic_cast<const StructInstantiationExpr *>(&expr)) {
        return resolve_struct_instantiation(*structInstantiation);
    }
    if (const auto *assignableExpr = dynamic_cast<const AssignableExpr *>(&expr)) {
        return resolve_assignable_expr(*assignableExpr);
    }
    if (const auto *errUnwrapExpr = dynamic_cast<const ErrUnwrapExpr *>(&expr)) {
        return resolve_err_unwrap_expr(*errUnwrapExpr);
    }
    if (const auto *catchErrExpr = dynamic_cast<const CatchErrExpr *>(&expr)) {
        return resolve_catch_err_expr(*catchErrExpr);
    }
    dmz_unreachable("unexpected expression");
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

std::unique_ptr<ResolvedParamDecl> Sema::resolve_param_decl(const ParamDecl &param) {
    std::optional<Type> type = resolve_type(param.type);

    if (!param.isVararg)
        if (!type || type->kind == Type::Kind::Void)
            return report(param.location, "parameter '" + std::string(param.identifier) + "' has invalid '" +
                                              std::string(param.type.name) + "' type");

    return std::make_unique<ResolvedParamDecl>(param.location, param.identifier, *type, param.isMutable,
                                               param.isVararg);
}

std::unique_ptr<ResolvedFuncDecl> Sema::resolve_function_decl(const FuncDecl &function) {
    std::optional<Type> type = resolve_type(function.type);

    if (!type)
        return report(function.location, "function '" + std::string(function.identifier) + "' has invalid '" +
                                             std::string(function.type.name) + "' type");

    if (function.identifier == "main") {
        if (type->kind != Type::Kind::Void)
            return report(function.location, "'main' function is expected to have 'void' type");

        if (!function.params.empty())
            return report(function.location, "'main' function is expected to take no arguments");
    } else if (function.identifier == "printf") {
        return report(function.location,
                      "'printf' is a reserved function name and cannot be used for "
                      "user-defined functions");
    }

    std::vector<std::unique_ptr<ResolvedParamDecl>> resolvedParams;

    ScopeRAII paramScope(*this);
    bool haveVararg = false;
    for (auto &&param : function.params) {
        auto resolvedParam = resolve_param_decl(*param);
        if (haveVararg) {
            report(resolvedParam->location, "vararg '...' can only be in the last argument");
            return nullptr;
        }

        if (!resolvedParam || !insert_decl_to_current_scope(*resolvedParam)) return nullptr;

        if (resolvedParam->isVararg) {
            haveVararg = true;
        }

        resolvedParams.emplace_back(std::move(resolvedParam));
    }

    if (dynamic_cast<const ExternFunctionDecl *>(&function)) {
        return std::make_unique<ResolvedExternFunctionDecl>(function.location, function.identifier, *type,
                                                            std::move(resolvedParams));
    }
    if (dynamic_cast<const FunctionDecl *>(&function)) {
        return std::make_unique<ResolvedFunctionDecl>(function.location, function.identifier, *type,
                                                      std::move(resolvedParams), nullptr);
    }

    dmz_unreachable("unexpected function");
};

std::vector<std::unique_ptr<ResolvedDecl>> Sema::resolve_ast() {
    ScopedTimer st(Stats::type::semanticTime);

    ScopeRAII globalScope(*this);
    std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree;

    bool error = false;
    std::vector<const FuncDecl *> functionsToResolve;

    // Resolve every struct first so that functions have access to them in their
    // signature.
    {
        ScopedTimer st(Stats::type::semanticResolveStructsTime);
        for (auto &&decl : m_ast) {
            if (const auto *st = dynamic_cast<const StructDecl *>(decl.get())) {
                std::unique_ptr<ResolvedDecl> resolvedDecl = resolve_struct_decl(*st);

                if (!resolvedDecl || !insert_decl_to_current_scope(*resolvedDecl)) {
                    error = true;
                    continue;
                }

                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }

            if (const auto *fn = dynamic_cast<const FuncDecl *>(decl.get())) {
                functionsToResolve.emplace_back(fn);
                continue;
            }
            if (const auto *err = dynamic_cast<const ErrGroupDecl *>(decl.get())) {
                std::unique_ptr<ResolvedDecl> resolvedDecl = resolve_err_group_decl(*err);
                if (!resolvedDecl) {
                    error = true;
                    continue;
                }
                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }

            dmz_unreachable("unexpected declaration");
        }
    }

    if (error) return {};

    // Insert println first to be able to detect a possible redeclaration.
    auto *printlnDecl = resolvedTree.emplace_back(create_builtin_println()).get();
    insert_decl_to_current_scope(*printlnDecl);
    {
        ScopedTimer st(Stats::type::semanticResolveFunctionsTime);
        for (auto &&fn : functionsToResolve) {
            if (auto resolvedDecl = resolve_function_decl(*fn);
                resolvedDecl && insert_decl_to_current_scope(*resolvedDecl)) {
                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }

            error = true;
        }
    }
    {
        decltype(functionsToResolve) aux_vec;
        aux_vec.reserve(functionsToResolve.size());

        for (auto &func : functionsToResolve) {
            if (!dynamic_cast<const ExternFunctionDecl *>(func)) {
                aux_vec.emplace_back(func);
            }
        }

        functionsToResolve.swap(aux_vec);
    }
    // Clear the extern functions
    // std::erase_if(functionsToResolve,
    //               [](const FuncDecl *func) { return dynamic_cast<const ExternFunctionDecl *>(func); });

    if (error) return {};
    {
        ScopedTimer st(Stats::type::semanticResolveBodysTime);
        auto nextFunctionDecl = functionsToResolve.begin();
        for (auto &&currentDecl : resolvedTree) {
            if (auto *st = dynamic_cast<ResolvedStructDecl *>(currentDecl.get())) {
                if (!resolve_struct_fields(*st)) error = true;

                continue;
            }

            if (auto *fn = dynamic_cast<ResolvedFunctionDecl *>(currentDecl.get())) {
                if (fn == printlnDecl) continue;

                ScopeRAII paramScope(*this);
                for (auto &&param : fn->params) insert_decl_to_current_scope(*param);

                currentFunction = fn;
                if (auto nextFunc = dynamic_cast<const FunctionDecl *>(*nextFunctionDecl++)) {
                    if (auto resolvedBody = resolve_block(*nextFunc->body)) {
                        fn->body = std::move(resolvedBody);
                        error |= run_flow_sensitive_checks(*fn);
                        continue;
                    }
                } else {
                    continue;
                }

                error = true;
            }
        }
    }
    if (error) return {};

    return resolvedTree;
}

std::unique_ptr<ResolvedUnaryOperator> Sema::resolve_unary_operator(const UnaryOperator &unary) {
    varOrReturn(resolvedRHS, resolve_expr(*unary.operand));

    if (unary.op != TokenType::amp && resolvedRHS->type.kind != Type::Kind::Int)
        return report(resolvedRHS->location, '\'' + std::string(resolvedRHS->type.name) +
                                                 "' cannot be used as an operand to unary operator " +
                                                 std::string(get_op_str(unary.op)));
    auto ret = std::make_unique<ResolvedUnaryOperator>(unary.location, unary.op, std::move(resolvedRHS));
    if (unary.op == TokenType::amp) {
        ret->type.isRef = true;
    }
    return ret;
}

std::unique_ptr<ResolvedBinaryOperator> Sema::resolve_binary_operator(const BinaryOperator &binop) {
    varOrReturn(resolvedLHS, resolve_expr(*binop.lhs));
    varOrReturn(resolvedRHS, resolve_expr(*binop.rhs));

    if (resolvedLHS->type.kind != Type::Kind::Int) {
        return report(resolvedLHS->location, '\'' + std::string(resolvedLHS->type.name) +
                                                 "' cannot be used as LHS operand to binary operator");
    }
    if (resolvedRHS->type.kind != Type::Kind::Int) {
        return report(resolvedRHS->location, '\'' + std::string(resolvedRHS->type.name) +
                                                 "' cannot be used as RHS operand to binary operator");
    }
    if (resolvedLHS->type.kind != resolvedRHS->type.kind || resolvedLHS->type.kind != Type::Kind::Int) {
        return report(binop.location, "unexpected type " + resolvedLHS->type.to_str() + " in binop");
    }

    return std::make_unique<ResolvedBinaryOperator>(binop.location, binop.op, std::move(resolvedLHS),
                                                    std::move(resolvedRHS));
}

std::unique_ptr<ResolvedGroupingExpr> Sema::resolve_grouping_expr(const GroupingExpr &grouping) {
    varOrReturn(resolvedExpr, resolve_expr(*grouping.expr));
    return std::make_unique<ResolvedGroupingExpr>(grouping.location, std::move(resolvedExpr));
}

std::unique_ptr<ResolvedIfStmt> Sema::resolve_if_stmt(const IfStmt &ifStmt) {
    varOrReturn(condition, resolve_expr(*ifStmt.condition));

    if (condition->type.kind != Type::Kind::Int) {
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

    if (condition->type.kind != Type::Kind::Int) {
        return report(condition->location, "expected int in condition");
    }

    varOrReturn(body, resolve_block(*whileStmt.body));

    condition->set_constant_value(cee.evaluate(*condition, false));

    return std::make_unique<ResolvedWhileStmt>(whileStmt.location, std::move(condition), std::move(body));
}

bool Sema::run_flow_sensitive_checks(const ResolvedFunctionDecl &fn) {
    CFG cfg = CFGBuilder().build(fn);

    bool error = false;
    error |= check_return_on_all_paths(fn, cfg);
    error |= check_variable_initialization(cfg);

    return error;
};

bool Sema::check_return_on_all_paths(const ResolvedFunctionDecl &fn, const CFG &cfg) {
    if (fn.type.kind == Type::Kind::Void) return false;

    int returnCount = 0;
    bool exitReached = false;

    std::set<int> visited;
    std::vector<int> worklist;
    worklist.emplace_back(cfg.entry);

    while (!worklist.empty()) {
        int bb = worklist.back();
        worklist.pop_back();

        if (!visited.emplace(bb).second) continue;

        exitReached |= bb == cfg.exit;

        const auto &[preds, succs, stmts] = cfg.m_basicBlocks[bb];

        if (!stmts.empty() && dynamic_cast<const ResolvedReturnStmt *>(stmts[0])) {
            ++returnCount;
            continue;
        }

        for (auto &&[succ, reachable] : succs)
            if (reachable) worklist.emplace_back(succ);
    }

    if (exitReached || returnCount == 0) {
        report(fn.location, returnCount > 0 ? "non-void function doesn't return a value on every path"
                                            : "non-void function doesn't return a value");
    }

    return exitReached || returnCount == 0;
}

std::unique_ptr<ResolvedDeclStmt> Sema::resolve_decl_stmt(const DeclStmt &declStmt) {
    varOrReturn(resolvedVarDecl, resolve_var_decl(*declStmt.varDecl));

    if (!insert_decl_to_current_scope(*resolvedVarDecl)) return nullptr;

    return std::make_unique<ResolvedDeclStmt>(declStmt.location, std::move(resolvedVarDecl));
}

std::unique_ptr<ResolvedVarDecl> Sema::resolve_var_decl(const VarDecl &varDecl) {
    if (!varDecl.type && !varDecl.initializer)
        return report(varDecl.location, "an uninitialized variable is expected to have a type specifier");

    std::unique_ptr<ResolvedExpr> resolvedInitializer = nullptr;
    if (varDecl.initializer) {
        resolvedInitializer = resolve_expr(*varDecl.initializer);
        if (!resolvedInitializer) return nullptr;
    }

    Type resolvableType = varDecl.type.value_or(resolvedInitializer->type);
    std::optional<Type> type = resolve_type(resolvableType);

    if (!type || type->kind == Type::Kind::Void)
        return report(varDecl.location, "variable '" + std::string(varDecl.identifier) + "' has invalid '" +
                                            std::string(resolvableType.name) + "' type");

    if (resolvedInitializer) {
        if (!Type::compare(*type, resolvedInitializer->type))
            return report(resolvedInitializer->location, "initializer type mismatch");

        resolvedInitializer->set_constant_value(cee.evaluate(*resolvedInitializer, false));
    }
    return std::make_unique<ResolvedVarDecl>(varDecl.location, varDecl.identifier, *type, varDecl.isMutable,
                                             std::move(resolvedInitializer));
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

std::unique_ptr<ResolvedAssignableExpr> Sema::resolve_assignable_expr(const AssignableExpr &assignableExpr) {
    if (const auto *declRefExpr = dynamic_cast<const DeclRefExpr *>(&assignableExpr))
        return resolve_decl_ref_expr(*declRefExpr);

    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(&assignableExpr))
        return resolve_member_expr(*memberExpr);

    dmz_unreachable("unexpected assignable expression");
}

std::unique_ptr<ResolvedMemberExpr> Sema::resolve_member_expr(const MemberExpr &memberExpr) {
    auto resolvedBase = resolve_expr(*memberExpr.base);
    if (!resolvedBase) return nullptr;

    if (resolvedBase->type.kind != Type::Kind::Struct)
        return report(memberExpr.base->location,
                      "cannot access field of '" + std::string(resolvedBase->type.name) + '\'');

    const auto *st = lookup_decl<ResolvedStructDecl>(resolvedBase->type.name).first;

    if (!st) {
        return report(memberExpr.location, "failed to lookup struct");
    }

    const ResolvedFieldDecl *fieldDecl = nullptr;
    for (auto &&field : st->fields) {
        if (field->identifier == memberExpr.field) fieldDecl = field.get();
    }

    if (!fieldDecl)
        return report(memberExpr.location, '\'' + std::string(resolvedBase->type.name) + "' has no field called '" +
                                               std::string(memberExpr.field) + '\'');

    return std::make_unique<ResolvedMemberExpr>(memberExpr.location, std::move(resolvedBase), *fieldDecl);
}

bool Sema::check_variable_initialization(const CFG &cfg) {
    enum class State { Bottom, Unassigned, Assigned, Top };

    using Lattice = std::map<const ResolvedDecl *, State>;

    auto joinStates = [](State s1, State s2) {
        if (s1 == s2) return s1;

        if (s1 == State::Bottom) return s2;

        if (s2 == State::Bottom) return s1;

        return State::Top;
    };

    std::vector<Lattice> curLattices(cfg.m_basicBlocks.size());
    std::vector<std::pair<SourceLocation, std::string>> pendingErrors;

    bool changed = true;
    while (changed) {
        changed = false;
        pendingErrors.clear();

        for (int bb = cfg.entry; bb != cfg.exit; --bb) {
            const auto &[preds, succs, stmts] = cfg.m_basicBlocks[bb];

            Lattice tmp;
            for (auto &&pred : preds) {
                for (auto &&[decl, state] : curLattices[pred.first]) {
                    tmp[decl] = joinStates(tmp[decl], state);
                }
            }

            for (auto it = stmts.rbegin(); it != stmts.rend(); ++it) {
                const ResolvedStmt *stmt = *it;

                if (auto *decl = dynamic_cast<const ResolvedDeclStmt *>(stmt)) {
                    tmp[decl->varDecl.get()] = decl->varDecl->initializer ? State::Assigned : State::Unassigned;
                    continue;
                }

                if (auto *assignment = dynamic_cast<const ResolvedAssignment *>(stmt)) {
                    const ResolvedExpr *base = assignment->assignee.get();
                    while (const auto *member = dynamic_cast<const ResolvedMemberExpr *>(base))
                        base = member->base.get();

                    const auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(base);

                    // The base of the expression is not a variable, but a temporary,
                    // which can be mutated.
                    if (!dre) continue;

                    const auto *decl = dynamic_cast<const ResolvedDecl *>(&dre->decl);

                    if (!decl->isMutable && tmp[decl] != State::Unassigned) {
                        std::string msg = '\'' + std::string(decl->identifier) + "' cannot be mutated";
                        pendingErrors.emplace_back(assignment->location, std::move(msg));
                    }

                    tmp[decl] = State::Assigned;
                    continue;
                }

                if (const auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(stmt)) {
                    const auto *var = dynamic_cast<const ResolvedVarDecl *>(&dre->decl);

                    if (var && tmp[var] != State::Assigned) {
                        std::string msg = '\'' + std::string(var->identifier) + "' is not initialized";
                        pendingErrors.emplace_back(dre->location, std::move(msg));
                    }

                    continue;
                }
            }

            if (curLattices[bb] != tmp) {
                curLattices[bb] = tmp;
                changed = true;
            }
        }
    }

    for (auto &&[loc, msg] : pendingErrors) {
        report(loc, msg);
    }

    return !pendingErrors.empty();
}

std::unique_ptr<ResolvedStructInstantiationExpr> Sema::resolve_struct_instantiation(
    const StructInstantiationExpr &structInstantiation) {
    const auto *st = lookup_decl<ResolvedStructDecl>(structInstantiation.identifier).first;

    if (!st)
        return report(structInstantiation.location,
                      "'" + std::string(structInstantiation.identifier) + "' is not a struct type");

    std::vector<std::unique_ptr<ResolvedFieldInitStmt>> resolvedFieldInits;
    std::map<std::string_view, const ResolvedFieldInitStmt *> inits;

    std::map<std::string_view, const ResolvedFieldDecl *> fields;
    for (auto &&fieldDecl : st->fields) fields[fieldDecl->identifier] = fieldDecl.get();

    bool error = false;
    for (auto &&initStmt : structInstantiation.fieldInitializers) {
        std::string_view id = initStmt->identifier;
        const SourceLocation &loc = initStmt->location;

        if (inits.count(id)) {
            report(loc, "field '" + std::string{id} + "' is already initialized");
            error = true;
            continue;
        }

        const ResolvedFieldDecl *fieldDecl = fields[id];
        if (!fieldDecl) {
            report(loc, "'" + std::string(st->identifier) + "' has no field named '" + std::string{id} + "'");
            error = true;
            continue;
        }

        auto resolvedInitExpr = resolve_expr(*initStmt->initializer);
        if (!resolvedInitExpr) {
            error = true;
            continue;
        }

        if (resolvedInitExpr->type.name != fieldDecl->type.name) {
            report(resolvedInitExpr->location, "'" + std::string(resolvedInitExpr->type.name) +
                                                   "' cannot be used to initialize a field of type '" +
                                                   std::string(fieldDecl->type.name) + "'");
            error = true;
            continue;
        }

        auto init = std::make_unique<ResolvedFieldInitStmt>(loc, *fieldDecl, std::move(resolvedInitExpr));
        inits[id] = resolvedFieldInits.emplace_back(std::move(init)).get();
    }

    for (auto &&fieldDecl : st->fields) {
        if (!inits.count(fieldDecl->identifier)) {
            report(structInstantiation.location,
                   "field '" + std::string(fieldDecl->identifier) + "' is not initialized");
            error = true;
            continue;
        }

        auto &initStmt = inits[fieldDecl->identifier];
        initStmt->initializer->set_constant_value(cee.evaluate(*initStmt->initializer, false));
    }

    if (error) return nullptr;

    return std::make_unique<ResolvedStructInstantiationExpr>(structInstantiation.location, *st,
                                                             std::move(resolvedFieldInits));
}

std::unique_ptr<ResolvedStructDecl> Sema::resolve_struct_decl(const StructDecl &structDecl) {
    std::set<std::string_view> identifiers;
    std::vector<std::unique_ptr<ResolvedFieldDecl>> resolvedFields;

    unsigned idx = 0;
    for (auto &&field : structDecl.fields) {
        if (!identifiers.emplace(field->identifier).second)
            return report(field->location, "field '" + std::string(field->identifier) + "' is already declared");

        resolvedFields.emplace_back(
            std::make_unique<ResolvedFieldDecl>(field->location, field->identifier, field->type, idx++));
    }

    return std::make_unique<ResolvedStructDecl>(structDecl.location, structDecl.identifier,
                                                Type::structType(structDecl.identifier), std::move(resolvedFields));
}

bool Sema::resolve_struct_fields(ResolvedStructDecl &resolvedStructDecl) {
    std::stack<std::pair<ResolvedStructDecl *, std::set<const ResolvedStructDecl *>>> worklist;
    worklist.push({&resolvedStructDecl, {}});

    while (!worklist.empty()) {
        auto [currentDecl, visited] = worklist.top();
        worklist.pop();

        if (!visited.emplace(currentDecl).second) {
            report(currentDecl->location, "struct '" + std::string(currentDecl->identifier) + "' contains itself");
            return false;
        }

        for (auto &&field : currentDecl->fields) {
            auto type = resolve_type(field->type);
            if (!type) {
                report(field->location,
                       "unable to resolve '" + std::string(field->type.name) + "' type of struct field");
                return false;
            }

            if (type->kind == Type::Kind::Void) {
                report(field->location, "struct field cannot be void");
                return false;
            }

            if (type->kind == Type::Kind::Struct) {
                auto *nestedStruct = lookup_decl<ResolvedStructDecl>(type->name).first;
                if (!nestedStruct) {
                    report(field->location, "unexpected type");
                    return false;
                }

                worklist.push({nestedStruct, visited});
            }

            field->type = *type;
        }
    }

    return true;
}

std::unique_ptr<ResolvedDeferStmt> Sema::resolve_defer_stmt(const DeferStmt &deferStmt) {
    varOrReturn(block, resolve_block(*deferStmt.block));
    std::shared_ptr<ResolvedBlock> sharedBlock = std::move(block);
    return std::make_unique<ResolvedDeferStmt>(deferStmt.location, sharedBlock);
}

std::unique_ptr<ResolvedErrGroupDecl> Sema::resolve_err_group_decl(const ErrGroupDecl &errGroupDecl) {
    std::vector<std::unique_ptr<ResolvedErrDecl>> resolvedErrors;

    for (auto &&err : errGroupDecl.errs) {
        auto &errDecl = resolvedErrors.emplace_back(std::make_unique<ResolvedErrDecl>(err->location, err->identifier));
        if (!insert_decl_to_current_scope(*errDecl)) return nullptr;
    }

    return std::make_unique<ResolvedErrGroupDecl>(errGroupDecl.location, errGroupDecl.identifier,
                                                  std::move(resolvedErrors));
}

std::unique_ptr<ResolvedErrDeclRefExpr> Sema::resolve_err_decl_ref_expr(const ErrDeclRefExpr &errDeclRef) {
    auto lookupErr = lookup_decl<ResolvedErrDecl>(errDeclRef.identifier).first;

    if (!lookupErr) {
        return report(errDeclRef.location, "err '" + std::string(errDeclRef.identifier) + "' not found");
    }

    return std::make_unique<ResolvedErrDeclRefExpr>(errDeclRef.location, *lookupErr);
}

std::unique_ptr<ResolvedErrUnwrapExpr> Sema::resolve_err_unwrap_expr(const ErrUnwrapExpr &errUnwrapExpr) {
    varOrReturn(resolvedToUnwrap, resolve_expr(*errUnwrapExpr.errToUnwrap));
    if (!resolvedToUnwrap->type.isOptional)
        return report(errUnwrapExpr.location,
                      "unexpected type to unwrap that is not optional '" + resolvedToUnwrap->type.to_str() + "'");

    Type unwrapType = resolvedToUnwrap->type;
    unwrapType.isOptional = false;
    return std::make_unique<ResolvedErrUnwrapExpr>(errUnwrapExpr.location, unwrapType, std::move(resolvedToUnwrap));
}

std::unique_ptr<ResolvedCatchErrExpr> Sema::resolve_catch_err_expr(const CatchErrExpr &catchErrExpr) {
    if (catchErrExpr.errTocatch) {
        auto resolvedErr = resolve_expr(*catchErrExpr.errTocatch);
        return std::make_unique<ResolvedCatchErrExpr>(catchErrExpr.location, std::move(resolvedErr), nullptr);
    } else if (catchErrExpr.declaration) {
        auto declaration = resolve_decl_stmt(*catchErrExpr.declaration);
        return std::make_unique<ResolvedCatchErrExpr>(catchErrExpr.location, nullptr, std::move(declaration));
    } else {
        dmz_unreachable("malformed CatchErrExpr");
    }
}
}  // namespace DMZ