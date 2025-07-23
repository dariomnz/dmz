
// #define DEBUG
#include "semantic/Semantic.hpp"

namespace DMZ {

std::unique_ptr<ResolvedParamDecl> Sema::resolve_param_decl(const ParamDecl &param) {
    debug_func(param.location);
    std::optional<Type> type = resolve_type(param.type);

    if (!param.isVararg)
        if (!type || type->kind == Type::Kind::Void)
            return report(param.location, "parameter '" + std::string(param.identifier) + "' has invalid '" +
                                              std::string(param.type.name) + "' type");

    return std::make_unique<ResolvedParamDecl>(param.location, param.identifier, *type, param.isMutable,
                                               param.isVararg);
}

std::unique_ptr<ResolvedGenericTypeDecl> Sema::resolve_generic_type_decl(const GenericTypeDecl &genericTypeDecl) {
    debug_func(genericTypeDecl.location);
    return std::make_unique<ResolvedGenericTypeDecl>(genericTypeDecl.location, genericTypeDecl.identifier);
}

std::unique_ptr<ResolvedGenericTypesDecl> Sema::resolve_generic_types_decl(const GenericTypesDecl &genericTypesDecl,
                                                                           const GenericTypes &specifiedTypes) {
    debug_func("");
    std::vector<std::unique_ptr<ResolvedGenericTypeDecl>> resolvedTypes;
    resolvedTypes.reserve(genericTypesDecl.types.size());
    for (size_t i = 0; i < genericTypesDecl.types.size(); i++) {
        auto resolvedGenericType = resolve_generic_type_decl(*genericTypesDecl.types[i]);
        if (specifiedTypes.types.size() >= i + 1) resolvedGenericType->specializedType = *specifiedTypes.types[i];
        if (!resolvedGenericType || !insert_decl_to_current_scope(*resolvedGenericType)) return nullptr;
        resolvedTypes.emplace_back(std::move(resolvedGenericType));
    }
    return std::make_unique<ResolvedGenericTypesDecl>(std::move(resolvedTypes));
}

std::unique_ptr<ResolvedMemberFunctionDecl> Sema::resolve_member_function_decl(const ResolvedStructDecl &structDecl,
                                                                               const MemberFunctionDecl &function) {
    debug_func(function.location);

    varOrReturn(resolvedFunc, resolve_function_decl(*function.function));
    varOrReturn(resolvedFunction, dynamic_cast<ResolvedFunctionDecl *>(resolvedFunc.get()));

    resolvedFunc.release();
    std::unique_ptr<ResolvedFunctionDecl> resolvedFunctionDecl(resolvedFunction);

    auto ret = std::make_unique<ResolvedMemberFunctionDecl>(&structDecl, std::move(resolvedFunctionDecl));
    resolvedFunction->parent = ret.get();
    return ret;
}
std::unique_ptr<ResolvedFuncDecl> Sema::resolve_function_decl(const FuncDecl &function) {
    debug_func(function.location);

    ScopeRAII paramScope(*this);
    std::unique_ptr<ResolvedGenericTypesDecl> resolvedGenericTypesDecl;
    if (auto func = dynamic_cast<const FunctionDecl *>(&function)) {
        if (func->genericTypes) {
            resolvedGenericTypesDecl = resolve_generic_types_decl(*func->genericTypes);
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
        return std::make_unique<ResolvedExternFunctionDecl>(function.location, function.identifier, *type,
                                                            std::move(resolvedParams));
    }
    if (auto functionDecl = dynamic_cast<const FunctionDecl *>(&function)) {
        return std::make_unique<ResolvedFunctionDecl>(function.location, function.identifier, nullptr, *type,
                                                      std::move(resolvedParams), std::move(resolvedGenericTypesDecl),
                                                      functionDecl, nullptr);
    }
    function.dump();
    dmz_unreachable("unexpected function");
}

ResolvedFuncDecl *Sema::specialize_generic_function(const ResolvedFuncDecl &parentFunc, ResolvedFunctionDecl &funcDecl,
                                                    const GenericTypes &genericTypes) {
    debug_func(funcDecl.location);
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
        if (func->genericTypes) {
            resolvedGenericTypesDecl = resolve_generic_types_decl(*func->genericTypes, genericTypes);
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

    auto resolvedFunc = std::make_unique<ResolvedSpecializedFunctionDecl>(
        funcDecl.location, funcDecl.identifier, &parentFunc, *type, std::move(resolvedParams), genericTypes, nullptr);

    auto &retFunc = funcDecl.specializations.emplace_back(std::move(resolvedFunc));

    auto prevFunc = m_currentFunction;
    m_currentFunction = retFunc.get();
    auto body = funcDecl.functionDecl->body.get();
    if (auto resolvedBody = resolve_block(*body)) {
        retFunc->body = std::move(resolvedBody);
        if (run_flow_sensitive_checks(*retFunc)) return nullptr;
    }
    m_currentFunction = prevFunc;
    return retFunc.get();
}

ResolvedStructDecl *Sema::specialize_generic_struct(ResolvedStructDecl &struDecl, const GenericTypes &genericTypes) {
    debug_func(struDecl.location);
    static std::mutex specialize_stru_mutex;
    std::unique_lock lock(specialize_stru_mutex);
    // Search if is specified
    for (auto &&stru : struDecl.specializations) {
        if (genericTypes == stru->specGenericTypes) return stru.get();
    }

    // If not found specialize the function
    ScopeRAII paramScope(*this);

    std::unique_ptr<ResolvedGenericTypesDecl> resolvedGenericTypesDecl;
    resolvedGenericTypesDecl = resolve_generic_types_decl(*struDecl.structDecl->genericTypes, genericTypes);
    if (!resolvedGenericTypesDecl) return nullptr;

    std::vector<std::unique_ptr<ResolvedFieldDecl>> resolvedFields;
    std::vector<std::unique_ptr<ResolvedMemberFunctionDecl>> resolvedFunctions;

    auto resolvedStruct = std::make_unique<ResolvedStructDecl>(
        struDecl.location, struDecl.identifier, struDecl.structDecl, Type::structType(struDecl.identifier), nullptr,
        std::move(resolvedFields), std::move(resolvedFunctions));

    unsigned idx = 0;
    for (auto &&field : struDecl.fields) {
        auto type = resolve_type(field->type);
        if (!type) return report(field->location, "cannot resolve '" + field->type.to_str() + "' type");
        resolvedFields.emplace_back(
            std::make_unique<ResolvedFieldDecl>(field->location, field->identifier, *type, idx++));
    }

    for (auto &&function : struDecl.functions) {
        dmz_unreachable("TODO");
        (void)function;
        // varOrReturn(func, resolve_function_decl(*function));
        // auto memberFunc = std::make_unique<ResolvedMemberFunctionDecl>(resolvedStruct, std::move(func));
        // resolvedFunctions.emplace_back(std::move(memberFunc));
    }

    resolvedStruct->fields = std::move(resolvedFields);
    resolvedStruct->functions = std::move(resolvedFunctions);

    if (!resolve_struct_members(*resolvedStruct)) return nullptr;

    auto &retStruct = struDecl.specializations.emplace_back(std::move(resolvedStruct));
    retStruct->type.genericTypes = genericTypes;
    retStruct->specGenericTypes = genericTypes;
    return retStruct.get();
}

std::unique_ptr<ResolvedVarDecl> Sema::resolve_var_decl(const VarDecl &varDecl) {
    debug_func(varDecl.location);
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
    debug_func(structDecl.location);
    std::set<std::string_view> identifiers;
    std::vector<std::unique_ptr<ResolvedFieldDecl>> resolvedFields;
    std::vector<std::unique_ptr<ResolvedMemberFunctionDecl>> resolvedFunctions;

    ScopeRAII fieldScope(*this);
    std::unique_ptr<ResolvedGenericTypesDecl> resolvedGenericTypesDecl;
    if (structDecl.genericTypes) {
        resolvedGenericTypesDecl = resolve_generic_types_decl(*structDecl.genericTypes);
        if (!resolvedGenericTypesDecl) return nullptr;
    }
    auto resStructDecl = std::make_unique<ResolvedStructDecl>(
        structDecl.location, structDecl.identifier, &structDecl, Type::structType(structDecl.identifier),
        std::move(resolvedGenericTypesDecl), std::move(resolvedFields), std::move(resolvedFunctions));

    unsigned idx = 0;
    for (auto &&field : structDecl.fields) {
        if (!identifiers.emplace(field->identifier).second)
            return report(field->location, "field '" + std::string(field->identifier) + "' is already declared");

        resolvedFields.emplace_back(
            std::make_unique<ResolvedFieldDecl>(field->location, field->identifier, field->type, idx++));
    }

    for (auto &&function : structDecl.functions) {
        if (!identifiers.emplace(function->identifier).second)
            return report(function->location,
                          "function '" + std::string(function->identifier) + "' is already declared");
    }

    resStructDecl->fields = std::move(resolvedFields);
    // resStructDecl->functions = std::move(resolvedFunctions);

    return resStructDecl;
}

bool Sema::resolve_struct_members(ResolvedStructDecl &resolvedStructDecl) {
    debug_func(resolvedStructDecl.location);
    if (resolvedStructDecl.genericTypes) return true;
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
                auto *nestedStruct =
                    static_cast<ResolvedStructDecl *>(lookup(type->name, ResolvedDeclType::ResolvedStructDecl).first);
                if (!nestedStruct) {
                    report(field->location, "unexpected type");
                    return false;
                }

                worklist.push({nestedStruct, visited});
            }

            field->type = *type;
        }
    }

