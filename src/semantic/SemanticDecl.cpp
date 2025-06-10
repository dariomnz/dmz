
#include "semantic/Semantic.hpp"

namespace DMZ {

// std::unique_ptr<ResolvedFunctionDecl> Sema::create_builtin_println() {
//     SourceLocation loc{"<builtin>", 0, 0};

//     auto param = std::make_unique<ResolvedParamDecl>(loc, "n", Type::builtinInt(), false);

//     std::vector<std::unique_ptr<ResolvedParamDecl>> params;
//     params.emplace_back(std::move(param));

//     auto block = std::make_unique<ResolvedBlock>(loc, std::vector<std::unique_ptr<ResolvedStmt>>());

//     return std::make_unique<ResolvedFunctionDecl>(loc, "println", Type::builtinVoid(), std::move(params),
//                                                   std::move(block));
// };

std::unique_ptr<ResolvedParamDecl> Sema::resolve_param_decl(const ParamDecl &param) {
    std::optional<Type> type = resolve_type(param.type);

    if (!param.isVararg)
        if (!type || type->kind == Type::Kind::Void)
            return report(param.location, "parameter '" + std::string(param.identifier) + "' has invalid '" +
                                              std::string(param.type.name) + "' type");

    return std::make_unique<ResolvedParamDecl>(param.location, param.identifier, *type, param.isMutable,
                                               param.isVararg);
}

std::unique_ptr<ResolvedGenericTypeDecl> Sema::resolve_generic_type_decl(const GenericTypeDecl &genericTypeDecl) {
    return std::make_unique<ResolvedGenericTypeDecl>(genericTypeDecl.location, genericTypeDecl.identifier,
                                                     m_currentModuleID);
}

std::unique_ptr<ResolvedGenericTypesDecl> Sema::resolve_generic_types_decl(const GenericTypesDecl &genericTypesDecl,
                                                                           const GenericTypes &specifiedTypes) {
    std::vector<std::unique_ptr<ResolvedGenericTypeDecl>> resolvedTypes;
    resolvedTypes.reserve(genericTypesDecl.types.size());
    for (size_t i = 0; i < genericTypesDecl.types.size(); i++) {
        auto resolvedGenericType = resolve_generic_type_decl(*genericTypesDecl.types[i]);
        if (specifiedTypes.types.size() >= i + 1) resolvedGenericType->specializedType = specifiedTypes.types[i];
        if (!resolvedGenericType || !insert_decl_to_current_scope(*resolvedGenericType)) return nullptr;
        resolvedTypes.emplace_back(std::move(resolvedGenericType));
    }
    return std::make_unique<ResolvedGenericTypesDecl>(std::move(resolvedTypes));
}

std::unique_ptr<ResolvedFuncDecl> Sema::resolve_function_decl(const FuncDecl &function) {
    if (auto memberFunctionDecl = dynamic_cast<const MemberFunctionDecl *>(&function)) {
        varOrReturn(resolvedStructDecl, lookup_decl<ResolvedStructDecl>(memberFunctionDecl->base.name).first);
        varOrReturn(resolvedFunc, resolve_function_decl(*memberFunctionDecl->function));
        varOrReturn(resolvedFunction, dynamic_cast<ResolvedFunctionDecl *>(resolvedFunc.get()));

        resolvedFunc.release();
        std::unique_ptr<ResolvedFunctionDecl> resolvedFunctionDecl(resolvedFunction);

        return std::make_unique<ResolvedMemberFunctionDecl>(function.location, function.identifier, *resolvedStructDecl,
                                                            m_currentModuleID, std::move(resolvedFunctionDecl));
    }

    ScopeRAII paramScope(*this);
    std::unique_ptr<ResolvedGenericTypesDecl> resolvedGenericTypesDecl;
    if (auto func = dynamic_cast<const FunctionDecl *>(&function)) {
        if (func->genericType) {
            resolvedGenericTypesDecl = resolve_generic_types_decl(*func->genericType);
            if (!resolvedGenericTypesDecl) return nullptr;
        }
    }

    std::optional<Type> type = resolve_type(function.type);

    if (!type)
        return report(function.location, "function '" + std::string(function.identifier) + "' has invalid '" +
                                             std::string(function.type.name) + "' type");

    if (function.identifier == "main") {
        if (type->kind != Type::Kind::Void)
            return report(function.location, "'main' function is expected to have 'void' type");

        if (!function.params.empty())
            return report(function.location, "'main' function is expected to take no arguments");
    }

    std::vector<std::unique_ptr<ResolvedParamDecl>> resolvedParams;

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
        // println("ptr resolvedParam1 " << resolvedParam.get());
        resolvedParams.emplace_back(std::move(resolvedParam));
        // println("ptr resolvedParam2 " << resolvedParams.back().get());
    }

