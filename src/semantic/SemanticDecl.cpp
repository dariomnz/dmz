
// #define DEBUG
#include "Stats.hpp"
#include "semantic/Semantic.hpp"

namespace DMZ {

ptr<ResolvedParamDecl> Sema::resolve_param_decl(const ParamDecl &param) {
    debug_func(param.location);
    auto type = resolve_type(*param.type);

    if (!param.isVararg)
        if (!type || dynamic_cast<const ResolvedTypeVoid *>(type.get()))
            return report(param.location,
                          "parameter '" + param.identifier + "' has invalid '" + param.type->to_str() + "' type");
    return makePtr<ResolvedParamDecl>(param.location, param.identifier, std::move(type), param.isMutable,
                                      param.isVararg);
}

ptr<ResolvedGenericTypeDecl> Sema::resolve_generic_type_decl(const GenericTypeDecl &genericTypeDecl) {
    debug_func(genericTypeDecl.location);
    return makePtr<ResolvedGenericTypeDecl>(genericTypeDecl.location, genericTypeDecl.identifier);
}

std::vector<ptr<ResolvedGenericTypeDecl>> Sema::resolve_generic_types_decl(
    const std::vector<ptr<GenericTypeDecl>> &genericTypesDecl) {
    debug_func("");
    std::vector<ptr<ResolvedGenericTypeDecl>> resolvedTypes;
    resolvedTypes.reserve(genericTypesDecl.size());
    for (size_t i = 0; i < genericTypesDecl.size(); i++) {
        auto resolvedGenericType = resolve_generic_type_decl(*genericTypesDecl[i]);
        if (!resolvedGenericType || !insert_decl_to_current_scope(*resolvedGenericType)) return {};
        resolvedTypes.emplace_back(std::move(resolvedGenericType));
    }
    return resolvedTypes;
}

ptr<ResolvedMemberFunctionDecl> Sema::resolve_member_function_decl(const ResolvedStructDecl &structDecl,
                                                                   const MemberFunctionDecl &function) {
    debug_func(function.location);

    varOrReturn(resolvedFunc, resolve_function_decl(function));
    varOrReturn(resolvedFunction, dynamic_cast<ResolvedFunctionDecl *>(resolvedFunc.get()));

    resolvedFunc.release();
    ptr<ResolvedFunctionDecl> resolvedFunctionDecl(resolvedFunction);

    SourceLocation loc{};
    if (!function.isStatic) {
        auto selfParam =
            makePtr<ResolvedParamDecl>(loc, "", makePtr<ResolvedTypePointer>(loc, structDecl.type->clone()), false);
        resolvedFunctionDecl->params.insert(resolvedFunctionDecl->params.begin(), std::move(selfParam));
    }

    return makePtr<ResolvedMemberFunctionDecl>(
        resolvedFunctionDecl->location, resolvedFunctionDecl->identifier, std::move(resolvedFunctionDecl->type),
        std::move(resolvedFunctionDecl->params), resolvedFunctionDecl->functionDecl,
        std::move(resolvedFunctionDecl->body), &structDecl, function.isStatic);
}

ptr<ResolvedFuncDecl> Sema::resolve_function_decl(const FuncDecl &function) {
    debug_func(function.location);

    ScopeRAII paramScope(*this);
    std::vector<ptr<ResolvedGenericTypeDecl>> resolvedGenericTypeDecl;
    std::vector<ResolvedDecl *> collectedScope;
    if (auto func = dynamic_cast<const GenericFunctionDecl *>(&function)) {
        if (func->genericTypes.size() != 0) {
            resolvedGenericTypeDecl = resolve_generic_types_decl(func->genericTypes);
            if (resolvedGenericTypeDecl.size() == 0) return nullptr;
            collectedScope = collect_scope();
        }
    }

    auto type = resolve_type(*function.type);

    if (!type)
        return report(function.location,
                      "function '" + function.identifier + "' has invalid '" + function.type->to_str() + "' type");

    if (function.identifier == "main") {
        if (!dynamic_cast<const ResolvedTypeVoid *>(type.get()))
            return report(function.location, "'main' function is expected to have 'void' type");

        if (!function.params.empty())
            return report(function.location, "'main' function is expected to take no arguments");
    }

    std::vector<ptr<ResolvedParamDecl>> resolvedParams;

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
        return makePtr<ResolvedExternFunctionDecl>(function.location, function.identifier, std::move(type),
                                                   std::move(resolvedParams));
    }
    if (auto functionDecl = dynamic_cast<const FunctionDecl *>(&function)) {
        if (dynamic_cast<const TestDecl *>(&function)) {
            return makePtr<ResolvedTestDecl>(function.location, function.identifier, functionDecl, nullptr);
        }
        if (resolvedGenericTypeDecl.size() != 0) {
            return makePtr<ResolvedGenericFunctionDecl>(function.location, function.identifier, std::move(type),
                                                        std::move(resolvedParams), functionDecl, nullptr,
                                                        std::move(resolvedGenericTypeDecl), std::move(collectedScope));
        } else {
            return makePtr<ResolvedFunctionDecl>(function.location, function.identifier, std::move(type),
                                                 std::move(resolvedParams), functionDecl, nullptr);
        }
    }
    function.dump();
    dmz_unreachable("unexpected function");
}