    for (size_t i = 0; i < resolvedStructDecl.functions.size(); i++) {
        auto &func = resolvedStructDecl.structDecl->functions[i];
        auto &resfunc = resolvedStructDecl.functions[i];
        if (!resolve_func_body(*resfunc->function, *func->function->body)) return false;
    }

    return true;
}

bool Sema::resolve_struct_decl_funcs(ResolvedStructDecl &resolvedStructDecl) {
    debug_func("");
    std::vector<std::unique_ptr<ResolvedMemberFunctionDecl>> resolvedFunctions;
    for (auto &&function : resolvedStructDecl.structDecl->functions) {
        auto memberFunc = (resolve_member_function_decl(resolvedStructDecl, *function));
        if (!memberFunc) return false;
        resolvedFunctions.emplace_back(std::move(memberFunc));
    }

    resolvedStructDecl.functions = std::move(resolvedFunctions);

    // resolvedStructDecl.dump();

    return true;
}

std::unique_ptr<ResolvedErrGroupDecl> Sema::resolve_err_group_decl(const ErrGroupDecl &errGroupDecl) {
    debug_func(errGroupDecl.location);
    std::vector<std::unique_ptr<ResolvedErrDecl>> resolvedErrors;

    for (auto &&err : errGroupDecl.errs) {
        auto &errDecl = resolvedErrors.emplace_back(std::make_unique<ResolvedErrDecl>(err->location, err->identifier));
        if (!insert_decl_to_current_scope(*errDecl)) return nullptr;
        // if (!insert_decl_to_modules(*errDecl)) return nullptr;
    }

    return std::make_unique<ResolvedErrGroupDecl>(errGroupDecl.location, errGroupDecl.identifier,
                                                  std::move(resolvedErrors));
}