    if (dynamic_cast<const ExternFunctionDecl *>(&function)) {
        return std::make_unique<ResolvedExternFunctionDecl>(function.location, function.identifier, m_currentModuleID,
                                                            *type, std::move(resolvedParams));
    }
    if (auto functionDecl = dynamic_cast<const FunctionDecl *>(&function)) {
        return std::make_unique<ResolvedFunctionDecl>(function.location, function.identifier, m_currentModuleID, *type,
                                                      std::move(resolvedParams), std::move(resolvedGenericTypesDecl),
                                                      functionDecl, nullptr);
    }
    function.dump();
    dmz_unreachable("unexpected function");
}

ResolvedFuncDecl *Sema::specialize_generic_function(ResolvedFunctionDecl &funcDecl, const GenericTypes &genericTypes) {
    static std::mutex specialize_func_mutex;
    std::unique_lock lock(specialize_func_mutex);
    // Search if is specified
    for (auto &&func : funcDecl.specializations) {
        if (genericTypes == func->genericTypes) return func.get();
    }

    // If not found specialize the function
    ScopeRAII paramScope(*this);
    std::unique_ptr<ResolvedGenericTypesDecl> resolvedGenericTypesDecl;
    if (auto func = dynamic_cast<const FunctionDecl *>(funcDecl.functionDecl)) {
        if (func->genericType) {
            resolvedGenericTypesDecl = resolve_generic_types_decl(*func->genericType, genericTypes);
            if (!resolvedGenericTypesDecl) return nullptr;
        }
    }

    std::optional<Type> type = resolve_type(funcDecl.functionDecl->type);

    if (!type)
        return report(funcDecl.location, "function '" + std::string(funcDecl.identifier) + "' has invalid '" +
                                             std::string(funcDecl.type.name) + "' type");

    std::vector<std::unique_ptr<ResolvedParamDecl>> resolvedParams;

    bool haveVararg = false;
    for (auto &&param : funcDecl.functionDecl->params) {
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

    auto resolvedFunc =
        std::make_unique<ResolvedSpecializedFunctionDecl>(funcDecl.location, funcDecl.identifier, funcDecl.moduleID,
                                                          *type, std::move(resolvedParams), genericTypes, nullptr);

    auto &retFunc = funcDecl.specializations.emplace_back(std::move(resolvedFunc));

    auto prevFunc = m_currentFunction;
    m_currentFunction = retFunc.get();
    auto body = funcDecl.functionDecl->body.get();
    if (auto resolvedBody = resolve_block(*body)) {
        retFunc->body = std::move(resolvedBody);
        // TODO: think if necesary in specialization
        //  if (!run_flow_sensitive_checks(*retFunc)) return nullptr;
    }
    m_currentFunction = prevFunc;
    return retFunc.get();
}

std::unique_ptr<ResolvedVarDecl> Sema::resolve_var_decl(const VarDecl &varDecl) {
    if (!varDecl.type && !varDecl.initializer)
        return report(varDecl.location, "an uninitialized variable is expected to have a type specifier");

    std::unique_ptr<ResolvedExpr> resolvedInitializer = nullptr;
    if (varDecl.initializer) {
        resolvedInitializer = resolve_expr(*varDecl.initializer);
        if (!resolvedInitializer) return nullptr;
    }

    Type *resolvableType = varDecl.type.get() != nullptr ? varDecl.type.get() : &resolvedInitializer->type;
    std::optional<Type> type = resolve_type(*resolvableType);

    if (!type || type->kind == Type::Kind::Void)
        return report(varDecl.location, "variable '" + std::string(varDecl.identifier) + "' has invalid '" +
                                            std::string(resolvableType->name) + "' type");

    if (resolvedInitializer) {
        if (dynamic_cast<ResolvedArrayInstantiationExpr *>(resolvedInitializer.get()) &&
            resolvedInitializer->type.kind == Type::Kind::Void && resolvedInitializer->type.isArray) {
            resolvedInitializer->type = *type;
            resolvedInitializer->type.isArray = 0;
        }
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

    return std::make_unique<ResolvedStructDecl>(structDecl.location, structDecl.identifier, m_currentModuleID,
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
        auto &errDecl = resolvedErrors.emplace_back(
            std::make_unique<ResolvedErrDecl>(err->location, err->identifier, m_currentModuleID));
        if (!insert_decl_to_current_scope(*errDecl)) return nullptr;
        if (!insert_decl_to_modules(*errDecl)) return nullptr;
    }

    return std::make_unique<ResolvedErrGroupDecl>(errGroupDecl.location, errGroupDecl.identifier, m_currentModuleID,
                                                  std::move(resolvedErrors));
}

std::unique_ptr<ResolvedModuleDecl> Sema::resolve_module_decl(const ModuleDecl &moduleDecl,
                                                              const ModuleID &prevModuleID) {
    ScopeRAII moduleScope(*this);
    std::unique_ptr<ResolvedModuleDecl> resolvedModuleDecl;
    ModuleID currentModuleID = prevModuleID;
    currentModuleID.modules.emplace_back(moduleDecl.identifier);
    m_currentModuleID = currentModuleID;
    if (moduleDecl.nestedModule) {
        varOrReturn(nestedModule, resolve_module_decl(*moduleDecl.nestedModule, currentModuleID));
        resolvedModuleDecl = std::make_unique<ResolvedModuleDecl>(moduleDecl.location, moduleDecl.identifier,
                                                                  prevModuleID, std::move(nestedModule));
    } else {
        auto resolvedDecls = resolve_in_module_decl(moduleDecl.declarations);
        resolvedModuleDecl = std::make_unique<ResolvedModuleDecl>(moduleDecl.location, moduleDecl.identifier,
                                                                  prevModuleID, nullptr, std::move(resolvedDecls));
    }
    insert_decl_to_modules(*resolvedModuleDecl);
    return resolvedModuleDecl;
}

bool Sema::resolve_module_body(ResolvedModuleDecl &moduleDecl) {
    ScopeRAII moduleScope(*this);
    bool result;
    ModuleID currentModuleID = moduleDecl.moduleID;
    currentModuleID.modules.emplace_back(moduleDecl.identifier);
    m_currentModuleID = currentModuleID;
    if (moduleDecl.nestedModule) {
        result = resolve_module_body(*moduleDecl.nestedModule);
    } else {
        result = resolve_in_module_body(moduleDecl.declarations);
    }
    return result;
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
            if (const auto *import = dynamic_cast<const ImportDecl *>(decl.get())) {
                if (m_currentModuleID.empty()) {
                    auto resolvedImport = resolve_import_decl(*import, ModuleID{});
                    if (!resolvedImport) {
                        error = true;
                        continue;
                    }
                    resolvedTree.emplace_back(std::move(resolvedImport));
                    continue;
                }
            }
            decl->dump();
            dmz_unreachable("unexpected declaration");
        }
    }

    if (error) return {};

    // if (expectMain && m_scopes.size() == 1) {
    //     // Insert println first to be able to detect a possible redeclaration.
    //     auto *printlnDecl = resolvedTree.emplace_back(create_builtin_println()).get();
    //     insert_decl_to_current_scope(*printlnDecl);
    // }
    {
        ScopedTimer st(Stats::type::semanticResolveFunctionsTime);
        for (auto &&decl : decls) {
            if (const auto *fn = dynamic_cast<const FuncDecl *>(decl.get())) {
                if (auto resolvedDecl = resolve_function_decl(*fn); resolvedDecl &&
                                                                    insert_decl_to_current_scope(*resolvedDecl) &&
                                                                    insert_decl_to_modules(*resolvedDecl)) {
                    auto &resolvedDeclEmpaced = resolvedTree.emplace_back(std::move(resolvedDecl));
                    if (auto resolvedfndecl = dynamic_cast<const ResolvedFunctionDecl *>(resolvedDeclEmpaced.get())) {
                        if (auto fndecl = dynamic_cast<const FunctionDecl *>(fn)) {
                            m_functionsToResolveMap[resolvedfndecl] = fndecl->body.get();
                        }
                    }
                    if (auto resolvedfndecl =
                            dynamic_cast<const ResolvedMemberFunctionDecl *>(resolvedDeclEmpaced.get())) {
                        if (auto fndecl = dynamic_cast<const MemberFunctionDecl *>(fn)) {
                            m_functionsToResolveMap[resolvedfndecl->function.get()] = fndecl->function->body.get();
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
        for (auto &&currentDeclRef : decls) {
            auto currentDecl = currentDeclRef.get();
            if (auto *importDecl = dynamic_cast<ResolvedImportDecl *>(currentDecl)) {
                if (!resolve_import_check(*importDecl)) error = true;
                continue;
            }
            if (auto *st = dynamic_cast<ResolvedStructDecl *>(currentDecl)) {
                if (!resolve_struct_fields(*st)) error = true;
                continue;
            }
            if (auto *fn = dynamic_cast<ResolvedMemberFunctionDecl *>(currentDecl)) {
                currentDecl = fn->function.get();
            }
            if (auto *fn = dynamic_cast<ResolvedFunctionDecl *>(currentDecl)) {
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

std::unique_ptr<ResolvedImportDecl> Sema::resolve_import_decl(const ImportDecl &importDecl,
                                                              const ModuleID &prevModuleID) {
    ModuleID currentModuleID = prevModuleID;
    currentModuleID.modules.emplace_back(importDecl.identifier);
    std::unique_ptr<ResolvedImportDecl> resolvedImportDecl;
    if (importDecl.nestedImport) {
        varOrReturn(nestedImport, resolve_import_decl(*importDecl.nestedImport, currentModuleID));
        resolvedImportDecl = std::make_unique<ResolvedImportDecl>(
            importDecl.location, importDecl.identifier, prevModuleID, std::move(nestedImport), importDecl.alias);
    } else {
        resolvedImportDecl = std::make_unique<ResolvedImportDecl>(importDecl.location, importDecl.identifier,
                                                                  prevModuleID, nullptr, importDecl.alias);
    }
    return resolvedImportDecl;
}

bool Sema::resolve_import_check(ResolvedImportDecl &importDecl) {
    const auto moduleDecl = lookup_in_modules(importDecl.moduleID, importDecl.identifier);
    if (!moduleDecl) {
        report(importDecl.location, "module '" + std::string(importDecl.identifier) + "' not found");
        return false;
    }
    if (importDecl.nestedImport) {
        return resolve_import_check(*importDecl.nestedImport);
    }

    return insert_decl_to_current_scope(importDecl);
}
}  // namespace DMZ