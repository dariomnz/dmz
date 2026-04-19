#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif
#include "Debug.hpp"
#include "Utils.hpp"
#include "semantic/Semantic.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

ptr<ResolvedParamDecl> Sema::resolve_param_decl(const ParamDecl &param) {
    debug_func(param.location);
    ptr<ResolvedType> type = nullptr;
    if (!param.isVararg) {
        type = resolve_type(*param.type);
    } else {
        type = makePtr<ResolvedTypeVarArg>(param.location);
    }

    if (!param.isVararg)
        if (!type || type->kind == ResolvedTypeKind::Void)
            return report(param.location,
                          "parameter '" + param.identifier + "' has invalid '" + param.type->to_str() + "' type");
    auto ret =
        makePtr<ResolvedParamDecl>(param.location, param.identifier, std::move(type), param.isMutable, param.isVararg);
    if (!param.isVararg) {
        ret->resolvedTypeExpr = resolve_expr(*param.type);
    }
    return ret;
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

ptr<ResolvedMemberFunctionDecl> Sema::resolve_member_function_decl(const ResolvedDecl &parentDecl,
                                                                   const MemberFunctionDecl &function) {
    debug_func(function.location);

    varOrReturn(resolvedFunc, resolve_function_decl(function));
    if (!dynamic_cast<ResolvedFunctionDecl *>(resolvedFunc.get())) {
        dmz_unreachable(function.location, "function is not a function");
    }

    ptr<ResolvedFunctionDecl> resolvedFunctionDecl = castPtr<ResolvedFunctionDecl>(std::move(resolvedFunc));

    bool isStatic = true;

    if (resolvedFunctionDecl->params.size() > 0) {
        ptr<ResolvedType> parentType = nullptr;
        if (auto sd = dynamic_cast<const ResolvedStructDecl *>(&parentDecl)) {
            parentType = makePtr<ResolvedTypeStruct>(function.location, const_cast<ResolvedStructDecl *>(sd));
        } else if (auto ud = dynamic_cast<const ResolvedUnionDecl *>(&parentDecl)) {
            parentType = makePtr<ResolvedTypeUnion>(function.location, const_cast<ResolvedUnionDecl *>(ud));
        }

        if (parentType) {
            auto paramType = makePtr<ResolvedTypePointer>(function.location, std::move(parentType));
            if (resolvedFunctionDecl->params[0]->type->equal(*paramType)) {
                isStatic = false;
            }
        }
    }

    auto ret = makePtr<ResolvedMemberFunctionDecl>(
        resolvedFunctionDecl->location, resolvedFunctionDecl->isPublic, resolvedFunctionDecl->identifier,
        std::move(resolvedFunctionDecl->type), std::move(resolvedFunctionDecl->params),
        std::move(resolvedFunctionDecl->scope), resolvedFunctionDecl->functionDecl, &parentDecl, isStatic);
    auto fnType = ret->getFnType();
    fnType->fnDecl = ret.get();
    ret->body = std::move(resolvedFunctionDecl->body);
    return ret;
}

ptr<ResolvedFuncDecl> Sema::resolve_function_decl(const FuncDecl &function) {
    debug_func(function.location);

    std::optional<ScopeRAII> genericFunctionScope = std::nullopt;
    ptr<ResolvedScope> takenGenericScope = nullptr;
    std::vector<ptr<ResolvedGenericTypeDecl>> resolvedGenericTypeDecl;
    if (auto func = dynamic_cast<const GenericFunctionDecl *>(&function)) {
        genericFunctionScope.emplace(*this);
        takenGenericScope = genericFunctionScope->takeScope();
        if (func->genericTypes.size() != 0) {
            resolvedGenericTypeDecl = resolve_generic_types_decl(func->genericTypes);
            if (resolvedGenericTypeDecl.size() == 0) return nullptr;
        }
    }

    ScopeRAII functionScope(*this);
    auto takenScope = functionScope.takeScope();

    auto returnType = resolve_type(*function.type);

    if (!returnType)
        return report(function.location, "function '" + function.identifier + "' has invalid '" +
                                             function.type->to_str() + "' return type");

    if (function.identifier == "main") {
        if (returnType->kind != ResolvedTypeKind::Void)
            return report(function.location, "'main' function is expected to have 'void' return type");

        if (!function.params.empty())
            return report(function.location, "'main' function is expected to take no arguments");
    }

    std::vector<ptr<ResolvedParamDecl>> resolvedParams;
    std::vector<ptr<ResolvedType>> resolvedParamsTypes;

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
        resolvedParamsTypes.emplace_back(resolvedParam->type->clone());
        resolvedParams.emplace_back(std::move(resolvedParam));
    }

    auto fnType = makePtr<ResolvedTypeFunction>(function.location, nullptr, std::move(resolvedParamsTypes),
                                                std::move(returnType));

    if (dynamic_cast<const ExternFunctionDecl *>(&function)) {
        auto ret =
            makePtr<ResolvedExternFunctionDecl>(function.location, function.isPublic, function.identifier,
                                                std::move(fnType), std::move(resolvedParams), std::move(takenScope));
        ret->getFnType()->fnDecl = ret.get();
        return ret;
    }
    if (auto functionDecl = dynamic_cast<const FunctionDecl *>(&function)) {
        if (dynamic_cast<const TestDecl *>(&function)) {
            auto ret =
                makePtr<ResolvedTestDecl>(function.location, function.identifier, functionDecl, std::move(takenScope));
            ret->getFnType()->fnDecl = ret.get();
            return ret;
        }
        if (resolvedGenericTypeDecl.size() != 0) {
            auto ret = makePtr<ResolvedGenericFunctionDecl>(
                function.location, function.isPublic, function.identifier, std::move(fnType), std::move(resolvedParams),
                std::move(takenScope), std::move(takenGenericScope), functionDecl, std::move(resolvedGenericTypeDecl),
                m_currentModule, m_currentStruct);
            ret->getFnType()->fnDecl = ret.get();
            return ret;
        } else {
            auto ret = makePtr<ResolvedFunctionDecl>(function.location, function.isPublic, function.identifier,
                                                     std::move(fnType), std::move(resolvedParams),
                                                     std::move(takenScope), functionDecl);
            ret->getFnType()->fnDecl = ret.get();
            return ret;
        }
    }
    function.dump();
    dmz_unreachable(function.location, "unexpected function");
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
    // Not specialize if generic types are no specialized
    for (auto &gt : genericTypes.specializedTypes) {
        if (gt->kind == ResolvedTypeKind::Generic) {
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
    defer([&]() {
        // Desespecialize the types for next iterations
        for (size_t i = 0; i < funcDecl.genericTypeDecls.size(); i++) {
            funcDecl.genericTypeDecls[i]->specializedType = nullptr;
        }
    });
    // Restore scope
    auto savedCurrentModule = std::move(m_currentModule);
    m_currentModule = funcDecl.saveCurrentModule;
    defer([&]() { m_currentModule = std::move(savedCurrentModule); });
    auto savedCurrentStruct = std::move(m_currentStruct);
    m_currentStruct = funcDecl.saveCurrentStruct;
    defer([&]() { m_currentStruct = std::move(savedCurrentStruct); });

    ScopeRAII parentFunctionScope(*this, funcDecl.genericScope.get());
    ScopeRAII functionScope(*this);
    auto takenScope = functionScope.takeScope();

    auto returnType = resolve_type(*funcDecl.functionDecl->type);

    if (!returnType)
        return report(funcDecl.location, "function '" + funcDecl.identifier + "' has invalid '" +
                                             funcDecl.type->to_str() + "' return type");

    std::vector<ptr<ResolvedParamDecl>> resolvedParams;
    std::vector<ptr<ResolvedType>> resolvedParamsTypes;

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
        resolvedParamsTypes.emplace_back(resolvedParam->type->clone());
        resolvedParams.emplace_back(std::move(resolvedParam));
    }

    auto fnType = makePtr<ResolvedTypeFunction>(funcDecl.location, nullptr, std::move(resolvedParamsTypes),
                                                std::move(returnType));

    auto resolvedFunc = makePtr<ResolvedSpecializedFunctionDecl>(
        funcDecl.location, funcDecl.isPublic, funcDecl.identifier, std::move(fnType), std::move(resolvedParams),
        std::move(takenScope), funcDecl.functionDecl, castPtr<ResolvedTypeSpecialized>(genericTypes.clone()));
    resolvedFunc->getFnType()->fnDecl = resolvedFunc.get();
    // auto &retFunc = resolvedFunc;
    auto *retFunc = funcDecl.specializations.emplace_back(std::move(resolvedFunc)).get();
    bool error = false;
    auto prevFunc = m_currentFunction;
    m_currentFunction = retFunc;
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
    add_dependency(retFunc);
    return retFunc;
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
        if (gt->kind == ResolvedTypeKind::Generic) {
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
    defer([&]() {
        // Desespecialize the types for next iterations
        for (size_t i = 0; i < struDecl.genericTypeDecls.size(); i++) {
            struDecl.genericTypeDecls[i]->specializedType = nullptr;
        }
    });
    // Restore scope
    auto savedCurrentModule = std::move(m_currentModule);
    m_currentModule = struDecl.saveCurrentModule;
    defer([&]() { m_currentModule = std::move(savedCurrentModule); });

    ScopeRAII parentStructScope(*this, struDecl.genericScope.get());
    ScopeRAII structScope(*this);

    std::vector<ptr<ResolvedFieldDecl>> resolvedFields;
    std::vector<ptr<ResolvedMemberFunctionDecl>> resolvedFunctions;

    auto resolvedStruct = makePtr<ResolvedSpecializedStructDecl>(
        struDecl.location, struDecl.isPublic, struDecl.identifier, struDecl.structDecl, struDecl.isPacked,
        std::move(resolvedFields), std::move(resolvedFunctions), structScope.takeScope(), &struDecl,
        castPtr<ResolvedTypeSpecialized>(genericTypes.clone()));

    auto *retStruct = struDecl.specializations.emplace_back(std::move(resolvedStruct)).get();
    retStruct->specializedTypes = castPtr<ResolvedTypeSpecialized>(genericTypes.clone());
    add_dependency(retStruct);

    auto prevStruct = m_currentStruct;
    m_currentStruct = retStruct;
    defer([&]() { m_currentStruct = prevStruct; });

    if (!resolve_struct_members(*retStruct)) return nullptr;
    if (!resolve_struct_decl_funcs(*retStruct)) return nullptr;
    m_pending_decls.emplace_back(retStruct);

    return retStruct;
}

ptr<ResolvedVarDecl> Sema::resolve_var_decl(const VarDecl &varDecl) {
    debug_func(varDecl.location);
    if (!varDecl.type && !varDecl.initializer)
        return report(varDecl.location, "an uninitialized variable is expected to have a type specifier");

    ptr<ResolvedType> resolvedvarType = nullptr;
    if (varDecl.type) {
        resolvedvarType = resolve_type(*varDecl.type);
        if (!resolvedvarType) {
            return report(varDecl.location,
                          "variable '" + varDecl.identifier + "' has invalid '" + varDecl.type->to_str() + "' type");
        }
    }
    if (resolvedvarType && resolvedvarType->kind == ResolvedTypeKind::Void) {
        return report(varDecl.location,
                      "variable '" + varDecl.identifier + "' has invalid '" + resolvedvarType->to_str() + "' type");
    }

    ptr<ResolvedType> type = resolvedvarType ? std::move(resolvedvarType) : nullptr;
    auto ret = makePtr<ResolvedVarDecl>(varDecl.location, &varDecl, varDecl.isPublic, varDecl.identifier,
                                        std::move(type), varDecl.isMutable, nullptr, varDecl.isGlobal);
    if (varDecl.type) {
        ret->resolvedTypeExpr = resolve_expr(*varDecl.type);
    }
    return ret;
}

bool Sema::resolve_var_decl_initialize(ResolvedVarDecl &varDecl) {
    debug_func(varDecl.location);
    if (varDecl.state == ResolvedState::FullyResolved) return debug_ret(true);
    if (varDecl.state == ResolvedState::InProgress && varDecl.nameResolved) return debug_ret(true);  // Cycle

    varDecl.state = ResolvedState::InProgress;
    defer([&]() { varDecl.nameResolved = true; });

    if (!varDecl.varDecl->initializer && !varDecl.type) {
        debug_msg(varDecl.varDecl);
        varDecl.varDecl->dump();
        dmz_unreachable(varDecl.location, "unexpected malformed var decl");
    }

    ptr<ResolvedExpr> resolvedInitializer = nullptr;
    if (varDecl.varDecl->initializer) {
        resolvedInitializer = resolve_expr(*varDecl.varDecl->initializer);
        if (!resolvedInitializer) return debug_ret(false);
    }

    ResolvedType *type = nullptr;
    if (!resolvedInitializer) {
        debug_msg("There are no initializer");
        type = varDecl.type.get();
    } else {
        type = resolvedInitializer->type.get();
        if (!type) {
            report(varDecl.location, "variable '" + varDecl.identifier + "' has invalid '" +
                                         resolvedInitializer->type->to_str() + "' type");
            return debug_ret(false);
        }

        if (varDecl.type) {
            bool shouldCheckType = true;

            if (dynamic_cast<ResolvedArrayInstantiationExpr *>(resolvedInitializer.get())) {
                if (auto arrType = dynamic_cast<ResolvedTypeArray *>(resolvedInitializer->type.get())) {
                    if (arrType->arrayType->kind == ResolvedTypeKind::Void) {
                        resolvedInitializer->type = varDecl.type->clone();
                        auto rarrType = dynamic_cast<ResolvedTypeArray *>(resolvedInitializer->type.get());
                        if (!rarrType) dmz_unreachable(resolvedInitializer->location, "unexpected error");
                        rarrType->arraySize = 0;
                        shouldCheckType = false;
                    }
                }
            }
            if (shouldCheckType) {
                perform_implicit_cast(resolvedInitializer, *varDecl.type);
                if (!varDecl.type->compare(*resolvedInitializer->type)) {
                    report(resolvedInitializer->location, "initializer type mismatch expected '" +
                                                              varDecl.type->to_str() + "' actual '" +
                                                              resolvedInitializer->type->to_str() + "'");
                    varDecl.state = ResolvedState::Unresolved;
                    return debug_ret(false);
                }
            }
            type = varDecl.type.get();
        }

        resolvedInitializer->set_constant_value(cee.evaluate(*resolvedInitializer, false));
    }

    if (type->kind == ResolvedTypeKind::Void) {
        report(varDecl.location, "variable '" + varDecl.identifier + "' has invalid '" + type->to_str() + "' type");
        varDecl.state = ResolvedState::Unresolved;
        return debug_ret(false);
    }

    if (!varDecl.type || !varDecl.type->equal(*type)) {
        varDecl.type = type->clone();
    }
    varDecl.initializer = std::move(resolvedInitializer);

    varDecl.state = ResolvedState::FullyResolved;
    return debug_ret(true);
}

bool Sema::resolve_union_members(ResolvedUnionDecl &resolvedUnionDecl) {
    debug_func(resolvedUnionDecl.location);

    auto prevStruct = m_currentStruct;
    m_currentStruct = &resolvedUnionDecl;
    defer([&]() { m_currentStruct = prevStruct; });

    if (!resolvedUnionDecl.fields.empty()) return true;

    std::vector<ptr<ResolvedFieldDecl>> resolvedFields;
    std::vector<std::string> resolvedFields_strs;
    int idx = 0;

    for (auto &&decl : resolvedUnionDecl.unionDecl()->decls) {
        auto field = dynamic_cast<const FieldDecl *>(decl.get());
        if (!field) continue;
        auto type = resolve_type(*field->type);
        if (!type) {
            report(field->type->location, "unexpected type '" + field->type->to_str() + "'");
            return false;
        }
        ptr<ResolvedExpr> default_initializer = nullptr;
        if (field->default_initializer) {
            default_initializer = resolve_expr(*field->default_initializer);
            if (!default_initializer) return false;
        }

        auto retField = std::make_unique<ResolvedFieldDecl>(field->location, field->identifier, std::move(type), idx++,
                                                            std::move(default_initializer));
        retField->resolvedTypeExpr = resolve_expr(*field->type);
        resolvedFields.emplace_back(std::move(retField));
        resolvedFields_strs.emplace_back(field->identifier);
    }

    resolvedUnionDecl.fields = std::move(resolvedFields);
    resolvedUnionDecl.fields_strs = std::move(resolvedFields_strs);

    return true;
}

ptr<ResolvedStructDecl> Sema::resolve_struct_decl(const StructDecl &structDecl) {
    debug_func(structDecl.location);
    if (m_resolvedStructs.contains(&structDecl))
        return report(structDecl.location, "struct '" + structDecl.identifier + "' is already declared");
    std::unordered_set<std::string> identifiers;

    std::optional<ScopeRAII> genericStructScope = std::nullopt;
    ptr<ResolvedScope> takenGenericScope = nullptr;
    std::vector<ptr<ResolvedGenericTypeDecl>> resolvedGenericTypesDecl;
    if (auto genstruct = dynamic_cast<const GenericStructDecl *>(&structDecl)) {
        genericStructScope.emplace(*this);
        takenGenericScope = genericStructScope->takeScope();
        resolvedGenericTypesDecl = resolve_generic_types_decl(genstruct->genericTypes);
        if (resolvedGenericTypesDecl.size() == 0) return nullptr;
    }
    ScopeRAII fieldScope(*this);
    auto takenFieldScope = fieldScope.takeScope();
    ptr<ResolvedStructDecl> resStructDecl;
    if (dynamic_cast<const GenericStructDecl *>(&structDecl)) {
        resStructDecl = makePtr<ResolvedGenericStructDecl>(
            structDecl.location, structDecl.isPublic, resolve_decl_name(structDecl.identifier), &structDecl,
            structDecl.isPacked, std::vector<ptr<ResolvedFieldDecl>>{}, std::vector<ptr<ResolvedMemberFunctionDecl>>{},
            std::move(takenFieldScope), std::move(takenGenericScope), std::move(resolvedGenericTypesDecl),
            m_currentModule);
    } else if (auto unionDecl = dynamic_cast<const UnionDecl *>(&structDecl)) {
        resStructDecl = makePtr<ResolvedUnionDecl>(
            unionDecl->location, unionDecl->isPublic, resolve_decl_name(unionDecl->identifier), unionDecl,
            unionDecl->isPacked, std::vector<ptr<ResolvedFieldDecl>>{}, std::vector<ptr<ResolvedMemberFunctionDecl>>{},
            std::move(takenFieldScope));
    } else {
        resStructDecl = makePtr<ResolvedStructDecl>(
            structDecl.location, structDecl.isPublic, resolve_decl_name(structDecl.identifier), &structDecl,
            structDecl.isPacked, std::vector<ptr<ResolvedFieldDecl>>{}, std::vector<ptr<ResolvedMemberFunctionDecl>>{},
            std::move(takenFieldScope));
    }

    debug_msg("structDecl.decls.size() " << structDecl.decls.size());
    for (auto &&decl : structDecl.decls) {
        debug_msg(decl->location << " " << decl->identifier);
        if (dynamic_cast<Decoration *>(decl.get())) continue;
        if (!identifiers.emplace(decl->identifier).second)
            return report(decl->location, "member '" + decl->identifier + "' is already declared");
    }

    m_resolvedStructs.emplace(&structDecl, resStructDecl.get());
    return resStructDecl;
}

bool Sema::resolve_struct_members(ResolvedStructDecl &resolvedStructDecl) {
    debug_func(resolvedStructDecl.location);

    if (auto unionStruct = dynamic_cast<ResolvedUnionDecl *>(&resolvedStructDecl)) {
        return resolve_union_members(*unionStruct);
    }

    auto prevStruct = m_currentStruct;
    m_currentStruct = &resolvedStructDecl;
    defer([&]() { m_currentStruct = prevStruct; });

    ResolvedScope *scopeToUse = resolvedStructDecl.scope.get();
    if (auto genStruct = dynamic_cast<ResolvedGenericStructDecl *>(&resolvedStructDecl)) {
        scopeToUse = genStruct->genericScope.get();
    }
    ScopeRAII structScope(*this, scopeToUse);

    std::optional<DeferAction> deferSpecializedType = std::nullopt;
    if (auto specStruct = dynamic_cast<const ResolvedSpecializedStructDecl *>(&resolvedStructDecl)) {
        for (size_t i = 0; i < specStruct->genStruct->genericTypeDecls.size(); i++) {
            debug_msg("Specialize " << specStruct->genStruct->genericTypeDecls[i]->identifier << " to "
                                    << specStruct->specializedTypes->specializedTypes[i]->to_str());
            specStruct->genStruct->genericTypeDecls[i]->specializedType =
                specStruct->specializedTypes->specializedTypes[i]->clone();
        }

        deferSpecializedType.emplace([&]() {
            // Reset specializedType
            for (size_t i = 0; i < specStruct->genStruct->genericTypeDecls.size(); i++) {
                specStruct->genStruct->genericTypeDecls[i]->specializedType = nullptr;
            }
        });
    }

    if (!resolvedStructDecl.structDecl) {
        // Tuples/Special cases might not have a structDecl
        return true;
    }

    if (resolvedStructDecl.fields.empty()) {
        std::vector<ptr<ResolvedFieldDecl>> resolvedFields;
        std::vector<std::string> resolvedFields_strs;
        int idx = 0;
        for (auto &&decl : resolvedStructDecl.structDecl->decls) {
            auto field = dynamic_cast<const FieldDecl *>(decl.get());
            if (!field) continue;
            auto type = resolve_type(*field->type);
            if (!type) {
                report(field->type->location, "unexpected type '" + field->type->to_str() + "'");
                return false;
            }
            ptr<ResolvedExpr> default_initizlizer = nullptr;
            if (field->default_initializer) {
                default_initizlizer = resolve_expr(*field->default_initializer);
                if (!default_initizlizer) return false;
            }

            auto retField = std::make_unique<ResolvedFieldDecl>(field->location, field->identifier, std::move(type),
                                                                idx++, std::move(default_initizlizer));
            retField->resolvedTypeExpr = resolve_expr(*field->type);
            resolvedFields.emplace_back(std::move(retField));
            resolvedFields_strs.emplace_back(field->identifier);
        }

        resolvedStructDecl.fields = std::move(resolvedFields);
        resolvedStructDecl.fields_strs = std::move(resolvedFields_strs);
    }

    std::stack<std::pair<ResolvedStructDecl *, std::set<const ResolvedStructDecl *>>> worklist;
    worklist.push({&resolvedStructDecl, {}});

    while (!worklist.empty()) {
        auto [currentDecl, visited] = worklist.top();
        worklist.pop();

        if (!visited.emplace(currentDecl).second) {
            report(currentDecl->location, "struct '" + currentDecl->identifier + "' contains itself");
            return false;
        }
        // size_t idx = 0;
        debug_msg("currentDecl->fields.size() " << currentDecl->fields.size());
        for (auto &&field : currentDecl->fields) {
            debug_msg(field->location << " " << field->identifier);
            auto type = re_resolve_type(*field->type);
            if (!type) return false;

            if (type->kind == ResolvedTypeKind::Void) {
                report(field->location, "struct field cannot be void");
                return false;
            }

            if (auto struType = dynamic_cast<const ResolvedTypeStruct *>(type.get())) {
                worklist.push({struType->decl, visited});
            }
        }
    }

    return true;
}

bool Sema::resolve_struct_body_funcs(ResolvedStructDecl &resolvedStructDecl) {
    auto prevStruct = m_currentStruct;
    m_currentStruct = &resolvedStructDecl;
    defer([&]() { m_currentStruct = prevStruct; });

    ResolvedScope *scopeToUse = resolvedStructDecl.scope.get();
    if (auto genstruct = dynamic_cast<const ResolvedGenericStructDecl *>(&resolvedStructDecl)) {
        scopeToUse = genstruct->genericScope.get();
    }
    ScopeRAII structScope(*this, scopeToUse);
    if (auto genstruct = dynamic_cast<const ResolvedGenericStructDecl *>(&resolvedStructDecl)) {
        auto prevModule = m_currentModule;
        m_currentModule = genstruct->saveCurrentModule;
        defer([&]() { m_currentModule = prevModule; });

        for (size_t i = 0; i < resolvedStructDecl.functions.size(); i++) {
            auto &resfunc = resolvedStructDecl.functions[i];
            if (!resolve_func_body(*resfunc, *resfunc->functionDecl->body)) return false;
        }

        for (auto &&spec : genstruct->specializations) {
            // Specialize types
            for (size_t i = 0; i < genstruct->genericTypeDecls.size(); i++) {
                debug_msg("Specialize " << genstruct->genericTypeDecls[i]->identifier << " to "
                                        << spec->specializedTypes->specializedTypes[i]->to_str());
                genstruct->genericTypeDecls[i]->specializedType = spec->specializedTypes->specializedTypes[i]->clone();
            }

            defer([&]() {
                // Reset specializedType
                for (size_t i = 0; i < genstruct->genericTypeDecls.size(); i++) {
                    genstruct->genericTypeDecls[i]->specializedType = nullptr;
                }
            });

            auto prevStruct = m_currentStruct;
            m_currentStruct = spec.get();
            defer([&]() { m_currentStruct = prevStruct; });

            // Resolve functions body
            for (size_t i = 0; i < spec->functions.size(); i++) {
                auto &resfunc = spec->functions[i];
                if (!resolve_func_body(*resfunc, *resfunc->functionDecl->body)) return false;
            }
        }
        // dmz_unreachable("TODO");
    } else {
        auto prevModule = m_currentModule;
        defer([&]() { m_currentModule = prevModule; });
        auto prevStruct = m_currentStruct;
        defer([&]() { m_currentStruct = prevStruct; });

        if (auto specStruct = dynamic_cast<ResolvedSpecializedStructDecl *>(&resolvedStructDecl)) {
            // Specialize types
            for (size_t i = 0; i < specStruct->genStruct->genericTypeDecls.size(); i++) {
                debug_msg("Specialize " << specStruct->genStruct->genericTypeDecls[i]->identifier << " to "
                                        << specStruct->specializedTypes->specializedTypes[i]->to_str());
                specStruct->genStruct->genericTypeDecls[i]->specializedType =
                    specStruct->specializedTypes->specializedTypes[i]->clone();
            }

            defer([&]() {
                // Reset specializedType
                for (size_t i = 0; i < specStruct->genStruct->genericTypeDecls.size(); i++) {
                    specStruct->genStruct->genericTypeDecls[i]->specializedType = nullptr;
                }
            });

            m_currentModule = specStruct->genStruct->saveCurrentModule;
            m_currentStruct = specStruct;
        }

        for (size_t i = 0; i < resolvedStructDecl.functions.size(); i++) {
            auto &resfunc = resolvedStructDecl.functions[i];
            if (!resolve_func_body(*resfunc, *resfunc->functionDecl->body)) return false;
        }
    }

    return true;
}

bool Sema::resolve_struct_decl_funcs(ResolvedStructDecl &resolvedStructDecl) {
    debug_func(resolvedStructDecl.location << " " << resolvedStructDecl.name());

    auto prevStruct = m_currentStruct;
    m_currentStruct = &resolvedStructDecl;
    defer([&]() { m_currentStruct = prevStruct; });
    ResolvedScope *scopeToUse = resolvedStructDecl.scope.get();
    if (auto genstruct = dynamic_cast<const ResolvedGenericStructDecl *>(&resolvedStructDecl)) {
        scopeToUse = genstruct->genericScope.get();
    }

    ScopeRAII structScope(*this, scopeToUse);

    std::optional<DeferAction> deferSpecializedType = std::nullopt;
    if (auto specStruct = dynamic_cast<const ResolvedSpecializedStructDecl *>(&resolvedStructDecl)) {
        for (size_t i = 0; i < specStruct->genStruct->genericTypeDecls.size(); i++) {
            debug_msg("Specialize " << specStruct->genStruct->genericTypeDecls[i]->identifier << " to "
                                    << specStruct->specializedTypes->specializedTypes[i]->to_str());
            specStruct->genStruct->genericTypeDecls[i]->specializedType =
                specStruct->specializedTypes->specializedTypes[i]->clone();
        }

        deferSpecializedType.emplace([&]() {
            // Reset specializedType
            for (size_t i = 0; i < specStruct->genStruct->genericTypeDecls.size(); i++) {
                specStruct->genStruct->genericTypeDecls[i]->specializedType = nullptr;
            }
        });
    }

    std::vector<ptr<ResolvedMemberFunctionDecl>> resolvedFunctions;
    std::vector<std::string> resolvedFunctions_strs;
    if (resolvedStructDecl.structDecl) {
        for (auto &&decl : resolvedStructDecl.structDecl->decls) {
            auto function = dynamic_cast<const MemberFunctionDecl *>(decl.get());
            if (!function) continue;
            auto memberFunc = (resolve_member_function_decl(resolvedStructDecl, *function));
            if (!memberFunc) return false;
            resolvedFunctions.emplace_back(std::move(memberFunc));
            resolvedFunctions_strs.emplace_back(function->identifier);
        }
    }

    resolvedStructDecl.functions = std::move(resolvedFunctions);
    resolvedStructDecl.functions_strs = std::move(resolvedFunctions_strs);

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

std::vector<ptr<ResolvedModuleDecl>> Sema::resolve_modules_decls(const std::vector<ptr<ModuleDecl>> &modules,
                                                                 const std::filesystem::path &sourcePath) {
    bool error = false;
    debug_func("error " << (error ? "true" : "false"));
    debug_msg("[Sema] resolve_modules_decls: resolving " << modules.size() << " modules");
    std::vector<ptr<ResolvedModuleDecl>> resolvedModules;

    // Pass 1: Create all empty module declarations
    for (auto &&module : modules) {
        module->module_path = std::filesystem::canonical(module->module_path);
        debug_msg("[Sema]   - module path: " << module->module_path);
        auto resolvedModule = resolve_module_decl(*module);
        if (!resolvedModule) {
            error = true;
            continue;
        }
        auto *resMod = resolvedModules.emplace_back(std::move(resolvedModule)).get();
        m_modules_for_import.emplace(module->module_path, resMod);
    }
    if (error) return {};

    // Pass 2: Discover all declarations (register names, lazy resolution)
    for (auto &&module : resolvedModules) {
        if (!discover_module_decls(*module, sourcePath)) {
            error = true;
        }
    }
    if (error) return {};

    return resolvedModules;
}

ptr<ResolvedModuleDecl> Sema::resolve_module_decl(const ModuleDecl &moduleDecl) {
    debug_func(moduleDecl.location);
    std::vector<ptr<DMZ::ResolvedDecl>> declarations;

    auto modDecl =
        makePtr<ResolvedModuleDecl>(moduleDecl.location, moduleDecl.identifier, moduleDecl, moduleDecl.module_path,
                                    std::vector<ptr<DMZ::ResolvedDecl>>{}, makePtr<ResolvedScope>(m_currentScope));

    return modDecl;
}

bool Sema::discover_module_decls(ResolvedModuleDecl &resolvedModuleDecl, const std::filesystem::path &sourcePath) {
    debug_func(resolvedModuleDecl.location);
    auto prevModule = m_currentModule;
    m_currentModule = &resolvedModuleDecl;
    defer([&]() { m_currentModule = prevModule; });
    bool error = false;
    ScopeRAII moduleScope{*this, resolvedModuleDecl.scope.get()};

    // Sub-pass 1: Register all type-like declarations (structs, unions, const decls)
    // This ensures types are available before function signatures reference them
    for (auto &&decl : resolvedModuleDecl.moduleDecl.declarations) {
        if (const auto *ds = dynamic_cast<const DeclStmt *>(decl.get())) {
            debug_msg(decl->identifier << " " << decl->location);
            ptr<ResolvedDecl> resolvedDecl = resolve_decl_stmt(*ds);
            if (!resolvedDecl || !insert_decl_to_module(resolvedModuleDecl, std::move(resolvedDecl))) {
                error = true;
                continue;
            }
        }
    }
    if (error) return false;

    // Sub-pass 2: Register function declarations (signatures only, no bodies)
    // Now all type names are available for param/return type resolution
    for (auto &&decl : resolvedModuleDecl.moduleDecl.declarations) {
        if (const auto *fn = dynamic_cast<const FuncDecl *>(decl.get())) {
            debug_msg(decl->identifier << " " << decl->location);
            auto resolvedDecl = resolve_function_decl(*fn);

            if (resolvedModuleDecl.module_path == sourcePath) {
                if (auto *test = dynamic_cast<ResolvedTestDecl *>(resolvedDecl.get())) {
                    auto it = std::find_if(m_tests.begin(), m_tests.end(),
                                           [test](ResolvedTestDecl *t) { return t->identifier == test->identifier; });
                    if (it == m_tests.end()) {
                        m_tests.emplace_back(test);
                    }
                }
            }

            if (!resolvedDecl || !insert_decl_to_current_scope(*resolvedDecl) ||
                !insert_decl_to_module(resolvedModuleDecl, std::move(resolvedDecl))) {
                error = true;
                continue;
            }
        }
    }

    return !error;
}

// --- Lazy ensure methods ---

bool Sema::ensure_struct_members_resolved(ResolvedStructDecl &st) {
    debug_func(st.location << " " << st.name());
    if (st.membersResolved) return debug_ret(true);
    if (st.state == ResolvedState::InProgress && st.membersResolved) {
        // Cycle detected or re-entrant call during member resolution
        // For members, we might need a more sophisticated cycle detection if we allow circular dependencies
        // but for now we just prevent infinite recursion.
        return debug_ret(true);
    }
    st.state = ResolvedState::InProgress;
    st.membersResolved = true;
    if (!resolve_struct_members(st)) {
        st.state = ResolvedState::Unresolved;
        st.membersResolved = false;
        return debug_ret(false);
    }
    st.state = ResolvedState::DeclResolved;
    return debug_ret(true);
}

bool Sema::ensure_struct_funcs_resolved(ResolvedStructDecl &st) {
    debug_func(st.location << " " << st.name());
    if (st.functionsResolved) return debug_ret(true);
    if (st.state == ResolvedState::InProgress && st.membersResolved)
        return debug_ret(true);  // Already resolving funcs or members

    ResolvedState prevState = st.state;
    if (st.state != ResolvedState::InProgress) st.state = ResolvedState::InProgress;
    st.functionsResolved = true;
    if (!resolve_struct_decl_funcs(st)) {
        st.state = prevState;
        st.functionsResolved = false;
        return debug_ret(false);
    }
    if (st.state == ResolvedState::InProgress) st.state = ResolvedState::DeclResolved;
    return debug_ret(true);
}

bool Sema::ensure_struct_bodies_resolved(ResolvedStructDecl &st) {
    debug_func(st.location << " " << st.name());
    if (st.functionBodiesResolved) return debug_ret(true);
    if (st.state == ResolvedState::FullyResolved) return debug_ret(true);

    ResolvedState prevState = st.state;
    st.state = ResolvedState::InProgress;
    st.functionBodiesResolved = true;
    if (!ensure_struct_members_resolved(st)) return debug_ret(false);
    if (!ensure_struct_funcs_resolved(st)) return debug_ret(false);
    if (!resolve_struct_body_funcs(st)) {
        st.state = prevState;
        st.functionBodiesResolved = false;
        return debug_ret(false);
    }
    st.state = ResolvedState::FullyResolved;
    return debug_ret(true);
}

bool Sema::ensure_fully_resolved(ResolvedDecl &decl) {
    debug_func(decl.location << " " << decl.className() << " " << decl.name());
    if (decl.state == ResolvedState::FullyResolved) return debug_ret(true);
    if (decl.state == ResolvedState::InProgress) {
        // Cycle detected. We return true to break the cycle, assuming it will be resolved by the caller.
        // This is safe for functions as they can be recursive.
        return debug_ret(true);
    }

    ResolvedState prevState = decl.state;
    decl.state = ResolvedState::InProgress;

    bool success = true;
    if (auto *st = dynamic_cast<ResolvedStructDecl *>(&decl)) {
        success = ensure_struct_bodies_resolved(*st);
    } else if (auto *ds = dynamic_cast<ResolvedDeclStmt *>(&decl)) {
        if (!ds->type) {
            success = resolve_decl_stmt_initialize(*ds);
        }
    } else if (auto *vr = dynamic_cast<ResolvedVarDecl *>(&decl)) {
        success = resolve_var_decl_initialize(*vr);
    } else if (auto *fn = dynamic_cast<ResolvedFunctionDecl *>(&decl)) {
        if (resolve_builtin_function(*fn)) {
            success = true;
        } else if (fn->functionDecl && fn->functionDecl->body && !fn->body) {
            success = resolve_func_body(*fn, *fn->functionDecl->body);
        }
    } else if (auto *vr = dynamic_cast<ResolvedVarDecl *>(&decl)) {
        if (!vr->type && vr->parentDeclStmt) {
            success = ensure_fully_resolved(*vr->parentDeclStmt);
        }
    }

    if (success) {
        decl.state = ResolvedState::FullyResolved;
    } else {
        decl.state = prevState;
    }
    return debug_ret(success);
}

bool Sema::resolve_module_body(ResolvedModuleDecl &moduleDecl) {
    debug_func("");
    auto prevModule = m_currentModule;
    m_currentModule = &moduleDecl;
    defer([&]() { m_currentModule = prevModule; });
    bool error = false;
    ScopeRAII moduleScope(*this, moduleDecl.scope.get());

    // Single unified pass: resolve everything lazily via ensure_fully_resolved
    debug_msg("moduleDecl.declarations.size() " << moduleDecl.declarations.size());
    for (size_t i = 0; i < moduleDecl.declarations.size(); i++) {
        auto currentDecl = moduleDecl.declarations[i].get();
        if (!ensure_fully_resolved(*currentDecl)) {
            error = true;
        }
    }
    debug_msg("error " << error);
    if (error) return false;

    return true;
}

bool Sema::resolve_pending_body() {
    debug_func("");
    bool error = false;
    while (m_pending_decls.size() != 0) {
        auto decl = *m_pending_decls.begin();
        if (!ensure_fully_resolved(*decl)) {
            error = true;
        }
        std::erase(m_pending_decls, decl);
    }
    return !error;
}

bool Sema::resolve_func_body(ResolvedFunctionDecl &function, const Block &body) {
    debug_func("");
    ScopeRAII paramScope(*this, function.scope.get());
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
    if (dynamic_cast<const ResolvedBuiltinFunctionDecl *>(&fnDecl)) {
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

    auto body = makePtr<ResolvedBlock>(loc, std::move(blockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{},
                                       makePtr<ResolvedScope>(m_currentScope));

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

    auto elseBlock = makePtr<ResolvedBlock>(loc, std::move(retBlockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{},
                                            makePtr<ResolvedScope>(m_currentScope));
    std::vector<ptr<ResolvedCaseStmt>> cases;
    for (size_t i = 0; i < m_tests.size(); i++) {
        auto test_name = makePtr<ResolvedStringLiteral>(loc, m_tests[i]->name());
        auto retStmt = makePtr<ResolvedReturnStmt>(loc, std::move(test_name), std::vector<ptr<ResolvedDeferRefStmt>>{});
        std::vector<ptr<ResolvedStmt>> retBlockStmts;
        retBlockStmts.emplace_back(std::move(retStmt));
        auto retBlock = makePtr<ResolvedBlock>(loc, std::move(retBlockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{},
                                               makePtr<ResolvedScope>(m_currentScope));
        auto caseCondition = makePtr<ResolvedIntLiteral>(loc, i);
        std::vector<ptr<ResolvedExpr>> caseConditions;
        caseConditions.emplace_back(std::move(caseCondition));
        auto caseStmt = makePtr<ResolvedCaseStmt>(loc, std::move(caseConditions), std::move(retBlock));
        cases.emplace_back(std::move(caseStmt));
    }

    auto switchStmt = makePtr<ResolvedSwitchStmt>(loc, std::move(cond), std::move(cases), std::move(elseBlock));
    std::vector<ptr<ResolvedStmt>> blockStmts;
    blockStmts.emplace_back(std::move(switchStmt));

    auto body = makePtr<ResolvedBlock>(loc, std::move(blockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{},
                                       makePtr<ResolvedScope>(m_currentScope));

    // End body
    auto mutfnDecl = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    mutfnDecl->body = std::move(body);
}

void Sema::resolve_builtin_test_run(const ResolvedFunctionDecl &fnDecl) {
    // Begin Body
    SourceLocation loc{.file_name = "builtin"};
    auto cond = makePtr<ResolvedDeclRefExpr>(loc, *fnDecl.params[0], fnDecl.params[0]->type->clone());

    auto elseBlock =
        makePtr<ResolvedBlock>(loc, std::vector<ptr<ResolvedStmt>>{}, std::vector<ptr<ResolvedDeferRefStmt>>{},
                               makePtr<ResolvedScope>(m_currentScope));
    std::vector<ptr<ResolvedCaseStmt>> cases;
    for (size_t i = 0; i < m_tests.size(); i++) {
        add_dependency(const_cast<ResolvedTestDecl *>(m_tests[i]));

        auto testType = m_tests[i]->getFnType();
        auto test_ref = makePtr<ResolvedDeclRefExpr>(loc, *m_tests[i], testType->clone());
        auto test_call = makePtr<ResolvedCallExpr>(loc, testType->returnType->clone(), std::move(test_ref),
                                                   std::vector<ptr<ResolvedExpr>>{});
        auto returnOptType = dynamic_cast<const ResolvedTypeOptional *>(testType->returnType.get());
        if (!returnOptType) dmz_unreachable(loc, "internal error, test return type is not optional");
        auto tryExpr = makePtr<ResolvedTryErrorExpr>(loc, returnOptType->optionalType->clone(), std::move(test_call),
                                                     std::vector<ptr<ResolvedDeferRefStmt>>{});

        std::vector<ptr<ResolvedStmt>> caseBlockStmts;
        caseBlockStmts.emplace_back(std::move(tryExpr));
        auto caseBlock =
            makePtr<ResolvedBlock>(loc, std::move(caseBlockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{},
                                   makePtr<ResolvedScope>(m_currentScope));
        auto caseCondition = makePtr<ResolvedIntLiteral>(loc, i);
        std::vector<ptr<ResolvedExpr>> caseConditions;
        caseConditions.emplace_back(std::move(caseCondition));
        auto caseStmt = makePtr<ResolvedCaseStmt>(loc, std::move(caseConditions), std::move(caseBlock));
        cases.emplace_back(std::move(caseStmt));
    }

    auto switchStmt = makePtr<ResolvedSwitchStmt>(loc, std::move(cond), std::move(cases), std::move(elseBlock));
    std::vector<ptr<ResolvedStmt>> blockStmts;
    blockStmts.emplace_back(std::move(switchStmt));

    auto body = makePtr<ResolvedBlock>(loc, std::move(blockStmts), std::vector<ptr<ResolvedDeferRefStmt>>{},
                                       makePtr<ResolvedScope>(m_currentScope));

    // End body
    auto mutfnDecl = const_cast<ResolvedFunctionDecl *>(&fnDecl);
    mutfnDecl->body = std::move(body);
}
}  // namespace DMZ