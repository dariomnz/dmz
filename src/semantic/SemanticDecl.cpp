#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif

#include <algorithm>

#include "Debug.hpp"
#include "Utils.hpp"
#include "driver/Driver.hpp"
#include "parser/Parser.hpp"
#include "semantic/ComptimeValue.hpp"
#include "semantic/Semantic.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

ptr<ResolvedParamDecl> Sema::resolve_param_decl(const ParamDecl &param) {
    debug_func(param.location);
    ptr<ResolvedTypeExpr> typeExpr = nullptr;
    if (!param.isVararg) {
        typeExpr = resolve_type_expr(*param.type);
    } else {
        typeExpr = makePtr<ResolvedTypeExpr>(param.location, makePtr<ResolvedTypeVarArg>(param.location));
    }

    if (!param.isVararg) {
        if (!typeExpr || typeExpr->resolvedType->kind == ResolvedTypeKind::Void) {
            return report(param.location,
                          "parameter '" + param.identifier + "' has invalid '" + param.type->to_str() + "' type");
        }
        if (typeExpr->resolvedType->kind == ResolvedTypeKind::Type && !param.isComptime) {
            return report(param.location, "parameter of type 'type' must be 'comptime'");
        }
    }
    return makePtr<ResolvedParamDecl>(param.location, param.identifier, std::move(typeExpr), param.isMutable,
                                      param.isComptime, param.isVararg);
}

ptr<ResolvedFunctionDecl> Sema::resolve_member_function_decl(const ResolvedDecl &parentDecl, const FuncDecl &function) {
    debug_func(function.location);

    varOrReturn(resolvedFuncDecl, resolve_function_decl(function));

    if (!dynamic_cast<ResolvedFunctionDecl *>(resolvedFuncDecl.get())) {
        resolvedFuncDecl->dump();
        dmz_unreachable(function.location, "this should be a function declaration");
    }

    auto resolvedFunc = castPtr<ResolvedFunctionDecl>(std::move(resolvedFuncDecl));

    bool isStatic = true;

    ResolvedStructDecl *parentStructDecl = nullptr;
    ptr<ResolvedType> parentType = nullptr;
    if (auto ud = dynamic_cast<const ResolvedUnionDecl *>(&parentDecl)) {
        parentType = makePtr<ResolvedTypeUnion>(function.location, const_cast<ResolvedUnionDecl *>(ud));
        parentStructDecl = const_cast<ResolvedUnionDecl *>(ud);
    } else if (auto ud = dynamic_cast<const ResolvedEnumDecl *>(&parentDecl)) {
        parentType = makePtr<ResolvedTypeEnum>(function.location, const_cast<ResolvedEnumDecl *>(ud));
        parentStructDecl = const_cast<ResolvedEnumDecl *>(ud);
    } else if (auto sd = dynamic_cast<const ResolvedStructDecl *>(&parentDecl)) {
        parentType = makePtr<ResolvedTypeStruct>(function.location, const_cast<ResolvedStructDecl *>(sd));
        parentStructDecl = const_cast<ResolvedStructDecl *>(sd);
    } else {
        dmz_unreachable(function.location,
                        "parentDecl " + std::string(parentDecl.className()) + " is not a struct or union");
    }

    if (parentType) {
        if (resolvedFunc->params.size() > 0) {
            auto paramType = makePtr<ResolvedTypePointer>(function.location, std::move(parentType));
            if (resolvedFunc->params[0]->type->equal(*paramType)) {
                isStatic = false;
            }
        }
    }

    resolvedFunc->parentDecl = parentStructDecl;
    resolvedFunc->isStatic = isStatic;

    if (auto rf = dynamic_cast<ResolvedFuncDecl *>(resolvedFunc.get())) {
        rf->scope->currentFunction = rf;
        rf->scope->currentStruct = parentStructDecl;
    }

    return resolvedFunc;
}