std::unique_ptr<ResolvedModuleDecl> Sema::resolve_module_decl(const ModuleDecl &moduleDecl) {
    debug_func(moduleDecl.location);
    auto resolvedDecls = resolve_in_module_decl(moduleDecl.declarations);
    auto resolvedModuleDecl =
        std::make_unique<ResolvedModuleDecl>(moduleDecl.location, moduleDecl.identifier, std::move(resolvedDecls));
    return resolvedModuleDecl;
}

bool Sema::resolve_module_body(ResolvedModuleDecl &moduleDecl) {
    debug_func(moduleDecl.location);
    return resolve_in_module_body(moduleDecl.declarations);
}

std::vector<std::unique_ptr<ResolvedDecl>> Sema::resolve_in_module_decl(
    const std::vector<std::unique_ptr<Decl>> &decls) {
    debug_func("Decls " << decls.size());
    bool error = false;
    std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree;
    ScopeRAII moduleScope(*this);
    // Resolve every struct first so that functions have access to them in their signature.
    {
        ScopedTimer st(Stats::type::semanticResolveStructsTime);
        for (auto &&decl : decls) {
            debug_msg(decl->identifier << " " << decl->location);
            if (const auto *st = dynamic_cast<const StructDecl *>(decl.get())) {
                std::unique_ptr<ResolvedDecl> resolvedDecl = resolve_struct_decl(*st);

                if (!resolvedDecl || !insert_decl_to_current_scope(*resolvedDecl)) {
                    error = true;
                    continue;
                }

                if (!resolve_struct_decl_funcs(*static_cast<ResolvedStructDecl *>(resolvedDecl.get()))) {
                    error = true;
                    continue;
                }

                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }
            if (const auto *st = dynamic_cast<const ModuleDecl *>(decl.get())) {
                std::unique_ptr<ResolvedDecl> resolvedDecl = resolve_module_decl(*st);

                if (!resolvedDecl || !insert_decl_to_current_scope(*resolvedDecl)) {
                    error = true;
                    continue;
                }

                resolvedTree.emplace_back(std::move(resolvedDecl));
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
        }
    }

    if (error) return {};

    {
        ScopedTimer st(Stats::type::semanticResolveFunctionsTime);
        for (auto &&decl : decls) {
            if (const auto *fn = dynamic_cast<const FuncDecl *>(decl.get())) {
                if (auto resolvedDecl = resolve_function_decl(*fn);
                    resolvedDecl && insert_decl_to_current_scope(*resolvedDecl)) {
                    resolvedTree.emplace_back(std::move(resolvedDecl));
                    continue;
                }

                error = true;
            }

            if (const auto *ds = dynamic_cast<const DeclStmt *>(decl.get())) {
                if (auto declStmt = resolve_decl_stmt(*ds)) {
                    resolvedTree.emplace_back(std::move(declStmt));
                } else {
                    error = true;
                }
                continue;
            }
        }
    }

    if (error) return {};
    return resolvedTree;
}

bool Sema::resolve_in_module_body(const std::vector<std::unique_ptr<ResolvedDecl>> &decls) {
    debug_func("");
    bool error = false;
    ScopeRAII moduleScope(*this);

    for (auto &&currentDeclRef : decls) {
        auto currentDecl = currentDeclRef.get();
        debug_msg(currentDecl << " " << currentDecl->identifier << " " << currentDecl->location);
        if (auto *st = dynamic_cast<ResolvedStructDecl *>(currentDecl)) {
            if (!insert_decl_to_current_scope(*st)) {
                error = true;
                continue;
            }
            for (auto &&func : st->functions) {
                if (!insert_decl_to_current_scope(*func)) {
                    error = true;
                    continue;
                }
            }
            continue;
        }
        if (auto *fn = dynamic_cast<ResolvedFuncDecl *>(currentDecl)) {
            if (!insert_decl_to_current_scope(*fn)) {
                error = true;
            }
            continue;
        }

        if (auto *fn = dynamic_cast<ResolvedDeclStmt *>(currentDecl)) {
            if (!insert_decl_to_current_scope(*fn->varDecl)) {
                error = true;
            }
            continue;
        }
        if (auto *eg = dynamic_cast<ResolvedErrGroupDecl *>(currentDecl)) {
            for (auto &&e : eg->errs) {
                if (!insert_decl_to_current_scope(*e)) {
                    error = true;
                }
            }
            continue;
        }
    }
    {
        ScopedTimer st(Stats::type::semanticResolveBodysTime);
        for (auto &&currentDeclRef : decls) {
            auto currentDecl = currentDeclRef.get();

            if (auto *st = dynamic_cast<ResolvedModuleDecl *>(currentDecl)) {
                error |= resolve_in_module_body(st->declarations);
                continue;
            }
            if (auto *st = dynamic_cast<ResolvedStructDecl *>(currentDecl)) {
                if (!resolve_struct_members(*st)) error = true;
                continue;
            }
            if (auto *fn = dynamic_cast<ResolvedMemberFunctionDecl *>(currentDecl)) {
                currentDecl = fn->function.get();
            }
            if (auto *fn = dynamic_cast<ResolvedFunctionDecl *>(currentDecl)) {
                if (fn->genericTypes) continue;

                if (!resolve_func_body(*fn, *fn->functionDecl->body)) {
                    debug_msg("error resolve_func_body");
                    error = true;
                }
                continue;
            }
        }
    }
    debug_msg("error " << error);
    if (error) return false;

    return true;
}

bool Sema::resolve_func_body(ResolvedFunctionDecl &function, const Block &body) {
    debug_func("");
    ScopeRAII paramScope(*this);
    for (auto &&param : function.params) insert_decl_to_current_scope(*param);

    m_currentFunction = &function;
    if (auto resolvedBody = resolve_block(body)) {
        function.body = std::move(resolvedBody);
        if (run_flow_sensitive_checks(function)) return false;
        debug_msg("true");
        return true;
    }
    debug_msg("false");
    return false;
}

std::unique_ptr<ResolvedImportExpr> Sema::resolve_import_expr(const ImportExpr &importExpr) {
    debug_func(importExpr.location);

    auto lookupModule = cast_lookup(importExpr.identifier, ResolvedModuleDecl);
    if (!lookupModule)
        return report(importExpr.location, "module '" + std::string(importExpr.identifier) + "' not found");

    return std::make_unique<ResolvedImportExpr>(importExpr.location, *lookupModule);
}
}  // namespace DMZ