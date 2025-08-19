
// #define DEBUG
#include "Stats.hpp"
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

    return std::make_unique<ResolvedMemberFunctionDecl>(
        resolvedFunctionDecl->location, resolvedFunctionDecl->identifier, nullptr, resolvedFunctionDecl->type,
        std::move(resolvedFunctionDecl->params), std::move(resolvedFunctionDecl->genericTypes),
        resolvedFunctionDecl->functionDecl, std::move(resolvedFunctionDecl->body), &structDecl);
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
        resolvedParams.emplace_back(std::move(resolvedParam));
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

    auto resolvedStruct =
        std::make_unique<ResolvedStructDecl>(struDecl.location, struDecl.identifier, struDecl.structDecl, nullptr,
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
    auto resStructDecl = std::make_unique<ResolvedStructDecl>(structDecl.location, structDecl.identifier, &structDecl,
                                                              std::move(resolvedGenericTypesDecl),
                                                              std::move(resolvedFields), std::move(resolvedFunctions));

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
        if (!resolve_func_body(*resfunc, *func->function->body)) return false;
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

std::unique_ptr<ResolvedErrorGroupExprDecl> Sema::resolve_error_group_expr_decl(
    const ErrorGroupExprDecl &ErrorGroupExprDecl) {
    debug_func(ErrorGroupExprDecl.location);
    std::vector<std::unique_ptr<ResolvedErrorDecl>> resolvedErrors;

    for (auto &&err : ErrorGroupExprDecl.errs) {
        resolvedErrors.emplace_back(std::make_unique<ResolvedErrorDecl>(err->location, err->identifier));
        // if (!insert_decl_to_current_scope(*ErrorDecl)) return nullptr;
        // if (!insert_decl_to_modules(*ErrorDecl)) return nullptr;
    }

    // println("Resolve error group with size " << resolvedErrors.size());
    return std::make_unique<ResolvedErrorGroupExprDecl>(ErrorGroupExprDecl.location, std::move(resolvedErrors));
}

std::unique_ptr<ResolvedModuleDecl> Sema::resolve_module(const ModuleDecl &moduleDecl, int level) {
    debug_func(moduleDecl.location);
    std::vector<std::unique_ptr<DMZ::ResolvedDecl>> declarations;
    bool error = false;
    {
        ScopeRAII moduleScope(*this);
        for (auto &&decl : moduleDecl.declarations) {
            if (auto *md = dynamic_cast<ModuleDecl *>(decl.get())) {
                int next_level = moduleDecl.identifier.find(".dmz") == std::string::npos ? level + 1 : level;
                auto resolvedDecl = resolve_module(*md, next_level);

                if (!resolvedDecl || !insert_decl_to_current_scope(*resolvedDecl)) {
                    error = true;
                    continue;
                }
                declarations.emplace_back(std::move(resolvedDecl));
                continue;
            }
        }
    }
    if (error) return nullptr;
    auto modDecl =
        std::make_unique<ResolvedModuleDecl>(moduleDecl.location, moduleDecl.identifier, std::move(declarations));

    if (level == 0 && modDecl->identifier.find(".dmz") == std::string::npos) {
        m_modules_for_import.emplace(modDecl->identifier, modDecl.get());
    }
    return modDecl;
}

bool Sema::resolve_module_decl(const ModuleDecl &moduleDecl, ResolvedModuleDecl &resolvedModuleDecl) {
    m_actualModule = &resolvedModuleDecl;
    // println("resolve_module_decl New actual module: "<<m_actualModule->identifier);
    auto resolvedDecls = resolve_in_module_decl(moduleDecl.declarations, std::move(resolvedModuleDecl.declarations));
    resolvedModuleDecl.declarations = std::move(resolvedDecls);

    return true;
}

bool Sema::resolve_module_body(ResolvedModuleDecl &moduleDecl) {
    debug_func(moduleDecl.location);
    m_actualModule = &moduleDecl;
    // println("resolve_module_body New actual module: "<<m_actualModule->identifier);
    return resolve_in_module_body(moduleDecl.declarations);
}

std::vector<std::unique_ptr<ResolvedDecl>> Sema::resolve_in_module_decl(
    const std::vector<std::unique_ptr<Decl>> &decls, std::vector<std::unique_ptr<ResolvedDecl>> alreadyResolved) {
    debug_func("Decls " << decls.size() << " Already resolved " << alreadyResolved.size());
    bool error = false;
    std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree;
    std::unordered_map<ModuleDecl *, ResolvedModuleDecl *> map_modules;
    ScopeRAII moduleScope(*this);
    // Resolve every struct first so that functions have access to them in their signature.
    {
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
            if (auto *st = dynamic_cast<ModuleDecl *>(decl.get())) {
                std::unique_ptr<ResolvedModuleDecl> resolvedDecl = nullptr;
                auto it = std::find_if(alreadyResolved.begin(), alreadyResolved.end(),
                                       [id = decl->identifier](std::unique_ptr<ResolvedDecl> &decl) {
                                           return decl && decl->identifier == id;
                                       });
                if (it != alreadyResolved.end()) {
                    if (auto ptrModDecl = dynamic_cast<ResolvedModuleDecl *>((*it).get())) {
                        (*it).release();
                        resolvedDecl = std::unique_ptr<ResolvedModuleDecl>(ptrModDecl);
                    } else {
                        report((*it)->location, "unexpected identifier with module name");
                        error = true;
                        continue;
                    }
                } else {
                    resolvedDecl = resolve_module(*st, 0);
                }
                if (!resolvedDecl || !insert_decl_to_current_scope(*resolvedDecl)) {
                    error = true;
                    continue;
                }
                map_modules.emplace(st, resolvedDecl.get());

                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }
            if (dynamic_cast<DeclStmt *>(decl.get()) || dynamic_cast<FuncDecl *>(decl.get()) ||
                dynamic_cast<TestDecl *>(decl.get())) {
                continue;
            }
            decl->dump();
            dmz_unreachable("TODO: unexpected declaration");
        }
    }

    if (error) return {};

    {
        for (auto &&decl : decls) {
            if (auto *st = dynamic_cast<ModuleDecl *>(decl.get())) {
                auto success = resolve_module_decl(*st, *map_modules[st]);
                if (!success) error = true;
                continue;
            }
        }
    }

    if (error) return {};

    {
        for (auto &&decl : decls) {
            if (const auto *fn = dynamic_cast<const FuncDecl *>(decl.get())) {
                if (auto resolvedDecl = resolve_function_decl(*fn);
                    resolvedDecl && insert_decl_to_current_scope(*resolvedDecl)) {
                    resolvedTree.emplace_back(std::move(resolvedDecl));
                    continue;
                }
                error = true;
                continue;
            }
            if (const auto *test = dynamic_cast<const TestDecl *>(decl.get())) {
                if (auto resolvedDecl = resolve_function_decl(*test->testFunction)) {
                    if (auto resolvedFunctionDecl_ptr = dynamic_cast<ResolvedFunctionDecl *>(resolvedDecl.get())) {
                        resolvedDecl.release();
                        std::unique_ptr<ResolvedFunctionDecl> resolvedFunctionDecl(resolvedFunctionDecl_ptr);
                        auto testDecl = std::make_unique<ResolvedTestDecl>(std::move(resolvedFunctionDecl));
                        if (testDecl && insert_decl_to_current_scope(*testDecl)) {
                            m_tests.emplace_back(testDecl.get());
                            resolvedTree.emplace_back(std::move(testDecl));
                            continue;
                        }
                    }
                }
                error = true;
                continue;
            }
            if (const auto *ds = dynamic_cast<const DeclStmt *>(decl.get())) {
                if (auto declStmt = resolve_decl_stmt(*ds)) {
                    resolvedTree.emplace_back(std::move(declStmt));
                } else {
                    error = true;
                }
                continue;
            }
            if (dynamic_cast<ModuleDecl *>(decl.get()) || dynamic_cast<StructDecl *>(decl.get())) {
                continue;
            }
            decl->dump();
            dmz_unreachable("TODO: unexpected declaration");
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
        if (auto *md = dynamic_cast<ResolvedModuleDecl *>(currentDecl)) {
            if (!insert_decl_to_current_scope(*md)) {
                error = true;
            }
            continue;
        }
        if (dynamic_cast<ResolvedTestDecl *>(currentDecl)) {
            continue;
        }
        currentDecl->dump();
        dmz_unreachable("TODO: unexpected declaration");
    }
    {
        for (auto &&currentDeclRef : decls) {
            auto currentDecl = currentDeclRef.get();

            if (auto *md = dynamic_cast<ResolvedModuleDecl *>(currentDecl)) {
                if (!resolve_module_body(*md)) {
                    debug_msg("error resolve_in_module_body");
                    error = true;
                }
                continue;
            }
            if (auto *st = dynamic_cast<ResolvedStructDecl *>(currentDecl)) {
                if (!resolve_struct_members(*st)) {
                    debug_msg("error resolve_struct_members");
                    error = true;
                }
                continue;
            }
            if (auto *fn = dynamic_cast<ResolvedFunctionDecl *>(currentDecl)) {
                if (resolve_builtin_function(*fn)) continue;
                if (fn->genericTypes) continue;

                if (!resolve_func_body(*fn, *fn->functionDecl->body)) {
                    debug_msg("error resolve_func_body");
                    error = true;
                }
                continue;
            }
            if (auto *testDecl = dynamic_cast<ResolvedTestDecl *>(currentDecl)) {
                if (!resolve_func_body(*testDecl->testFunction, *testDecl->testFunction->functionDecl->body)) {
                    debug_msg("error test resolve_func_body");
                    error = true;
                }
                continue;
            }
            if (dynamic_cast<ResolvedDeclStmt *>(currentDecl) ||
                dynamic_cast<ResolvedExternFunctionDecl *>(currentDecl)) {
                continue;
            }
            currentDecl->dump();
            dmz_unreachable("TODO: unexpected declaration");
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

bool Sema::resolve_builtin_function(const ResolvedFunctionDecl &fnDecl) {
    m_currentFunction = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    defer([this]() { m_currentFunction = nullptr; });
    if (fnDecl.identifier == "@builtin_test_num") {
        resolve_builtin_test_num(fnDecl);
        return true;
    }
    if (fnDecl.identifier == "@builtin_test_name") {
        resolve_builtin_test_name(fnDecl);
        return true;
    }
    if (fnDecl.identifier == "@builtin_test_run") {
        resolve_builtin_test_run(fnDecl);
        return true;
    }
    return false;
}

void Sema::resolve_builtin_test_num(const ResolvedFunctionDecl &fnDecl) {
    SourceLocation loc{.file_name = "builtin"};
    auto test_num = std::make_unique<ResolvedIntLiteral>(loc, m_tests.size());
    auto retStmt = std::make_unique<ResolvedReturnStmt>(loc, std::move(test_num),
                                                        std::vector<std::unique_ptr<DMZ::ResolvedDeferRefStmt>>{});
    std::vector<std::unique_ptr<ResolvedStmt>> blockStmts;
    blockStmts.emplace_back(std::move(retStmt));

    auto body = std::make_unique<ResolvedBlock>(loc, std::move(blockStmts),
                                                std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});

    auto mutfnDecl = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    mutfnDecl->body = std::move(body);
}

void Sema::resolve_builtin_test_name(const ResolvedFunctionDecl &fnDecl) {
    // Begin Body
    SourceLocation loc{.file_name = "builtin"};
    auto cond = std::make_unique<ResolvedDeclRefExpr>(loc, *fnDecl.params[0], fnDecl.params[0]->type);

    auto elseName = std::make_unique<ResolvedStringLiteral>(loc, "Error in builtin_test_name");
    auto retStmt = std::make_unique<ResolvedReturnStmt>(loc, std::move(elseName),
                                                        std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});
    std::vector<std::unique_ptr<ResolvedStmt>> retBlockStmts;
    retBlockStmts.emplace_back(std::move(retStmt));

    auto elseBlock = std::make_unique<ResolvedBlock>(loc, std::move(retBlockStmts),
                                                     std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});
    std::vector<std::unique_ptr<ResolvedCaseStmt>> cases;
    for (size_t i = 0; i < m_tests.size(); i++) {
        auto test_name = std::make_unique<ResolvedStringLiteral>(loc, m_tests[i]->identifier);
        auto retStmt = std::make_unique<ResolvedReturnStmt>(loc, std::move(test_name),
                                                            std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});
        std::vector<std::unique_ptr<ResolvedStmt>> retBlockStmts;
        retBlockStmts.emplace_back(std::move(retStmt));
        auto retBlock = std::make_unique<ResolvedBlock>(loc, std::move(retBlockStmts),
                                                        std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});
        auto caseCondition = std::make_unique<ResolvedIntLiteral>(loc, i);
        auto caseStmt = std::make_unique<ResolvedCaseStmt>(loc, std::move(caseCondition), std::move(retBlock));
        cases.emplace_back(std::move(caseStmt));
    }

    auto switchStmt =
        std::make_unique<ResolvedSwitchStmt>(loc, std::move(cond), std::move(cases), std::move(elseBlock));
    std::vector<std::unique_ptr<ResolvedStmt>> blockStmts;
    blockStmts.emplace_back(std::move(switchStmt));

    auto body = std::make_unique<ResolvedBlock>(loc, std::move(blockStmts),
                                                std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});

    // End body
    auto mutfnDecl = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    mutfnDecl->body = std::move(body);
}