ptr<ResolvedFuncDecl> Sema::resolve_function_decl(const FuncDecl &function) {
    debug_func(function.location);

    std::optional<ScopeRAII> genericFunctionScope = std::nullopt;
    ptr<ResolvedScope> takenGenericScope = nullptr;
    int nextSlot = 0;
    bool isGenericAST = false;
    for (auto &&p : function.params) {
        if (p->isComptime || dynamic_cast<const TypeAnyType *>(p->type.get())) {
            isGenericAST = true;
            break;
        }
    }

    if (isGenericAST) {
        genericFunctionScope.emplace(*this);
        takenGenericScope = genericFunctionScope->takeScope();
    }

    ScopeRAII functionScope(*this);
    auto takenScope = functionScope.takeScope();

    std::vector<ptr<ResolvedParamDecl>> resolvedParams;
    std::vector<ptr<ResolvedType>> resolvedParamsTypes;

    bool haveVararg = false;
    for (auto &&param : function.params) {
        if (haveVararg) {
            report(param->location, "vararg '...' can only be in the last argument");
            return nullptr;
        }

        auto resolvedParam = resolve_param_decl(*param);
        if (!resolvedParam || !insert_decl_to_current_scope(*resolvedParam)) return nullptr;

        if (resolvedParam->isVararg) {
            haveVararg = true;
        }

        if (resolvedParam->isComptime && resolvedParam->type->kind == ResolvedTypeKind::Type) {
            auto slot = nextSlot++;
            resolvedParam->type =
                makePtr<ResolvedTypeAnyType>(resolvedParam->location, slot, resolvedParam->identifier);
            resolvedParam->set_constant_value(
                ComptimeValue(makePtr<ResolvedTypeAnyType>(resolvedParam->location, slot, resolvedParam->identifier)));
        } else if (resolvedParam->type->kind == ResolvedTypeKind::AnyType) {
            auto &anyType = static_cast<const ResolvedTypeAnyType &>(*resolvedParam->type);
            if (anyType.genericSlot < 0) {
                auto slot = nextSlot++;
                resolvedParam->type = makePtr<ResolvedTypeAnyType>(resolvedParam->location, slot, "anytype");
                if (resolvedParam->resolvedTypeExpr)
                    resolvedParam->resolvedTypeExpr->resolvedType = resolvedParam->type->clone();
            }
        }

        resolvedParamsTypes.emplace_back(resolvedParam->type->clone());
        resolvedParams.emplace_back(std::move(resolvedParam));
    }

    if (dynamic_cast<TypeAnyType *>(function.type.get())) {
        // For the moment return type cannot be anytype
        return report(function.type->location, "functions cannot have anytype return type");
    }

    auto returnType = resolve_type(*function.type);

    if (!returnType) {
        if (isGenericAST) {
            returnType = makePtr<ResolvedTypeAnyType>(function.location);
        } else {
            return report(function.location, "function '" + function.identifier + "' has invalid '" +
                                                 function.type->to_str() + "' return type");
        }
    }

    auto resolvedReturnTypeExpr = resolve_expr(*function.type, true);
    if (!resolvedReturnTypeExpr) {
        if (!isGenericAST) {
            return report(function.location, "function '" + function.identifier + "' has invalid '" +
                                                 function.type->to_str() + "' return type");
        }
    }

    debug_msg("returnType: " << returnType->className() << " " << returnType->to_str());

    if (function.identifier == "main") {
        if (returnType->kind != ResolvedTypeKind::Void)
            return report(function.location, "'main' function is expected to have 'void' return type");

        if (!function.params.empty())
            return report(function.location, "'main' function is expected to take no arguments");
    }

    bool hasComptimeParam = false;
    for (auto &&param : resolvedParams) {
        if (param->isComptime) {
            hasComptimeParam = true;
            break;
        }
    }

    auto fnType = makePtr<ResolvedTypeFunction>(function.location, nullptr, std::move(resolvedParamsTypes),
                                                std::move(returnType));

    if (dynamic_cast<const ExternFunctionDecl *>(&function)) {
        auto ret = makePtr<ResolvedExternFunctionDecl>(function.location, function.isPublic, function.identifier,
                                                       std::move(fnType), std::move(resolvedParams),
                                                       std::move(resolvedReturnTypeExpr), std::move(takenScope));
        ret->getFnType()->fnDecl = ret.get();
        if (ret->scope) ret->scope->currentFunction = ret.get();
        return ret;
    }
    if (auto functionDecl = dynamic_cast<const FunctionDecl *>(&function)) {
        if (dynamic_cast<const TestDecl *>(&function)) {
            auto ret =
                makePtr<ResolvedTestDecl>(function.location, function.identifier, functionDecl, std::move(takenScope));
            ret->getFnType()->fnDecl = ret.get();
            if (ret->scope) ret->scope->currentFunction = ret.get();
            return ret;
        }
        if (nextSlot > 0 || hasComptimeParam) {
            auto ret = makePtr<ResolvedGenericFunctionDecl>(
                function.location, function.isPublic, function.identifier, std::move(fnType), std::move(resolvedParams),
                std::move(resolvedReturnTypeExpr), std::move(takenScope), std::move(takenGenericScope), functionDecl);
            ret->getFnType()->fnDecl = ret.get();
            ret->symbolName = resolve_decl_name(function.identifier);
            if (ret->genericScope) ret->genericScope->currentFunction = ret.get();
            if (ret->scope) ret->scope->currentFunction = ret.get();
            return ret;
        } else {
            auto ret = makePtr<ResolvedFunctionDecl>(function.location, function.isPublic, function.identifier,
                                                     std::move(fnType), std::move(resolvedParams),
                                                     std::move(resolvedReturnTypeExpr), std::move(takenScope),
                                                     functionDecl, nullptr, false, functionDecl->isExport);
            ret->getFnType()->fnDecl = ret.get();
            ret->symbolName = resolve_decl_name(function.identifier);
            if (ret->scope) ret->scope->currentFunction = ret.get();
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
    size_t numComptime = 0;
    for (auto &&p : funcDecl.params) {
        bool isTypeComptime = p->isComptime && p->type->kind == ResolvedTypeKind::AnyType && p->resolvedTypeExpr &&
                              p->resolvedTypeExpr->resolvedType->kind == ResolvedTypeKind::Type;
        if (p->isComptime && !isTypeComptime) numComptime++;
    }

    int maxSlot = -1;
    for (auto &&p : funcDecl.params) {
        if (p->type->kind == ResolvedTypeKind::AnyType) {
            int s = static_cast<const ResolvedTypeAnyType &>(*p->type).genericSlot;
            if (s > maxSlot) maxSlot = s;
        }
    }
    if (funcDecl.type->kind == ResolvedTypeKind::Function) {
        auto &fnType = static_cast<const ResolvedTypeFunction &>(*funcDecl.type);
        if (fnType.returnType->kind == ResolvedTypeKind::AnyType) {
            int s = static_cast<const ResolvedTypeAnyType &>(*fnType.returnType).genericSlot;
            if (s > maxSlot) maxSlot = s;
        }
    }
    size_t numGeneric = maxSlot >= 0 ? maxSlot + 1 : 0;
    if (numGeneric + numComptime != genericTypes.specializedTypes.size()) {
        return report(location, "unexpected number of specializations, expected " +
                                    std::to_string(numGeneric + numComptime) + " actual " +
                                    std::to_string(genericTypes.specializedTypes.size()));
    }
    for (auto &gt : genericTypes.specializedTypes) {
        if (gt->kind == ResolvedTypeKind::AnyType) {
            debug_msg("Not specialize generic types are no specialized");
            return nullptr;
        }
    }
    // Search if is specified
    for (auto &&func : funcDecl.specializations) {
        if (genericTypes.equal(*func->specializedTypes)) {
            return func.get();
        }
    }

    // If not found specialize the function
    auto placeholderFunc = makePtr<ResolvedSpecializedFunctionDecl>(
        funcDecl.location, funcDecl.isPublic, funcDecl.identifier, nullptr, std::vector<ptr<ResolvedParamDecl>>{},
        nullptr, nullptr, funcDecl.functionDecl, &funcDecl, castPtr<ResolvedTypeSpecialized>(genericTypes.clone()));
    placeholderFunc->symbolName = funcDecl.symbolName;
    auto *retFunc = funcDecl.specializations.emplace_back(std::move(placeholderFunc)).get();

    // Restore scope
    ScopeRAII parentFunctionScope(*this, funcDecl.genericScope.get());
    ScopeRAII functionScope(*this);
    auto takenScope = functionScope.takeScope();
    retFunc->scope = std::move(takenScope);
    retFunc->scope->currentFunction = retFunc;

    std::vector<ptr<ResolvedParamDecl>> resolvedParams;
    std::vector<ptr<ResolvedType>> resolvedParamsTypes;

    bool haveVararg = false;
    size_t specIdx = numGeneric;
    for (size_t i = 0; i < funcDecl.functionDecl->params.size(); i++) {
        auto &&param = funcDecl.functionDecl->params[i];
        auto resolvedParam = resolve_param_decl(*param);
        if (!resolvedParam) return nullptr;

        // Replace anytype with the specialized concrete type (clone returns concrete when specialized)
        if (resolvedParam->type->kind == ResolvedTypeKind::AnyType) {
            if (i < funcDecl.params.size()) {
                auto &origAnyType = static_cast<const ResolvedTypeAnyType &>(*funcDecl.params[i]->type);
                if (origAnyType.genericSlot >= 0 &&
                    (size_t)origAnyType.genericSlot < retFunc->specializedTypes->specializedTypes.size()) {
                    auto concreteType = retFunc->specializedTypes->specializedTypes[origAnyType.genericSlot]->clone();
                    static_cast<ResolvedTypeAnyType *>(resolvedParam->type.get())
                        ->set_specialized(concreteType->clone());
                    resolvedParam->type = resolvedParam->type->clone();
                    if (resolvedParam->resolvedTypeExpr &&
                        resolvedParam->resolvedTypeExpr->resolvedType->kind == ResolvedTypeKind::AnyType) {
                        static_cast<ResolvedTypeAnyType *>(resolvedParam->resolvedTypeExpr->resolvedType.get())
                            ->set_specialized(concreteType->clone());
                        resolvedParam->resolvedTypeExpr->resolvedType =
                            resolvedParam->resolvedTypeExpr->resolvedType->clone();
                    }
                }
            }
        }

        if (resolvedParam->isComptime) {
            bool isTypeComptime = funcDecl.params[i]->type->kind == ResolvedTypeKind::AnyType &&
                                  funcDecl.params[i]->resolvedTypeExpr &&
                                  funcDecl.params[i]->resolvedTypeExpr->resolvedType->kind == ResolvedTypeKind::Type;
            if (isTypeComptime) {
                auto &anyType = static_cast<const ResolvedTypeAnyType &>(*funcDecl.params[i]->type);
                if (anyType.genericSlot >= 0 &&
                    (size_t)anyType.genericSlot < retFunc->specializedTypes->specializedTypes.size()) {
                    resolvedParam->set_constant_value(
                        ComptimeValue(retFunc->specializedTypes->specializedTypes[anyType.genericSlot]->clone()));
                }
            } else {
                if (specIdx >= retFunc->specializedTypes->specializedTypes.size()) {
                    dmz_unreachable(funcDecl.location, "missing specialized comptime value");
                }
                auto comptimeType = dynamic_cast<const ResolvedTypeComptimeValue *>(
                    retFunc->specializedTypes->specializedTypes[specIdx++].get());
                if (!comptimeType) {
                    dmz_unreachable(funcDecl.location, "expected comptime value in specialized types");
                }
                resolvedParam->set_constant_value(*comptimeType->value);
            }
        }

        debug_msg("Specialize param " << resolvedParam->identifier << " type " << resolvedParam->type->to_str());
        if (resolvedParam->resolvedTypeExpr)
            resolvedParam->resolvedTypeExpr->resolvedType = resolvedParam->type->clone();
        resolvedParamsTypes.emplace_back(resolvedParam->type->clone());

        if (haveVararg) {
            report(resolvedParam->location, "vararg '...' can only be in the last argument");
            return nullptr;
        }

        if (!insert_decl_to_current_scope(*resolvedParam)) return nullptr;

        if (resolvedParam->isVararg) {
            haveVararg = true;
        }
        resolvedParams.emplace_back(std::move(resolvedParam));
    }

    // Resolve return type from AST — decl->type->clone() returns concrete type via specialized AnyType
    ptr<ResolvedType> returnType;
    if (funcDecl.functionDecl->type) {
        returnType = resolve_type(*funcDecl.functionDecl->type);
    }
    if (!returnType) {
        // Fallback: clone generic return type (AnyType::clone returns specialized type if set)
        auto &fnType = static_cast<const ResolvedTypeFunction &>(*funcDecl.type);
        returnType = fnType.returnType->clone();
    }

    if (!returnType)
        return report(funcDecl.location, "function '" + funcDecl.identifier + "' has invalid '" +
                                             funcDecl.type->to_str() + "' return type");

    auto newFnType = makePtr<ResolvedTypeFunction>(funcDecl.location, retFunc, std::move(resolvedParamsTypes),
                                                   std::move(returnType));

    retFunc->type = std::move(newFnType);
    retFunc->params = std::move(resolvedParams);
    retFunc->resolvedReturnTypeExpr = std::move(funcDecl.resolvedReturnTypeExpr);

    debug_msg("Adding specialized function " << retFunc->name() << " to pending decls");
    m_pending_decls.emplace(retFunc);

    return retFunc;
}

ptr<ResolvedVarDecl> Sema::resolve_var_decl(const VarDecl &varDecl) {
    debug_func(varDecl.location);
    if (!varDecl.type && !varDecl.initializer)
        return report(varDecl.location, "an uninitialized variable is expected to have a type specifier");

    ScopeRAII scope(*this);
    auto takenScope = scope.takeScope();

    ptr<ResolvedTypeExpr> resolvedvarType = nullptr;
    if (varDecl.type) {
        resolvedvarType = resolve_type_expr(*varDecl.type);
        if (!resolvedvarType) {
            return report(varDecl.location,
                          "variable '" + varDecl.identifier + "' has invalid '" + varDecl.type->to_str() + "' type");
        }
    }
    if (resolvedvarType && resolvedvarType->resolvedType->kind == ResolvedTypeKind::AnyType) {
        if (!m_currentFunction || (!dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction) &&
                                   !dynamic_cast<ResolvedSpecializedFunctionDecl *>(m_currentFunction))) {
            return report(varDecl.location,
                          "variable '" + varDecl.identifier +
                              "' cannot have 'anytype' type, 'anytype' is only allowed in function parameters");
        }
    }
    if (resolvedvarType && resolvedvarType->resolvedType->kind == ResolvedTypeKind::Void) {
        return report(varDecl.location, "variable '" + varDecl.identifier + "' has invalid '" +
                                            resolvedvarType->resolvedType->to_str() + "' type");
    }
    if (resolvedvarType && resolvedvarType->resolvedType->kind == ResolvedTypeKind::Type && varDecl.isMutable) {
        return report(varDecl.location, "variable of type 'type' must be 'const'");
    }

    auto ret = makePtr<ResolvedVarDecl>(varDecl.location, &varDecl, varDecl.isPublic, varDecl.identifier,
                                        std::move(resolvedvarType), varDecl.isMutable, std::move(takenScope), nullptr,
                                        varDecl.isGlobal);
    ret->symbolName = resolve_decl_name(varDecl.identifier);
    return ret;
}

bool Sema::resolve_var_decl_initialize(ResolvedVarDecl &varDecl) {
    debug_func(varDecl.location);
    if (varDecl.state == ResolvedState::Error) return debug_ret(false);
    if (varDecl.state == ResolvedState::FullyResolved) return debug_ret(true);
    if (varDecl.state == ResolvedState::InProgress && varDecl.nameResolved) return debug_ret(true);  // Cycle

    varDecl.state = ResolvedState::InProgress;
    varDecl.nameResolved = true;

    if (varDecl.varDecl && !varDecl.varDecl->initializer && !varDecl.type) {
        debug_msg(varDecl.varDecl);
        varDecl.varDecl->dump();
        dmz_unreachable(varDecl.location, "unexpected malformed var decl");
    }

    ScopeRAII scopeRAII(*this, varDecl.scope.get());

    ptr<ResolvedExpr> resolvedInitializer = nullptr;
    if (varDecl.varDecl && varDecl.varDecl->initializer) {
        resolvedInitializer =
            resolve_expr(*varDecl.varDecl->initializer, dynamic_cast<StructDecl *>(varDecl.varDecl->initializer.get()));
        if (!resolvedInitializer) {
            varDecl.state = ResolvedState::Error;
            return debug_ret(false);
        }

        if (dynamic_cast<const StructDecl *>(varDecl.varDecl->initializer.get()) ||
            dynamic_cast<const UnionDecl *>(varDecl.varDecl->initializer.get())) {
            if (auto *resolvedTypeExpr = dynamic_cast<ResolvedTypeExpr *>(resolvedInitializer.get())) {
                if (auto *structType = dynamic_cast<ResolvedTypeStructDecl *>(resolvedTypeExpr->resolvedType.get())) {
                    if (structType->decl->identifier.find("structL") != std::string::npos ||
                        structType->decl->identifier.find("unionL") != std::string::npos ||
                        structType->decl->identifier.find("enumL") != std::string::npos) {
                        if (m_currentFunction == nullptr) {
                            structType->decl->identifier = resolve_decl_name(varDecl.identifier);
                        } else {
                            structType->decl->identifier += "." + varDecl.identifier;
                        }
                        structType->decl->isPublic = varDecl.isPublic;
                    }
                }
            }
        }
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
            varDecl.state = ResolvedState::Error;
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
                if (!perform_implicit_cast(resolvedInitializer, *varDecl.type)) return false;
                if (!varDecl.type->compare(*resolvedInitializer->type)) {
                    report(resolvedInitializer->location, "initializer type mismatch expected '" +
                                                              varDecl.type->to_str() + "' actual '" +
                                                              resolvedInitializer->type->to_str() + "'");
                    varDecl.state = ResolvedState::Error;
                    return debug_ret(false);
                }
            }
            type = varDecl.type.get();
        }

        auto val = cee.evaluate(*resolvedInitializer, false);
        resolvedInitializer->set_constant_value(val);
        if (val) {
            varDecl.set_constant_value(val);
        }
    }

    if (type->kind == ResolvedTypeKind::Void) {
        report(varDecl.location, "variable '" + varDecl.identifier + "' has invalid '" + type->to_str() + "' type");
        varDecl.state = ResolvedState::Error;
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

    ScopeRAII unionScope(*this, resolvedUnionDecl.scope.get());

    std::vector<ptr<ResolvedFieldDecl>> resolvedFields;
    std::vector<std::string> resolvedFields_strs;
    int idx = 0;

    for (auto &&decl : resolvedUnionDecl.unionDecl()->decls) {
        if (auto field = dynamic_cast<const FieldDecl *>(decl.get())) {
            auto typeExpr = resolve_type_expr(*field->type);
            if (!typeExpr) {
                report(field->type->location, "unexpected type '" + field->type->to_str() + "'");
                return false;
            }
            ptr<ResolvedExpr> default_initializer = nullptr;
            if (field->default_initializer) {
                default_initializer = resolve_expr(*field->default_initializer);
                if (!default_initializer) return false;
            }

            auto retField = makePtr<ResolvedFieldDecl>(field->location, field->identifier, std::move(typeExpr), idx++,
                                                       std::move(default_initializer));
            resolvedFields.emplace_back(std::move(retField));
            resolvedFields_strs.emplace_back(field->identifier);
        } else if (auto declStmt = dynamic_cast<const DeclStmt *>(decl.get())) {
            auto resDeclStmt = resolve_decl_stmt(*declStmt);
            if (!resDeclStmt) return false;
            resolvedUnionDecl.otherDecls.emplace_back(std::move(resDeclStmt));
        }
    }

    if (resolvedUnionDecl.fields.size() != 0) {
        dmz_unreachable(resolvedUnionDecl.location, "resolvedUnionDecl.fields.size() != 0");
    }
    resolvedUnionDecl.fields = std::move(resolvedFields);
    resolvedUnionDecl.fields_strs = std::move(resolvedFields_strs);

    return true;
}

