#include "Semantic.hpp"

#include <cassert>
#include <map>
#include <set>
#include <stack>

#include "Utils.hpp"

namespace C {

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

    auto param = std::make_unique<ResolvedParamDecl>(loc, "n", Type::builtinInt());

    std::vector<std::unique_ptr<ResolvedParamDecl>> params;
    params.emplace_back(std::move(param));

    auto block = std::make_unique<ResolvedBlock>(loc, std::vector<std::unique_ptr<ResolvedStmt>>());

    return std::make_unique<ResolvedFunctionDecl>(loc, "println", Type::builtinVoid(), std::move(params),
                                                  std::move(block));
};

std::optional<Type> Sema::resolve_type(Type parsedType) {
    if (parsedType.kind == Type::Kind::Custom) {
        // TODO
        //  if (auto *decl = lookupDecl<ResolvedStructDecl>(parsedType.name).first)
        //    return Type::structType(decl->identifier);

        return std::nullopt;
    }

    return parsedType;
}

std::unique_ptr<ResolvedDeclRefExpr> Sema::resolve_decl_ref_expr(const DeclRefExpr &declRefExpr, bool isCallee) {
    ResolvedDecl *decl = lookup_decl<ResolvedDecl>(declRefExpr.identifier).first;
    if (!decl) return report(declRefExpr.location, "symbol '" + std::string(declRefExpr.identifier) + "' not found");

    if (!isCallee && dynamic_cast<ResolvedFunctionDecl *>(decl))
        return report(declRefExpr.location, "expected to call function '" + std::string(declRefExpr.identifier) + "'");

    //   if (dynamic_cast<ResolvedStructDecl *>(decl))
    //     return report(declRefExpr.location,
    //                   "expected an instance of '" + decl->type.name + '\'');

    return std::make_unique<ResolvedDeclRefExpr>(declRefExpr.location, *decl);
}

std::unique_ptr<ResolvedCallExpr> Sema::resolve_call_expr(const CallExpr &call) {
    const auto *dre = dynamic_cast<const DeclRefExpr *>(call.callee.get());
    if (!dre) return report(call.location, "expression cannot be called as a function");

    varOrReturn(resolvedCallee, resolve_decl_ref_expr(*dre, true));

    const auto *resolvedFunctionDecl = dynamic_cast<const ResolvedFunctionDecl *>(&resolvedCallee->decl);

    if (!resolvedFunctionDecl) return report(call.location, "calling non-function symbol");

    if (call.arguments.size() != resolvedFunctionDecl->params.size())
        return report(call.location, "argument count mismatch in function call");

    std::vector<std::unique_ptr<ResolvedExpr>> resolvedArguments;
    int idx = 0;
    for (auto &&arg : call.arguments) {
        varOrReturn(resolvedArg, resolve_expr(*arg));

        if (resolvedArg->type.name != resolvedFunctionDecl->params[idx]->type.name)
            return report(resolvedArg->location, "unexpected type of argument");

        // resolvedArg->setConstantValue(cee.evaluate(*resolvedArg, false));

        ++idx;
        resolvedArguments.emplace_back(std::move(resolvedArg));
    }

    return std::make_unique<ResolvedCallExpr>(call.location, *resolvedFunctionDecl, std::move(resolvedArguments));
}

std::unique_ptr<ResolvedStmt> Sema::resolve_stmt(const Statement &stmt) {
    if (auto *expr = dynamic_cast<const Expr *>(&stmt)) return resolve_expr(*expr);

    //   if (auto *ifStmt = dynamic_cast<const IfStmt *>(&stmt))
    //     return resolveIfStmt(*ifStmt);

    //   if (auto *assignment = dynamic_cast<const Assignment *>(&stmt))
    //     return resolveAssignment(*assignment);

    //   if (auto *declStmt = dynamic_cast<const DeclStmt *>(&stmt))
    //     return resolveDeclStmt(*declStmt);

    //   if (auto *whileStmt = dynamic_cast<const WhileStmt *>(&stmt))
    //     return resolveWhileStmt(*whileStmt);

    if (auto *returnStmt = dynamic_cast<const ReturnStmt *>(&stmt)) return resolve_return_stmt(*returnStmt);

    llvm_unreachable("unexpected statement");
}

std::unique_ptr<ResolvedReturnStmt> Sema::resolve_return_stmt(const ReturnStmt &returnStmt) {
    assert(currentFunction && "return stmt outside a function");

    if (currentFunction->type.kind == Type::Kind::Void && returnStmt.expr)
        return report(returnStmt.location, "unexpected return value in void function");

    if (currentFunction->type.kind != Type::Kind::Void && !returnStmt.expr)
        return report(returnStmt.location, "expected a return value");

    std::unique_ptr<ResolvedExpr> resolvedExpr;
    if (returnStmt.expr) {
        resolvedExpr = resolve_expr(*returnStmt.expr);
        if (!resolvedExpr) return nullptr;

        if (currentFunction->type.name != resolvedExpr->type.name)
            return report(resolvedExpr->location, "unexpected return type");

        // resolvedExpr->setConstantValue(cee.evaluate(*resolvedExpr, false));
    }

    return std::make_unique<ResolvedReturnStmt>(returnStmt.location, std::move(resolvedExpr));
}

std::unique_ptr<ResolvedExpr> Sema::resolve_expr(const Expr &expr) {
    if (const auto *number = dynamic_cast<const NumberLiteral *>(&expr))
        return std::make_unique<ResolvedNumberLiteral>(number->location, std::stod(std::string(number->value)));

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

    //   if (const auto *structInstantiation =
    //           dynamic_cast<const StructInstantiationExpr *>(&expr))
    //     return resolveStructInstantiation(*structInstantiation);

    //   if (const auto *assignableExpr = dynamic_cast<const AssignableExpr *>(&expr))
    //     return resolveAssignableExpr(*assignableExpr);

    llvm_unreachable("unexpected expression");
}