ResolvedSpecializedFunctionDecl *Sema::specialize_generic_function(const SourceLocation &location,
                                                                   ResolvedGenericFunctionDecl &funcDecl,
                                                                   const ResolvedTypeSpecialized &genericTypes) {
    debug_func(funcDecl.location);
    if (funcDecl.genericTypeDecls.size() != genericTypes.specializedTypes.size()) {
        return report(location, "unexpected number of specializations, expected " +
                                    std::to_string(funcDecl.genericTypeDecls.size()) + " actual " +
                                    std::to_string(genericTypes.specializedTypes.size()));
    }
    for (auto &gt : genericTypes.specializedTypes) {
        auto res = re_resolve_type(*gt);
        if (!res) return report(gt->location, "cannot resolve type of " + gt->to_str());
        *gt = std::move(*res);
    }
    // // Not specialize if generic types are no specialized
    for (auto &gt : genericTypes.specializedTypes) {
        if (dynamic_cast<const ResolvedTypeGeneric *>(gt.get())) {
            debug_msg("Not specialize generic types are no specialized");
            return nullptr;
        }
    }
    // Search if is specified
    for (auto &&func : funcDecl.specializations) {
        if (genericTypes.equal(*func->specializedTypes)) {
            add_dependency(func.get());
            return func.get();
        }
    }

    // If not found specialize the function
    for (size_t i = 0; i < funcDecl.genericTypeDecls.size(); i++) {
        debug_msg("Specialize " << funcDecl.genericTypeDecls[i]->identifier << " to "
                                << genericTypes.specializedTypes[i]->to_str());
        funcDecl.genericTypeDecls[i]->specializedType = genericTypes.specializedTypes[i]->clone();
    }
    // Restore scope
    // auto savedScope = std::move(m_scopes);
    // defer([&]() { m_scopes = std::move(savedScope); });
    ScopeRAII restoreScope(*this);
    for (auto &&decl : funcDecl.scopeToSpecialize) {
        m_scopes.back().emplace(decl->identifier, decl);
    }

    ScopeRAII functionScope(*this);

    auto type = resolve_type(*funcDecl.functionDecl->type);

    if (!type)
        return report(funcDecl.location,
                      "function '" + funcDecl.identifier + "' has invalid '" + funcDecl.type->to_str() + "' type");

    std::vector<ptr<ResolvedParamDecl>> resolvedParams;

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

    auto resolvedFunc = makePtr<ResolvedSpecializedFunctionDecl>(
        funcDecl.location, funcDecl.identifier, std::move(type), std::move(resolvedParams), funcDecl.functionDecl,
        nullptr, castPtr<ResolvedTypeSpecialized>(genericTypes.clone()));
    // auto &retFunc = resolvedFunc;
    auto &retFunc = funcDecl.specializations.emplace_back(std::move(resolvedFunc));

    bool error = false;
    auto prevFunc = m_currentFunction;
    m_currentFunction = retFunc.get();
    auto body = funcDecl.functionDecl->body.get();
    if (auto resolvedBody = resolve_block(*body)) {
        retFunc->body = std::move(resolvedBody);
        if (run_flow_sensitive_checks(*retFunc)) error = true;
    } else {
        error = true;
    }
    m_currentFunction = prevFunc;
    // println("m_currentFunction " << m_currentFunction->identifier << " prev " << prevFunc->identifier);
    if (error) return nullptr;
    add_dependency(retFunc.get());
    return retFunc.get();
}