bool Sema::resolve_enum_members(ResolvedEnumDecl &resolvedEnumDecl) {
    debug_func(resolvedEnumDecl.location);

    ScopeRAII enumScope(*this, resolvedEnumDecl.scope.get());

    std::vector<ptr<ResolvedFieldDecl>> resolvedFields;
    std::vector<std::string> resolvedFields_strs;
    int idx = 0;
    ResolvedTypeNumber int_type{resolvedEnumDecl.location, ResolvedNumberKind::Int, 32};
    for (auto &&decl : resolvedEnumDecl.enumDecl()->decls) {
        if (auto field = dynamic_cast<const FieldDecl *>(decl.get())) {
            auto type = makePtr<ResolvedTypeEnum>(field->location, &resolvedEnumDecl);
            ptr<ResolvedExpr> default_initializer = nullptr;
            if (field->default_initializer) {
                default_initializer = resolve_expr(*field->default_initializer);
                if (!default_initializer) return false;
                if (!int_type.compare(*default_initializer->type)) {
                    report(default_initializer->location,
                           "unexpected type in enum field initializer '" + default_initializer->type->to_str() + "'");
                    return false;
                }
                default_initializer->set_constant_value(cee.evaluate(*default_initializer, false));
                if (!default_initializer->get_constant_value()) {
                    report(default_initializer->location,
                           "enum initializer must be a compile-time constant expression");
                    return false;
                }
            }

            auto retField = makePtr<ResolvedFieldDecl>(field->location, field->identifier,
                                                       makePtr<ResolvedTypeExpr>(field->location, std::move(type)),
                                                       idx++, std::move(default_initializer));
            if (retField->default_initializer) {
                idx = retField->default_initializer->get_constant_value().value().getInt();
                retField->set_constant_value(ComptimeValue((int64_t)idx));
                idx++;
            } else {
                retField->set_constant_value(ComptimeValue((int64_t)retField->index));
            }

            resolvedFields.emplace_back(std::move(retField));
            resolvedFields_strs.emplace_back(field->identifier);
        } else if (auto declStmt = dynamic_cast<const DeclStmt *>(decl.get())) {
            auto resDeclStmt = resolve_decl_stmt(*declStmt);
            if (!resDeclStmt) return false;
            resolvedEnumDecl.otherDecls.emplace_back(std::move(resDeclStmt));
        }
    }

    if (resolvedEnumDecl.fields.size() != 0) {
        dmz_unreachable(resolvedEnumDecl.location, "resolvedEnumDecl.fields.size() != 0");
    }
    resolvedEnumDecl.fields = std::move(resolvedFields);
    resolvedEnumDecl.fields_strs = std::move(resolvedFields_strs);

    return true;
}