void Sema::resolve_builtin_test_run(const ResolvedFunctionDecl &fnDecl) {
    // Begin Body
    SourceLocation loc{.file_name = "builtin"};
    auto cond = std::make_unique<ResolvedDeclRefExpr>(loc, *fnDecl.params[0], fnDecl.params[0]->type);

    auto elseBlock = std::make_unique<ResolvedBlock>(loc, std::vector<std::unique_ptr<ResolvedStmt>>{},
                                                     std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});
    std::vector<std::unique_ptr<ResolvedCaseStmt>> cases;
    for (size_t i = 0; i < m_tests.size(); i++) {
        add_dependency(m_tests[i]->testFunction.get());
        auto test_call = std::make_unique<ResolvedCallExpr>(loc, *m_tests[i]->testFunction,
                                                            std::vector<std::unique_ptr<ResolvedExpr>>{});
        auto tryExpr = std::make_unique<ResolvedTryErrorExpr>(loc, std::move(test_call),
                                                              std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});

        std::vector<std::unique_ptr<ResolvedStmt>> caseBlockStmts;
        caseBlockStmts.emplace_back(std::move(tryExpr));
        auto caseBlock = std::make_unique<ResolvedBlock>(loc, std::move(caseBlockStmts),
                                                         std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});
        auto caseCondition = std::make_unique<ResolvedIntLiteral>(loc, i);
        auto caseStmt = std::make_unique<ResolvedCaseStmt>(loc, std::move(caseCondition), std::move(caseBlock));
        cases.emplace_back(std::move(caseStmt));
    }

    auto switchStmt =
        std::make_unique<ResolvedSwitchStmt>(loc, std::move(cond), std::move(cases), std::move(elseBlock));
    std::vector<std::unique_ptr<ResolvedStmt>> blockStmts;
    blockStmts.emplace_back(std::move(switchStmt));

    auto body = std::make_unique<ResolvedBlock>(loc, std::move(blockStmts),
                                                std::vector<std::unique_ptr<ResolvedDeferRefStmt>>{});

    // End body
    auto mutfnDecl = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    mutfnDecl->body = std::move(body);
}
}  // namespace DMZ