ResolvedSpecializedStructDecl *Sema::specialize_generic_struct(const SourceLocation &location,
                                                               ResolvedGenericStructDecl &struDecl,
                                                               const ResolvedTypeSpecialized &genericTypes) {
    debug_func(struDecl.location << " " << genericTypes.to_str());
    if (struDecl.genericTypeDecls.size() != genericTypes.specializedTypes.size()) {
        return report(location, "unexpected number of specializations, expected " +
                                    std::to_string(struDecl.genericTypeDecls.size()) + " actual " +
                                    std::to_string(genericTypes.specializedTypes.size()));
    }
    for (auto &gt : genericTypes.specializedTypes) {
        auto res = re_resolve_type(*gt);
        if (!res) return report(gt->location, "cannot resolve type of " + gt->to_str());
        *gt = std::move(*res);
    }

    // // Not specialize if generic types are no specialized
    for (auto &gt : genericTypes.specializedTypes) {
        if (dynamic_cast<const ResolvedTypeGeneric *>(gt.get())) {
            debug_msg("Not specialize generic types are no specialized");
            return nullptr;
        }
    }
    // Search if is specified
    for (auto &&stru : struDecl.specializations) {
        if (genericTypes.equal(*stru->specializedTypes)) {
            add_dependency(stru.get());
            return stru.get();
        }
    }

    // If not found specialize the function
    for (size_t i = 0; i < struDecl.genericTypeDecls.size(); i++) {
        debug_msg("Specialize " << struDecl.genericTypeDecls[i]->identifier << " to "
                                << genericTypes.specializedTypes[i]->to_str());
        struDecl.genericTypeDecls[i]->specializedType = genericTypes.specializedTypes[i]->clone();
    }
    // Restore scope
    // auto savedScope = std::move(m_scopes);
    // defer([&]() { m_scopes = std::move(savedScope); });
    ScopeRAII restoreScope(*this);
    for (auto &&decl : struDecl.scopeToSpecialize) {
        debug_msg("Restore to scope " << decl->identifier);
        m_scopes.back().emplace(decl->identifier, decl);
    }
    ScopeRAII structScope(*this);

    std::vector<ptr<ResolvedFieldDecl>> resolvedFields;
    std::vector<ptr<ResolvedMemberFunctionDecl>> resolvedFunctions;

    auto resolvedStruct = makePtr<ResolvedSpecializedStructDecl>(
        struDecl.location, struDecl.identifier, struDecl.structDecl, struDecl.isPacked, std::move(resolvedFields),
        std::move(resolvedFunctions), castPtr<ResolvedTypeSpecialized>(genericTypes.clone()));

    auto prevStruct = m_currentStruct;
    m_currentStruct = resolvedStruct.get();
    defer([&]() { m_currentStruct = prevStruct; });

    unsigned idx = 0;
    for (auto &&field : struDecl.fields) {
        auto type = re_resolve_type(*field->type);
        if (!type) return report(field->location, "cannot resolve '" + field->type->to_str() + "' type");
        resolvedFields.emplace_back(
            makePtr<ResolvedFieldDecl>(field->location, field->identifier, std::move(type), idx++));
    }

    for (auto &&function : struDecl.functions) {
        if (auto memFunc = dynamic_cast<const MemberFunctionDecl *>(function->functionDecl)) {
            varOrReturn(func, resolve_member_function_decl(*resolvedStruct, *memFunc));
            resolvedFunctions.emplace_back(std::move(func));
        } else {
            return report(function->location, "internal error, expected member function decl");
        }
    }

    resolvedStruct->fields = std::move(resolvedFields);
    resolvedStruct->functions = std::move(resolvedFunctions);

    for (auto &&func : resolvedStruct->functions) {
        if (!resolve_func_body(*func, *func->functionDecl->body)) return nullptr;
    }

    if (!resolve_struct_members(*resolvedStruct)) return nullptr;

    auto &retStruct = struDecl.specializations.emplace_back(std::move(resolvedStruct));
    retStruct->specializedTypes = castPtr<ResolvedTypeSpecialized>(genericTypes.clone());
    add_dependency(retStruct.get());
    return retStruct.get();
}