ptr<ResolvedStructDecl> Sema::resolve_struct_decl(const StructDecl &structDecl) {
    debug_func(structDecl.location);
    std::unordered_set<std::string> identifiers;
    auto count = m_structInstanceCounts[&structDecl]++;

    std::optional<ScopeRAII> genericStructScope = std::nullopt;
    ptr<ResolvedScope> takenGenericScope = nullptr;
    ScopeRAII fieldScope(*this);
    auto takenFieldScope = fieldScope.takeScope();
    ptr<ResolvedStructDecl> resStructDecl;
    std::string structName = resolve_decl_name(structDecl.identifier);
    if (count > 0) structName += "." + std::to_string(count);
    if (auto unionDecl = dynamic_cast<const UnionDecl *>(&structDecl)) {
        resStructDecl = makePtr<ResolvedUnionDecl>(unionDecl->location, unionDecl->isPublic, structName, unionDecl,
                                                   unionDecl->isPacked, vec<ptr<ResolvedFieldDecl>>{},
                                                   vec<ptr<ResolvedFunctionDecl>>{}, std::move(takenFieldScope));
    } else if (auto enumDecl = dynamic_cast<const EnumDecl *>(&structDecl)) {
        resStructDecl = makePtr<ResolvedEnumDecl>(enumDecl->location, enumDecl->isPublic, structName, enumDecl,
                                                  enumDecl->isPacked, vec<ptr<ResolvedFieldDecl>>{},
                                                  vec<ptr<ResolvedFunctionDecl>>{}, std::move(takenFieldScope));
    } else {
        resStructDecl = makePtr<ResolvedStructDecl>(structDecl.location, structDecl.isPublic, structName, &structDecl,
                                                    structDecl.isPacked, vec<ptr<ResolvedFieldDecl>>{},
                                                    vec<ptr<ResolvedFunctionDecl>>{}, std::move(takenFieldScope));
    }
    if (resStructDecl->scope) resStructDecl->scope->currentStruct = resStructDecl.get();

    debug_msg("structDecl.decls.size() " << structDecl.decls.size());
    for (auto &&decl : structDecl.decls) {
        debug_msg(decl->location << " " << decl->identifier);
        if (dynamic_cast<Decoration *>(decl.get())) continue;
        if (!identifiers.emplace(decl->identifier).second)
            return report(decl->location, "member '" + decl->identifier + "' is already declared");
    }

    return resStructDecl;
}

