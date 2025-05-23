#include <map>
#include <stack>
#include <unordered_set>

#include "semantic/Semantic.hpp"

namespace DMZ {

std::unique_ptr<ResolvedFunctionDecl> Sema::create_builtin_println() {
    SourceLocation loc{"<builtin>", 0, 0};

    auto param = std::make_unique<ResolvedParamDecl>(loc, "n", Type::builtinInt(), false);

    std::vector<std::unique_ptr<ResolvedParamDecl>> params;
    params.emplace_back(std::move(param));

    auto block = std::make_unique<ResolvedBlock>(loc, std::vector<std::unique_ptr<ResolvedStmt>>());

    return std::make_unique<ResolvedFunctionDecl>(loc, "println", Type::builtinVoid(), std::move(params),
                                                  std::move(block));
};

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

std::unique_ptr<ResolvedErrGroupDecl> Sema::resolve_err_group_decl(const ErrGroupDecl &errGroupDecl) {
    std::vector<std::unique_ptr<ResolvedErrDecl>> resolvedErrors;

    for (auto &&err : errGroupDecl.errs) {
        auto &errDecl = resolvedErrors.emplace_back(std::make_unique<ResolvedErrDecl>(err->location, err->identifier));
        if (!insert_decl_to_current_scope(*errDecl)) return nullptr;
    }

    return std::make_unique<ResolvedErrGroupDecl>(errGroupDecl.location, errGroupDecl.identifier,
                                                  std::move(resolvedErrors));
}

std::unique_ptr<ResolvedModuleDecl> Sema::resolve_module_decl(const ModuleDecl &moduleDecl) {
    ScopeRAII moduleScope(*this);
    if (moduleDecl.nestedModule) {
        varOrReturn(nestedModule, resolve_module_decl(*moduleDecl.nestedModule));

        return std::make_unique<ResolvedModuleDecl>(moduleDecl.location, moduleDecl.identifier,
                                                    std::move(nestedModule));
    }

    auto resolvedDecls = resolve_in_module_decl(moduleDecl.declarations);
    if (resolvedDecls.size() == 0) return nullptr;

    return std::make_unique<ResolvedModuleDecl>(moduleDecl.location, moduleDecl.identifier, nullptr,
                                                std::move(resolvedDecls));
}

bool Sema::resolve_module_body(ResolvedModuleDecl &moduleDecl) {
    ScopeRAII moduleScope(*this);
    if (moduleDecl.nestedModule) {
        return resolve_module_body(*moduleDecl.nestedModule);
    }

    return resolve_in_module_body(moduleDecl.declarations);
}

std::vector<std::unique_ptr<ResolvedDecl>> Sema::resolve_in_module_decl(
    const std::vector<std::unique_ptr<Decl>> &decls) {
    bool error = false;
    std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree;

    // Resolve every struct first so that functions have access to them in their
    // signature.
    {
        ScopedTimer st(Stats::type::semanticResolveStructsTime);
        for (auto &&decl : decls) {
            if (const auto *st = dynamic_cast<const StructDecl *>(decl.get())) {
                std::unique_ptr<ResolvedDecl> resolvedDecl = resolve_struct_decl(*st);

                if (!resolvedDecl || !insert_decl_to_current_scope(*resolvedDecl)) {
                    error = true;
                    continue;
                }

                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }

            if (dynamic_cast<const FuncDecl *>(decl.get())) {
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

    if (m_scopes.size() == 1) {
        // Insert println first to be able to detect a possible redeclaration.
        auto *printlnDecl = resolvedTree.emplace_back(create_builtin_println()).get();
        insert_decl_to_current_scope(*printlnDecl);
    }
    {
        ScopedTimer st(Stats::type::semanticResolveFunctionsTime);
        for (auto &&decl : decls) {
            if (const auto *fn = dynamic_cast<const FuncDecl *>(decl.get())) {
                if (auto resolvedDecl = resolve_function_decl(*fn);
                    resolvedDecl && insert_decl_to_current_scope(*resolvedDecl)) {
                    auto &resolvedDeclEmpaced = resolvedTree.emplace_back(std::move(resolvedDecl));
                    if (auto resolvedfndecl = dynamic_cast<const ResolvedFunctionDecl *>(resolvedDeclEmpaced.get())) {
                        if (auto fndecl = dynamic_cast<const FunctionDecl *>(fn)) {
                            m_functionsToResolveMap[resolvedfndecl] = fndecl->body.get();
                        }
                    }
                    continue;
                }

                error = true;
            }
        }
    }

    if (error) return {};
    return resolvedTree;
}

bool Sema::resolve_in_module_body(const std::vector<std::unique_ptr<ResolvedDecl>> &decls) {
    bool error = false;
    {
        ScopedTimer st(Stats::type::semanticResolveBodysTime);
        for (auto &&currentDecl : decls) {
            if (auto *st = dynamic_cast<ResolvedStructDecl *>(currentDecl.get())) {
                if (!resolve_struct_fields(*st)) error = true;

                continue;
            }

            if (auto *fn = dynamic_cast<ResolvedFunctionDecl *>(currentDecl.get())) {
                if (fn->identifier == "println") continue;

                ScopeRAII paramScope(*this);
                for (auto &&param : fn->params) insert_decl_to_current_scope(*param);

                m_currentFunction = fn;
                if (auto body = m_functionsToResolveMap[fn]) {
                    m_functionsToResolveMap.erase(fn);
                    if (auto resolvedBody = resolve_block(*body)) {
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
    if (error) return false;

    return true;
}
}  // namespace DMZ