ptr<ResolvedVarDecl> Sema::resolve_var_decl(const VarDecl &varDecl) {
    debug_func(varDecl.location);
    if (!varDecl.type && !varDecl.initializer)
        return report(varDecl.location, "an uninitialized variable is expected to have a type specifier");

    ptr<ResolvedExpr> resolvedInitializer = nullptr;
    if (varDecl.initializer) {
        resolvedInitializer = resolve_expr(*varDecl.initializer);
        if (!resolvedInitializer) return nullptr;
    }
    ResolvedType *type = nullptr;
    ptr<ResolvedType> resolvedvarType = nullptr;
    if (varDecl.type) {
        resolvedvarType = resolve_type(*varDecl.type);
        if (!resolvedvarType) {
            return report(varDecl.location,
                          "variable '" + varDecl.identifier + "' has invalid '" + varDecl.type->to_str() + "' type");
        }
    }
    if (!resolvedInitializer) {
        type = resolvedvarType.get();
    } else {
        type = resolvedInitializer->type.get();
        if (!type) {
            return report(varDecl.location, "variable '" + varDecl.identifier + "' has invalid '" +
                                                resolvedInitializer->type->to_str() + "' type");
        }

        if (varDecl.type) {
            bool shouldCheckType = true;

            if (dynamic_cast<ResolvedArrayInstantiationExpr *>(resolvedInitializer.get())) {
                if (auto arrType = dynamic_cast<ResolvedTypeArray *>(resolvedInitializer->type.get())) {
                    if (auto arrInnerType = dynamic_cast<ResolvedTypeVoid *>(arrType->arrayType.get())) {
                        resolvedInitializer->type = resolvedvarType->clone();
                        auto rarrType = dynamic_cast<ResolvedTypeArray *>(resolvedInitializer->type.get());
                        if (!rarrType) dmz_unreachable("unexpected error");
                        rarrType->arraySize = 0;
                        shouldCheckType = false;
                    }
                }
            }
            if (shouldCheckType) {
                if (!resolvedvarType->compare(*resolvedInitializer->type)) {
                    resolvedvarType->dump();
                    resolvedInitializer->type->dump();
                    return report(resolvedInitializer->location, "initializer type mismatch expected '" +
                                                                     resolvedvarType->to_str() + "' actual '" +
                                                                     resolvedInitializer->type->to_str() + "'");
                }
            }
            type = resolvedvarType.get();
        }

        resolvedInitializer->set_constant_value(cee.evaluate(*resolvedInitializer, false));
    }

    if (dynamic_cast<const ResolvedTypeVoid *>(type)) {
        return report(varDecl.location,
                      "variable '" + varDecl.identifier + "' has invalid '" + type->to_str() + "' type");
    }

    return makePtr<ResolvedVarDecl>(varDecl.location, varDecl.identifier, type->clone(), varDecl.isMutable,
                                    std::move(resolvedInitializer));
}