bool Sema::resolve_struct_members(ResolvedStructDecl &resolvedStructDecl) {
    debug_func(resolvedStructDecl.location);

    if (auto unionStruct = dynamic_cast<ResolvedUnionDecl *>(&resolvedStructDecl)) {
        return resolve_union_members(*unionStruct);
    } else if (auto enumStruct = dynamic_cast<ResolvedEnumDecl *>(&resolvedStructDecl)) {
        return resolve_enum_members(*enumStruct);
    }

    ScopeRAII structScope(*this, resolvedStructDecl.scope.get());

    if (!resolvedStructDecl.structDecl) {
        // Tuples/Special cases might not have a structDecl
        return true;
    }

    if (resolvedStructDecl.fields.empty()) {
        std::vector<ptr<ResolvedFieldDecl>> resolvedFields;
        std::vector<std::string> resolvedFields_strs;
        int idx = 0;
        for (auto &&decl : resolvedStructDecl.structDecl->decls) {
            if (auto field = dynamic_cast<const FieldDecl *>(decl.get())) {
                auto typeExpr = resolve_type_expr(*field->type);
                if (!typeExpr) {
                    report(field->type->location, "unexpected type '" + field->type->to_str() + "'");
                    return false;
                }
                ptr<ResolvedExpr> default_initizlizer = nullptr;
                if (field->default_initializer) {
                    default_initizlizer = resolve_expr(*field->default_initializer);
                    if (!default_initizlizer) return false;
                }

                auto retField = makePtr<ResolvedFieldDecl>(field->location, field->identifier, std::move(typeExpr),
                                                           idx++, std::move(default_initizlizer));
                resolvedFields.emplace_back(std::move(retField));
                resolvedFields_strs.emplace_back(field->identifier);
            } else if (auto declStmt = dynamic_cast<const DeclStmt *>(decl.get())) {
                auto resDeclStmt = resolve_decl_stmt(*declStmt);
                if (!resDeclStmt) return false;
                resolvedStructDecl.otherDecls.emplace_back(std::move(resDeclStmt));
            }
        }

        if (resolvedStructDecl.fields.size() != 0) {
            dmz_unreachable(resolvedStructDecl.location, "resolvedStructDecl.fields.size() != 0");
        }
        resolvedStructDecl.fields = std::move(resolvedFields);
        resolvedStructDecl.fields_strs = std::move(resolvedFields_strs);
    }

    std::stack<std::pair<ResolvedStructDecl *, std::unordered_set<const ResolvedStructDecl *>>> worklist;
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
            auto type = field->type->clone();
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
    ResolvedScope *scopeToUse = resolvedStructDecl.scope.get();
    ScopeRAII structScope(*this, scopeToUse);

    for (size_t i = 0; i < resolvedStructDecl.functions.size(); i++) {
        auto &resfunc = resolvedStructDecl.functions[i];
        if (!resolve_func_body(*resfunc, *resfunc->functionDecl->body)) return false;
    }

    return true;
}