std::unique_ptr<ResolvedBlock> Sema::resolve_block(const Block &block) {
    std::vector<std::unique_ptr<ResolvedStmt>> resolvedStatements;

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

        if (dynamic_cast<ReturnStmt *>(stmt.get())) ++reportUnreachableCount;
    }

    if (error) return nullptr;

    return std::make_unique<ResolvedBlock>(block.location, std::move(resolvedStatements));
}

std::unique_ptr<ResolvedParamDecl> Sema::resolve_param_decl(const ParamDecl &param) {
    std::optional<Type> type = resolve_type(param.type);

    if (!type || type->kind == Type::Kind::Void)
        return report(param.location, "parameter '" + std::string(param.identifier) + "' has invalid '" +
                                          std::string(param.type.name) + "' type");

    return std::make_unique<ResolvedParamDecl>(param.location, param.identifier, *type);
}

std::unique_ptr<ResolvedFunctionDecl> Sema::resolve_function_decl(const FunctionDecl &function) {
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
    for (auto &&param : function.params) {
        auto resolvedParam = resolve_param_decl(*param);

        if (!resolvedParam || !insert_decl_to_current_scope(*resolvedParam)) return nullptr;

        resolvedParams.emplace_back(std::move(resolvedParam));
    }

    return std::make_unique<ResolvedFunctionDecl>(function.location, function.identifier, *type,
                                                  std::move(resolvedParams), nullptr);
};

std::vector<std::unique_ptr<ResolvedDecl>> Sema::resolve_ast() {
    ScopeRAII globalScope(*this);
    std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree;

    bool error = false;
    std::vector<const FunctionDecl *> functionsToResolve;

    // Resolve every struct first so that functions have access to them in their
    // signature.
    for (auto &&decl : m_ast) {
        // if (const auto *st = dynamic_cast<const StructDecl *>(decl.get())) {
        //   std::unique_ptr<ResolvedDecl> resolvedDecl = resolveStructDecl(*st);

        //   if (!resolvedDecl || !insertDeclToCurrentScope(*resolvedDecl)) {
        //     error = true;
        //     continue;
        //   }

        //   resolvedTree.emplace_back(std::move(resolvedDecl));
        //   continue;
        // }

        if (const auto *fn = dynamic_cast<const FunctionDecl *>(decl.get())) {
            functionsToResolve.emplace_back(fn);
            continue;
        }

        llvm_unreachable("unexpected declaration");
    }

    if (error) return {};

    // Insert println first to be able to detect a possible redeclaration.
    auto *printlnDecl = resolvedTree.emplace_back(create_builtin_println()).get();
    insert_decl_to_current_scope(*printlnDecl);

    for (auto &&fn : functionsToResolve) {
        if (auto resolvedDecl = resolve_function_decl(*fn);
            resolvedDecl && insert_decl_to_current_scope(*resolvedDecl)) {
            resolvedTree.emplace_back(std::move(resolvedDecl));
            continue;
        }

        error = true;
    }

    if (error) return {};

    auto nextFunctionDecl = functionsToResolve.begin();
    for (auto &&currentDecl : resolvedTree) {
        // if (auto *st = dynamic_cast<ResolvedStructDecl *>(currentDecl.get())) {
        //   if (!resolveStructFields(*st))
        //     error = true;

        //   continue;
        // }

        if (auto *fn = dynamic_cast<ResolvedFunctionDecl *>(currentDecl.get())) {
            if (fn == printlnDecl) continue;

            ScopeRAII paramScope(*this);
            for (auto &&param : fn->params) insert_decl_to_current_scope(*param);

            currentFunction = fn;
            if (auto resolvedBody = resolve_block(*(*nextFunctionDecl++)->body)) {
                fn->body = std::move(resolvedBody);
                // error |= runFlowSensitiveChecks(*fn);
                continue;
            }

            error = true;
        }
    }

    if (error) return {};

    return resolvedTree;
}

std::unique_ptr<ResolvedUnaryOperator> Sema::resolve_unary_operator(const UnaryOperator &unary) {
    varOrReturn(resolvedRHS, resolve_expr(*unary.operand));

    if (resolvedRHS->type.kind == Type::Kind::Void)
        return report(resolvedRHS->location, "void expression cannot be used as an operand to unary operator");

    return std::make_unique<ResolvedUnaryOperator>(unary.location, unary.op, std::move(resolvedRHS));
}

std::unique_ptr<ResolvedBinaryOperator> Sema::resolve_binary_operator(const BinaryOperator &binop) {
    varOrReturn(resolvedLHS, resolve_expr(*binop.lhs));
    varOrReturn(resolvedRHS, resolve_expr(*binop.rhs));

    if (resolvedLHS->type.kind == Type::Kind::Void)
        return report(resolvedLHS->location, "void expression cannot be used as LHS operand to binary operator");

    if (resolvedRHS->type.kind == Type::Kind::Void)
        return report(resolvedRHS->location, "void expression cannot be used as RHS operand to binary operator");

    return std::make_unique<ResolvedBinaryOperator>(binop.location, binop.op, std::move(resolvedLHS),
                                                    std::move(resolvedRHS));
}

std::unique_ptr<ResolvedGroupingExpr> Sema::resolve_grouping_expr(const GroupingExpr &grouping) {
    varOrReturn(resolvedExpr, resolve_expr(*grouping.expr));
    return std::make_unique<ResolvedGroupingExpr>(grouping.location, std::move(resolvedExpr));
}
}  // namespace C