ptr<ResolvedStructDecl> Sema::resolve_struct_decl(const StructDecl &structDecl) {
    debug_func(structDecl.location);
    std::set<std::string_view> identifiers;

    ScopeRAII fieldScope(*this);
    ptr<ResolvedStructDecl> resStructDecl;
    if (auto genstruct = dynamic_cast<const GenericStructDecl *>(&structDecl)) {
        auto resolvedGenericTypesDecl = resolve_generic_types_decl(genstruct->genericTypes);
        if (resolvedGenericTypesDecl.size() == 0) return nullptr;
        resStructDecl = makePtr<ResolvedGenericStructDecl>(structDecl.location, structDecl.identifier, &structDecl,
                                                           structDecl.isPacked, std::vector<ptr<ResolvedFieldDecl>>{},
                                                           std::vector<ptr<ResolvedMemberFunctionDecl>>{},
                                                           std::move(resolvedGenericTypesDecl), collect_scope());
    } else {
        resStructDecl = makePtr<ResolvedStructDecl>(structDecl.location, structDecl.identifier, &structDecl,
                                                    structDecl.isPacked, std::vector<ptr<ResolvedFieldDecl>>{},
                                                    std::vector<ptr<ResolvedMemberFunctionDecl>>{});
    }

    // unsigned idx = 0;
    for (auto &&field : structDecl.fields) {
        if (!identifiers.emplace(field->identifier).second)
            return report(field->location, "field '" + field->identifier + "' is already declared");
    }

    for (auto &&function : structDecl.functions) {
        if (!identifiers.emplace(function->identifier).second)
            return report(function->location, "function '" + function->identifier + "' is already declared");
    }

    return resStructDecl;
}

bool Sema::resolve_struct_members(ResolvedStructDecl &resolvedStructDecl) {
    debug_func(resolvedStructDecl.location);

    auto prevStruct = m_currentStruct;
    m_currentStruct = &resolvedStructDecl;
    defer([&]() { m_currentStruct = prevStruct; });

    std::stack<std::pair<ResolvedStructDecl *, std::set<const ResolvedStructDecl *>>> worklist;
    worklist.push({&resolvedStructDecl, {}});

    while (!worklist.empty()) {
        auto [currentDecl, visited] = worklist.top();
        worklist.pop();

        ScopeRAII fieldScope(*this);
        if (auto genStruct = dynamic_cast<ResolvedGenericStructDecl *>(&resolvedStructDecl)) {
            for (auto &&genType : genStruct->genericTypeDecls) {
                if (!insert_decl_to_current_scope(*genType)) dmz_unreachable("unexpected error");
            }
        }

        if (!visited.emplace(currentDecl).second) {
            report(currentDecl->location, "struct '" + currentDecl->identifier + "' contains itself");
            return false;
        }
        // size_t idx = 0;
        for (auto &&field : currentDecl->fields) {
            auto type = re_resolve_type(*field->type);
            if (!type) return false;

            if (dynamic_cast<const ResolvedTypeVoid *>(type.get())) {
                report(field->location, "struct field cannot be void");
                return false;
            }

            if (auto struType = dynamic_cast<const ResolvedTypeStruct *>(type.get())) {
                worklist.push({struType->decl, visited});
            }
        }
    }

    if (dynamic_cast<ResolvedGenericStructDecl *>(&resolvedStructDecl)) return true;

    for (size_t i = 0; i < resolvedStructDecl.functions.size(); i++) {
        auto &func = resolvedStructDecl.structDecl->functions[i];
        auto &resfunc = resolvedStructDecl.functions[i];
        if (!resolve_func_body(*resfunc, *func->body)) return false;
    }

    return true;
}