bool Sema::resolve_struct_decl_funcs(ResolvedStructDecl &resolvedStructDecl) {
    debug_func(resolvedStructDecl.location << " " << resolvedStructDecl.name());

    ScopeRAII structScope(*this, resolvedStructDecl.scope.get());

    std::vector<ptr<ResolvedFunctionDecl>> resolvedFunctions;
    std::vector<std::string> resolvedFunctions_strs;
    if (resolvedStructDecl.structDecl) {
        for (auto &&decl : resolvedStructDecl.structDecl->decls) {
            auto func = dynamic_cast<const FunctionDecl *>(decl.get());
            if (!func) continue;

            auto resolvedMember = resolve_member_function_decl(resolvedStructDecl, *func);
            if (!resolvedMember) return false;
            resolvedFunctions.emplace_back(std::move(resolvedMember));
            resolvedFunctions_strs.emplace_back(func->identifier);
        }
    }

    if (resolvedStructDecl.functions.size() != 0) {
        dmz_unreachable(resolvedStructDecl.location, "resolvedStructDecl.functions.size() != 0");
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

std::vector<ptr<ResolvedModuleDecl>> Sema::resolve_modules_decls(std::vector<ptr<ModuleDecl>> &modules) {
    bool error = false;
    debug_func("error " << (error ? "true" : "false"));
    debug_msg("[Sema] resolve_modules_decls: resolving " << modules.size() << " modules");
    std::vector<ptr<ResolvedModuleDecl>> resolvedModules;

    // Pass 1: Create all module declarations (some might be lazy/empty AST)
    for (auto &&module : modules) {
        std::string identifier;
        std::filesystem::path module_path;
        if (module) {
            if (std::filesystem::exists(module->module_path)) {
                module->module_path = std::filesystem::canonical(module->module_path);
            } else {
                module->module_path = std::filesystem::absolute(module->module_path);
            }
            identifier = module->identifier;
            module_path = module->module_path;
        }

        debug_msg("[Sema]   - module path: " << module_path);
        auto resolvedModule = resolve_module_decl(std::move(module), identifier, module_path);
        if (!resolvedModule) {
            error = true;
            continue;
        }
        resolvedModules.emplace_back(std::move(resolvedModule));
    }
    if (error) return {};

    // Pass 2: Discover declarations only for modules that are already parsed
    // (the main source file, usually)
    for (auto &&module : resolvedModules) {
        if (module->moduleDecl) {
            if (!ensure_module_discovered(*module)) {
                error = true;
            }
        } else {
            module->state = ResolvedState::Unresolved;
        }
    }
    if (error) return {};

    return resolvedModules;
}

ptr<ResolvedModuleDecl> Sema::resolve_module_decl(ptr<ModuleDecl> moduleDecl, std::string identifier,
                                                  std::filesystem::path module_path) {
    debug_func(module_path << " " << (moduleDecl ? moduleDecl->location : SourceLocation{}));
    std::vector<ptr<DMZ::ResolvedDecl>> declarations;

    auto modDecl =
        makePtr<ResolvedModuleDecl>(moduleDecl ? moduleDecl->location : SourceLocation{}, identifier,
                                    std::move(moduleDecl), module_path, std::vector<ptr<DMZ::ResolvedDecl>>{}, nullptr);
    modDecl->scope = makePtr<ResolvedScope>(modDecl.get());

    return modDecl;
}

bool Sema::ensure_module_parsed(ResolvedModuleDecl &mod) {
    debug_func(mod.location << " " << mod.name() << " " << mod.state);
    if (mod.moduleDecl) return true;
    debug_msg("Lazily parsing module: " << mod.module_path);

    Lexer l(mod.module_path.string());
    Parser p(m_driver, l);
    auto [parse_ast, success] = p.parse_source_file();
    if (!success || !parse_ast) {
        return false;
    }
    parse_ast->module_path = mod.module_path;
    mod.moduleDecl = std::move(parse_ast);
    return true;
}

bool Sema::ensure_module_discovered(ResolvedModuleDecl &mod) {
    debug_func(mod.location << " " << mod.name() << " " << mod.state);
    if (mod.state == ResolvedState::Error) return debug_ret(false);
    if (mod.state >= ResolvedState::DeclResolved) return debug_ret(true);
    if (mod.state == ResolvedState::InProgress) {
        dmz_unreachable(mod.location, "Module discovery cycle detected for " + mod.identifier);
    }

    if (!ensure_module_parsed(mod)) {
        mod.state = ResolvedState::Error;
        return false;
    }

    if (!discover_module_decls(mod)) {
        mod.state = ResolvedState::Error;
        return false;
    }
    mod.state = ResolvedState::DeclResolved;
    return true;
}

bool Sema::discover_module_decls(ResolvedModuleDecl &resolvedModuleDecl) {
    debug_func(resolvedModuleDecl.location);
    bool error = false;
    ScopeRAII moduleScope{*this, resolvedModuleDecl.scope.get()};

    if (!ensure_module_parsed(resolvedModuleDecl)) return false;

    // Sub-pass 1: Register all type-like declarations (structs, unions, const decls)
    // This ensures types are available before function signatures reference them
    for (auto &&decl : resolvedModuleDecl.moduleDecl->declarations) {
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
    bool isRoot = (resolvedModuleDecl.module_path == m_driver.m_options.source);
    bool inTestDir = !m_driver.m_options.testDir.empty() && [&]() {
        auto rel = std::filesystem::relative(resolvedModuleDecl.module_path, m_driver.m_options.testDir);
        return !rel.empty() && !rel.string().starts_with("..");
    }();
    for (auto &&decl : resolvedModuleDecl.moduleDecl->declarations) {
        if (const auto *fn = dynamic_cast<const FuncDecl *>(decl.get())) {
            debug_msg(decl->identifier << " " << decl->location);
            auto resolvedDecl = resolve_function_decl(*fn);

            if (isRoot || inTestDir) {
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
    debug_func(st.location << " " << st.name() << " " << st.state);
    if (st.state == ResolvedState::Error) return debug_ret(false);
    if (st.state >= ResolvedState::DeclResolved && st.membersResolved) return debug_ret(true);
    if (st.state == ResolvedState::InProgress && st.membersResolved) return debug_ret(true);

    st.state = ResolvedState::InProgress;
    st.membersResolved = true;
    if (!resolve_struct_members(st)) {
        st.state = ResolvedState::Error;
        st.membersResolved = false;
        return debug_ret(false);
    }
    st.state = ResolvedState::DeclResolved;
    return debug_ret(true);
}

bool Sema::ensure_struct_funcs_resolved(ResolvedStructDecl &st) {
    debug_func(st.location << " " << st.name() << " " << st.state);
    if (st.state == ResolvedState::Error) return debug_ret(false);
    if (st.state >= ResolvedState::DeclResolved && st.functionsResolved) return debug_ret(true);
    if (st.state == ResolvedState::InProgress && st.functionsResolved) return debug_ret(true);

    st.state = ResolvedState::InProgress;
    st.functionsResolved = true;
    if (!resolve_struct_decl_funcs(st)) {
        st.state = ResolvedState::Error;
        st.functionsResolved = false;
        return debug_ret(false);
    }
    st.state = ResolvedState::DeclResolved;
    return debug_ret(true);
}

bool Sema::ensure_struct_bodies_resolved(ResolvedStructDecl &st) {
    debug_func(st.location << " " << st.name() << " " << st.state);
    if (st.state == ResolvedState::Error) return debug_ret(false);
    if (st.state == ResolvedState::FullyResolved) return debug_ret(true);
    if (st.state == ResolvedState::InProgress && st.functionBodiesResolved) return debug_ret(true);

    st.state = ResolvedState::InProgress;
    st.functionBodiesResolved = true;
    if (!ensure_struct_funcs_resolved(st)) return debug_ret(false);
    if (!ensure_struct_members_resolved(st)) return debug_ret(false);
    if (!resolve_struct_body_funcs(st)) {
        st.state = ResolvedState::Error;
        st.functionBodiesResolved = false;
        return debug_ret(false);
    }
    st.state = ResolvedState::FullyResolved;
    return debug_ret(true);
}

bool Sema::ensure_fully_resolved(ResolvedDecl &decl) {
    debug_func(decl.location << " " << decl.className() << " " << decl.name() << " " << decl.state);
    if (decl.state == ResolvedState::Error) return debug_ret(false);
    if (decl.state == ResolvedState::FullyResolved) return debug_ret(true);
    if (decl.state == ResolvedState::InProgress) return debug_ret(true);

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
        if ((!vr->type && vr->parentDeclStmt) || (vr->parentDeclStmt && !vr->parentDeclStmt->type)) {
            success = ensure_fully_resolved(*vr->parentDeclStmt);
        }
    } else if (auto *fn = dynamic_cast<ResolvedFunctionDecl *>(&decl)) {
        if (dynamic_cast<ResolvedBuiltinFunctionDecl *>(fn)) {
            success = true;
        } else if (fn->functionDecl && fn->functionDecl->body && !fn->body) {
            if (fn->parentDecl && !ensure_struct_bodies_resolved(*fn->parentDecl)) {
                success = false;
            } else {
                success = resolve_func_body(*fn, *fn->functionDecl->body);
            }
        }
    } else if (auto *mod = dynamic_cast<ResolvedModuleDecl *>(&decl)) {
        success = resolve_module_body(*mod);
    }

    if (success) {
        decl.state = ResolvedState::FullyResolved;
    } else {
        decl.state = ResolvedState::Error;
    }
    return debug_ret(success);
}

bool Sema::ensure_fully_resolved(ResolvedType &type) {
    debug_func(type.location << " " << type.className() << " " << type.to_str());

    if (auto structDecl = dynamic_cast<ResolvedTypeStructDecl *>(&type)) {
        return ensure_fully_resolved(*structDecl->decl);
    } else if (auto strcut = dynamic_cast<ResolvedTypeStruct *>(&type)) {
        return ensure_fully_resolved(*strcut->decl);
    }

    return debug_ret(true);
}

bool Sema::resolve_module_body(ResolvedModuleDecl &moduleDecl) {
    debug_func("resolve_module_body for " << moduleDecl.module_path);
    bool error = false;
    ScopeRAII moduleScope(*this, moduleDecl.scope.get());

    debug_msg(moduleDecl.module_path << " " << m_driver.m_options.source);
    debug_msg("Source module: resolving entry points only");
    for (size_t i = 0; i < moduleDecl.declarations.size(); i++) {
        auto currentDecl = moduleDecl.declarations[i].get();

        if (!ensure_fully_resolved(*currentDecl)) {
            error = true;
            continue;
        }
    }
    debug_msg("error " << error);
    if (error) return false;

    moduleDecl.state = ResolvedState::FullyResolved;
    return true;
}

bool Sema::resolve_pending_body() {
    debug_func("");
    bool error = false;

    while (!m_pending_decls.empty()) {
        auto pending = std::move(m_pending_decls);
        for (auto *decl : pending) {
            if (!ensure_fully_resolved(*decl)) {
                error = true;
            }
        }
    }
    return !error;
}

bool Sema::resolve_func_body(ResolvedFunctionDecl &function, const Block &body) {
    debug_func("");

    if (function.state == ResolvedState::Error) return debug_ret(false);
    if (function.state == ResolvedState::FullyResolved) return debug_ret(true);

    ScopeRAII paramScope(*this, function.scope.get());

    if (auto resolvedBody = resolve_block(body)) {
        if (function.body) dmz_unreachable(function.location, "Function already have a body");
        function.body = std::move(resolvedBody);
        if (run_flow_sensitive_checks(function)) return false;
        debug_msg("true");
        function.state = ResolvedState::FullyResolved;
        return true;
    }

    function.state = ResolvedState::Error;
    debug_msg("false");
    return false;
}
}  // namespace DMZ