bool Sema::resolve_struct_decl_funcs(ResolvedStructDecl &resolvedStructDecl) {
    debug_func("");

    auto prevStruct = m_currentStruct;
    m_currentStruct = &resolvedStructDecl;
    defer([&]() { m_currentStruct = prevStruct; });

    ScopeRAII structScope(*this);
    if (auto genstruct = dynamic_cast<const ResolvedGenericStructDecl *>(&resolvedStructDecl)) {
        for (auto &&gen : genstruct->genericTypeDecls) {
            if (!insert_decl_to_current_scope(*gen)) return false;
        }
    }

    std::vector<ptr<ResolvedFieldDecl>> resolvedFields;
    int idx = 0;
    for (auto &&field : resolvedStructDecl.structDecl->fields) {
        auto type = resolve_type(*field->type);
        if (!type) {
            report(field->type->location, "unexpected type '" + field->type->to_str() + "'");
            return false;
        }
        resolvedFields.emplace_back(
            std::make_unique<ResolvedFieldDecl>(field->location, field->identifier, std::move(type), idx++));
    }

    resolvedStructDecl.fields = std::move(resolvedFields);

    std::vector<ptr<ResolvedMemberFunctionDecl>> resolvedFunctions;
    for (auto &&function : resolvedStructDecl.structDecl->functions) {
        auto memberFunc = (resolve_member_function_decl(resolvedStructDecl, *function));
        if (!memberFunc) return false;
        resolvedFunctions.emplace_back(std::move(memberFunc));
    }

    resolvedStructDecl.functions = std::move(resolvedFunctions);

    return true;
}

ptr<ResolvedErrorGroupExprDecl> Sema::resolve_error_group_expr_decl(const ErrorGroupExprDecl &ErrorGroupExprDecl) {
    debug_func(ErrorGroupExprDecl.location);
    std::vector<ptr<ResolvedErrorDecl>> resolvedErrors;

    for (auto &&err : ErrorGroupExprDecl.errs) {
        resolvedErrors.emplace_back(makePtr<ResolvedErrorDecl>(err->location, err->identifier));
        // if (!insert_decl_to_current_scope(*ErrorDecl)) return nullptr;
        // if (!insert_decl_to_modules(*ErrorDecl)) return nullptr;
    }

    // println("Resolve error group with size " << resolvedErrors.size());
    return makePtr<ResolvedErrorGroupExprDecl>(ErrorGroupExprDecl.location, std::move(resolvedErrors));
}

ptr<ResolvedModuleDecl> Sema::resolve_module(const ModuleDecl &moduleDecl, int level) {
    debug_func(moduleDecl.location);
    std::vector<ptr<DMZ::ResolvedDecl>> declarations;
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
    auto modDecl = makePtr<ResolvedModuleDecl>(moduleDecl.location, moduleDecl.identifier, std::move(declarations));

    if (level == 0 && modDecl->identifier.find(".dmz") == std::string::npos) {
        m_modules_for_import.emplace(modDecl->identifier, modDecl.get());
    }
    return modDecl;
}

bool Sema::resolve_module_decl(const ModuleDecl &moduleDecl, ResolvedModuleDecl &resolvedModuleDecl) {
    auto prevModule = m_currentModule;
    m_currentModule = &resolvedModuleDecl;
    defer([&]() { m_currentModule = prevModule; });
    // println("resolve_module_decl New actual module: "<<m_currentModule->identifier);
    auto resolvedDecls = resolve_in_module_decl(moduleDecl.declarations, std::move(resolvedModuleDecl.declarations));
    resolvedModuleDecl.declarations = std::move(resolvedDecls);

    return true;
}

bool Sema::resolve_module_body(ResolvedModuleDecl &moduleDecl) {
    debug_func(moduleDecl.location);
    auto prevModule = m_currentModule;
    m_currentModule = &moduleDecl;
    defer([&]() { m_currentModule = prevModule; });
    // println("resolve_module_body New actual module: "<<m_currentModule->identifier);
    return resolve_in_module_body(moduleDecl.declarations);
}

std::vector<ptr<ResolvedDecl>> Sema::resolve_in_module_decl(const std::vector<ptr<Decl>> &decls,
                                                            std::vector<ptr<ResolvedDecl>> alreadyResolved) {
    debug_func("Decls " << decls.size() << " Already resolved " << alreadyResolved.size());
    bool error = false;
    std::vector<ptr<ResolvedDecl>> resolvedTree;
    std::unordered_map<ModuleDecl *, ResolvedModuleDecl *> map_modules;
    ScopeRAII moduleScope(*this);
    // Resolve every struct first so that functions have access to them in their signature.
    {
        for (auto &&decl : decls) {
            debug_msg(decl->identifier << " " << decl->location);
            if (const auto *st = dynamic_cast<const StructDecl *>(decl.get())) {
                ptr<ResolvedDecl> resolvedDecl = resolve_struct_decl(*st);

                if (!resolvedDecl || !insert_decl_to_current_scope(*resolvedDecl)) {
                    error = true;
                    continue;
                }

                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }
            if (auto *st = dynamic_cast<ModuleDecl *>(decl.get())) {
                ptr<ResolvedModuleDecl> resolvedDecl = nullptr;
                auto it = std::find_if(
                    alreadyResolved.begin(), alreadyResolved.end(),
                    [id = decl->identifier](ptr<ResolvedDecl> &decl) { return decl && decl->identifier == id; });
                if (it != alreadyResolved.end()) {
                    if (auto ptrModDecl = dynamic_cast<ResolvedModuleDecl *>((*it).get())) {
                        (*it).release();
                        resolvedDecl = ptr<ResolvedModuleDecl>(ptrModDecl);
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
        for (auto &&resDecl : resolvedTree) {
            if (auto *st = dynamic_cast<ResolvedStructDecl *>(resDecl.get())) {
                if (!resolve_struct_decl_funcs(*st)) {
                    error = true;
                    continue;
                }
            }
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
                    if (const auto *test = dynamic_cast<const ResolvedTestDecl *>(resolvedDecl.get())) {
                        m_tests.emplace_back(test);
                    }
                    resolvedTree.emplace_back(std::move(resolvedDecl));
                    continue;
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

bool Sema::resolve_in_module_body(const std::vector<ptr<ResolvedDecl>> &decls) {
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

    for (auto &&currentDeclRef : decls) {
        auto currentDecl = currentDeclRef.get();
        if (auto *fn = dynamic_cast<ResolvedGenericFunctionDecl *>(currentDecl)) {
            debug_msg("Collect scope for: " << fn->identifier);
            auto collectedScope = collect_scope();
            fn->scopeToSpecialize.insert(fn->scopeToSpecialize.end(), collectedScope.begin(), collectedScope.end());
        }
        if (auto *fn = dynamic_cast<ResolvedGenericStructDecl *>(currentDecl)) {
            debug_msg("Collect scope for: " << fn->identifier);
            auto collectedScope = collect_scope();
            fn->scopeToSpecialize.insert(fn->scopeToSpecialize.end(), collectedScope.begin(), collectedScope.end());
        }
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
                if (dynamic_cast<ResolvedGenericFunctionDecl *>(fn)) continue;

                if (!resolve_func_body(*fn, *fn->functionDecl->body)) {
                    debug_msg("error resolve_func_body");
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
    auto prevFunc = m_currentFunction;
    m_currentFunction = &function;
    defer([&]() { m_currentFunction = prevFunc; });
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
    auto prevFunc = m_currentFunction;
    m_currentFunction = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    defer([&]() { m_currentFunction = prevFunc; });
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
    auto test_num = makePtr<ResolvedIntLiteral>(loc, m_tests.size());
    auto retStmt = makePtr<ResolvedReturnStmt>(loc, std::move(test_num), std::vector<ptr<DMZ::ResolvedDeferRefStmt>>{});
    std::vector<ptr<ResolvedStmt>> blockStmts;
    blockStmts.emplace_back(std::move(retStmt));

    auto body = makePtr<ResolvedBlock>(loc, std::move(blockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{});

    auto mutfnDecl = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    mutfnDecl->body = std::move(body);
}

void Sema::resolve_builtin_test_name(const ResolvedFunctionDecl &fnDecl) {
    // Begin Body
    SourceLocation loc{.file_name = "builtin"};
    auto cond = makePtr<ResolvedDeclRefExpr>(loc, *fnDecl.params[0], fnDecl.params[0]->type->clone());

    auto elseName = makePtr<ResolvedStringLiteral>(loc, "Error in builtin_test_name");
    auto retStmt = makePtr<ResolvedReturnStmt>(loc, std::move(elseName), std::vector<ptr<ResolvedDeferRefStmt>>{});
    std::vector<ptr<ResolvedStmt>> retBlockStmts;
    retBlockStmts.emplace_back(std::move(retStmt));

    auto elseBlock = makePtr<ResolvedBlock>(loc, std::move(retBlockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{});
    std::vector<ptr<ResolvedCaseStmt>> cases;
    for (size_t i = 0; i < m_tests.size(); i++) {
        auto test_name = makePtr<ResolvedStringLiteral>(loc, m_tests[i]->identifier);
        auto retStmt = makePtr<ResolvedReturnStmt>(loc, std::move(test_name), std::vector<ptr<ResolvedDeferRefStmt>>{});
        std::vector<ptr<ResolvedStmt>> retBlockStmts;
        retBlockStmts.emplace_back(std::move(retStmt));
        auto retBlock = makePtr<ResolvedBlock>(loc, std::move(retBlockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{});
        auto caseCondition = makePtr<ResolvedIntLiteral>(loc, i);
        auto caseStmt = makePtr<ResolvedCaseStmt>(loc, std::move(caseCondition), std::move(retBlock));
        cases.emplace_back(std::move(caseStmt));
    }

    auto switchStmt = makePtr<ResolvedSwitchStmt>(loc, std::move(cond), std::move(cases), std::move(elseBlock));
    std::vector<ptr<ResolvedStmt>> blockStmts;
    blockStmts.emplace_back(std::move(switchStmt));

    auto body = makePtr<ResolvedBlock>(loc, std::move(blockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{});

    // End body
    auto mutfnDecl = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    mutfnDecl->body = std::move(body);
}

void Sema::resolve_builtin_test_run(const ResolvedFunctionDecl &fnDecl) {
    // Begin Body
    SourceLocation loc{.file_name = "builtin"};
    auto cond = makePtr<ResolvedDeclRefExpr>(loc, *fnDecl.params[0], fnDecl.params[0]->type->clone());

    auto elseBlock =
        makePtr<ResolvedBlock>(loc, std::vector<ptr<ResolvedStmt>>{}, std::vector<ptr<ResolvedDeferRefStmt>>{});
    std::vector<ptr<ResolvedCaseStmt>> cases;
    for (size_t i = 0; i < m_tests.size(); i++) {
        add_dependency(const_cast<ResolvedTestDecl *>(m_tests[i]));
        auto test_call = makePtr<ResolvedCallExpr>(loc, *m_tests[i], std::vector<ptr<ResolvedExpr>>{});
        auto funcType = dynamic_cast<const ResolvedTypeOptional *>(m_tests[i]->type.get());
        if (!funcType) dmz_unreachable("internal error, test type is not optional");
        auto tryExpr = makePtr<ResolvedTryErrorExpr>(loc, funcType->optionalType->clone(), std::move(test_call),
                                                     std::vector<ptr<ResolvedDeferRefStmt>>{});

        std::vector<ptr<ResolvedStmt>> caseBlockStmts;
        caseBlockStmts.emplace_back(std::move(tryExpr));
        auto caseBlock =
            makePtr<ResolvedBlock>(loc, std::move(caseBlockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{});
        auto caseCondition = makePtr<ResolvedIntLiteral>(loc, i);
        auto caseStmt = makePtr<ResolvedCaseStmt>(loc, std::move(caseCondition), std::move(caseBlock));
        cases.emplace_back(std::move(caseStmt));
    }

    auto switchStmt = makePtr<ResolvedSwitchStmt>(loc, std::move(cond), std::move(cases), std::move(elseBlock));
    std::vector<ptr<ResolvedStmt>> blockStmts;
    blockStmts.emplace_back(std::move(switchStmt));

    auto body = makePtr<ResolvedBlock>(loc, std::move(blockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{});

    // End body
    auto mutfnDecl = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    mutfnDecl->body = std::move(body);
}
}  // namespace DMZ