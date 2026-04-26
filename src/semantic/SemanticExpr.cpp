#include <algorithm>
#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif

#include "Debug.hpp"
#include "Utils.hpp"
#include "codegen/CodegenUtils.hpp"
#include "driver/Driver.hpp"
#include "parser/ParserSymbols.hpp"
#include "semantic/Semantic.hpp"
#include "semantic/SemanticSymbols.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

bool op_generate_bool(TokenType op) {
    static const std::unordered_set<TokenType> op_bool = {
        TokenType::op_excla_mark, TokenType::op_less,    TokenType::op_less_eq,
        TokenType::op_more,       TokenType::op_more_eq, TokenType::op_equal,
        TokenType::op_not_equal,  TokenType::pipepipe,   TokenType::ampamp,
    };
    return op_bool.count(op) != 0;
}

ptr<ResolvedGenericExpr> Sema::resolve_generic_expr(const GenericExpr &genericExpr) {
    debug_func(genericExpr.location);
    varOrReturn(resolvedBase, resolve_expr(*genericExpr.base));

    varOrReturn(specializedType, resolve_specialized_type(genericExpr));
    ResolvedDecl *declToGeneric = nullptr;
    ResolvedDecl *decl = nullptr;
    if (auto memExpr = dynamic_cast<ResolvedMemberExpr *>(resolvedBase.get())) {
        declToGeneric = const_cast<ResolvedDecl *>(&memExpr->member);
    } else if (auto declRef = dynamic_cast<ResolvedDeclRefExpr *>(resolvedBase.get())) {
        declToGeneric = const_cast<ResolvedDecl *>(&declRef->decl);
    } else {
        resolvedBase->dump();
        dmz_unreachable(resolvedBase->location, "unexpected base expresion in generic expresion");
    }

    if (declToGeneric && (declToGeneric->type->kind == ResolvedTypeKind::StructDecl ||
                          declToGeneric->type->kind == ResolvedTypeKind::Function)) {
        if (auto strType = dynamic_cast<ResolvedTypeStructDecl *>(declToGeneric->type.get())) {
            declToGeneric = strType->decl;
        } else if (auto fnType = dynamic_cast<ResolvedTypeFunction *>(declToGeneric->type.get())) {
            declToGeneric = fnType->fnDecl;
        }
    }

    if (!declToGeneric) {
        resolvedBase->dump();
        genericExpr.dump();
        specializedType->dump();
        dmz_unreachable(genericExpr.location, "unexpected there are no decl to specialize");
    }

    if (auto structDecl = dynamic_cast<ResolvedGenericStructDecl *>(declToGeneric)) {
        decl = specialize_generic_struct(genericExpr.location, *structDecl, *specializedType);
        if (!decl) {
            decl = structDecl;
        }
    } else if (auto functionDecl = dynamic_cast<ResolvedGenericFunctionDecl *>(declToGeneric)) {
        decl = specialize_generic_function(genericExpr.location, *functionDecl, *specializedType);
        if (!decl) {
            decl = functionDecl;
        }
    } else {
        declToGeneric->type->dump();
        declToGeneric->dump();
        resolvedBase->dump();
        return report(genericExpr.location, "cannot specialize a non generic decl '" + resolvedBase->type->to_str() +
                                                "' with " + genericExpr.to_str());
    }

    std::vector<ptr<ResolvedTypeExpr>> specializedTypesExpr;
    for (auto &&t : genericExpr.types) {
        varOrReturn(resExpr, resolve_type_expr(*t));
        specializedTypesExpr.emplace_back(std::move(resExpr));
    }

    if (!decl) {
        resolvedBase->dump();
        genericExpr.dump();
        specializedType->dump();
        // dmz_unreachable("FIX");
        return report(genericExpr.location,
                      "cannot specialize '" + resolvedBase->type->to_str() + "' with " + genericExpr.to_str());
    } else {
        return makePtr<ResolvedGenericExpr>(genericExpr.location, std::move(resolvedBase), *decl,
                                            std::move(specializedType), std::move(specializedTypesExpr));
    }
}

ptr<ResolvedDeclRefExpr> Sema::resolve_decl_ref_expr(const DeclRefExpr &declRefExpr) {
    debug_func(declRefExpr.location);
    // Search in the module scope
    ResolvedDecl *decl = lookup(declRefExpr.location, declRefExpr.identifier);
    if (!decl && declRefExpr.identifier.starts_with("@")) {
        decl = resolve_builtin_function_symbol(declRefExpr, declRefExpr.identifier);
    }

    if (!decl) {
#ifdef DEBUG
        dump_scopes();
#endif
        return report(declRefExpr.location, "expression symbol '" + declRefExpr.identifier + "' not found");
    }

    if (!decl->type && !ensure_fully_resolved(*decl)) return nullptr;

    debug_msg("Adding decl ref " << decl->name() << " to pending decls");
    m_pending_decls.emplace(decl);
    if (!decl->type) {
        decl->dump();
        return report(declRefExpr.location, "could not resolve type for '" + declRefExpr.identifier + "'");
    }

    debug_msg("Resolving decl ref " << declRefExpr.identifier << " with type "
                                    << (decl->type ? decl->type->className() : "NULL") << " "
                                    << (decl->type ? decl->type->to_str() : "NULL"));
    auto type = re_resolve_type(*decl->type);
    if (!type) {
        decl->dump();
        dmz_unreachable(declRefExpr.location, "unreachable");
    }
    auto resolvedDeclRefExpr =
        makePtr<ResolvedDeclRefExpr>(declRefExpr.location, declRefExpr.identifier, *decl, std::move(type));

    resolvedDeclRefExpr->set_constant_value(cee.evaluate(*resolvedDeclRefExpr, false));

    return resolvedDeclRefExpr;
}

ptr<ResolvedTypeSpecialized> Sema::infer_generic_types(const SourceLocation &location,
                                                       ResolvedGenericFunctionDecl &funcDecl,
                                                       const std::vector<ptr<ResolvedExpr>> &arguments) {
    debug_func(location);
    std::unordered_map<ResolvedGenericTypeDecl *, ptr<ResolvedType>> inferredTypes;

    for (size_t i = 0; i < funcDecl.params.size() && i < arguments.size(); ++i) {
        if (!internal_infer_type(inferredTypes, *funcDecl.params[i]->type, *arguments[i]->type)) {
            return report(arguments[i]->location, "type mismatch during generic inference: expected '" +
                                                      funcDecl.params[i]->type->to_str() + "', actual '" +
                                                      arguments[i]->type->to_str() + "'");
        }
    }

    std::vector<ptr<ResolvedType>> specializedTypes;
    for (auto &&gtDecl : funcDecl.genericTypeDecls) {
        if (inferredTypes.count(gtDecl.get())) {
            debug_msg(location << " func " << funcDecl.name() << " inferred type for '" << gtDecl->identifier
                               << "' is '" << inferredTypes[gtDecl.get()]->to_str() << "'");
            specializedTypes.emplace_back(inferredTypes[gtDecl.get()]->clone());
        } else {
            return report(location, "could not infer generic type for '" + gtDecl->identifier + "'");
        }
    }

    return makePtr<ResolvedTypeSpecialized>(location, std::move(specializedTypes));
}

bool Sema::internal_infer_type(std::unordered_map<ResolvedGenericTypeDecl *, ptr<ResolvedType>> &inferredTypes,
                               const ResolvedType &paramType, const ResolvedType &argType) {
    if (auto genType = dynamic_cast<const ResolvedTypeGeneric *>(&paramType)) {
        if (inferredTypes.count(genType->decl)) {
            return inferredTypes[genType->decl]->compare(argType);
        }
        inferredTypes[genType->decl] = argType.clone();
        return true;
    }

    if (paramType.kind == ResolvedTypeKind::Pointer && argType.kind == ResolvedTypeKind::Pointer) {
        return internal_infer_type(inferredTypes, *static_cast<const ResolvedTypePointer &>(paramType).pointerType,
                                   *static_cast<const ResolvedTypePointer &>(argType).pointerType);
    }

    if (paramType.kind == ResolvedTypeKind::Slice && argType.kind == ResolvedTypeKind::Slice) {
        return internal_infer_type(inferredTypes, *static_cast<const ResolvedTypeSlice &>(paramType).sliceType,
                                   *static_cast<const ResolvedTypeSlice &>(argType).sliceType);
    }

    if (paramType.kind == ResolvedTypeKind::Optional && argType.kind == ResolvedTypeKind::Optional) {
        return internal_infer_type(inferredTypes, *static_cast<const ResolvedTypeOptional &>(paramType).optionalType,
                                   *static_cast<const ResolvedTypeOptional &>(argType).optionalType);
    }

    if (paramType.kind == ResolvedTypeKind::Array && argType.kind == ResolvedTypeKind::Array) {
        return internal_infer_type(inferredTypes, *static_cast<const ResolvedTypeArray &>(paramType).arrayType,
                                   *static_cast<const ResolvedTypeArray &>(argType).arrayType);
    }

    return true;
}

ptr<ResolvedCallExpr> Sema::resolve_call_expr(const CallExpr &call) {
    debug_func(call.location);
    bool isMemberCall = false;
    ptr<ResolvedExpr> resolvedBase = nullptr;

    varOrReturn(resolvedCallee, resolve_expr(*call.callee));

    if (auto memberExpr = dynamic_cast<ResolvedMemberExpr *>(resolvedCallee.get())) {
        if (auto memFunc = dynamic_cast<const ResolvedMemberFunctionDecl *>(&memberExpr->member)) {
            isMemberCall = !memFunc->isStatic;
            if (isMemberCall) {
                resolvedBase = std::move(memberExpr->base);
                // Recreate resolvedCallee
                resolvedCallee = resolve_expr(*call.callee);
            }
        }
    }

    ResolvedTypeFunction *fnType = nullptr;
    auto functionType = resolvedCallee->type.get();
    if (functionType) {
        if (functionType->kind == ResolvedTypeKind::Function) {
            fnType = static_cast<ResolvedTypeFunction *>(functionType);
        } else if (auto ptrType = dynamic_cast<ResolvedTypePointer *>(functionType)) {
            if (ptrType->pointerType->kind == ResolvedTypeKind::Function) {
                fnType = static_cast<ResolvedTypeFunction *>(ptrType->pointerType.get());
            }
        }
    }

    if (!fnType) {
        if (functionType && functionType->kind == ResolvedTypeKind::Generic) {
            std::vector<ptr<ResolvedExpr>> resolvedArguments;
            for (auto &&arg : call.arguments) {
                varOrReturn(resolvedArg, resolve_expr(*arg));
                resolvedArguments.emplace_back(std::move(resolvedArg));
            }
            return makePtr<ResolvedCallExpr>(call.location, makePtr<ResolvedTypeGeneric>(call.location, nullptr),
                                             std::move(resolvedCallee), std::move(resolvedArguments));
        }
        // call.dump();
        return report(call.location, "calling non-function symbol");
    }

    bool errGeneric = false;
    errGeneric |= fnType->returnType->kind == ResolvedTypeKind::Generic;
    for (auto &&param : fnType->paramsTypes) {
        if (errGeneric) break;
        errGeneric |= param->kind == ResolvedTypeKind::Generic;
    }

    size_t call_args_num = call.arguments.size();
    size_t func_args_num = fnType->paramsTypes.size();
    if (isMemberCall) func_args_num -= 1;
    bool isVararg = (func_args_num != 0) ? fnType->paramsTypes.back()->kind == ResolvedTypeKind::VarArg : false;
    size_t funcDeclArgs = isVararg ? (func_args_num - 1) : func_args_num;

    std::vector<ptr<ResolvedExpr>> resolvedArguments;
    if (isMemberCall && resolvedBase) {
        ptr<ResolvedExpr> argsToAdd = nullptr;
        if (resolvedBase->type->kind == ResolvedTypeKind::Struct ||
            resolvedBase->type->kind == ResolvedTypeKind::Union || resolvedBase->type->kind == ResolvedTypeKind::Enum ||
            resolvedBase->type->kind == ResolvedTypeKind::Simd) {
            argsToAdd = makePtr<ResolvedRefPtrExpr>(resolvedBase->location, std::move(resolvedBase));
        } else if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(resolvedBase->type.get())) {
            if (ptrType->pointerType->kind == ResolvedTypeKind::Struct ||
                ptrType->pointerType->kind == ResolvedTypeKind::Union ||
                ptrType->pointerType->kind == ResolvedTypeKind::Enum ||
                ptrType->pointerType->kind == ResolvedTypeKind::Simd) {
                argsToAdd = std::move(resolvedBase);
            }
        }
        if (argsToAdd) {
            resolvedArguments.emplace_back(std::move(argsToAdd));
        }
    }

    for (auto &&arg : call.arguments) {
        varOrReturn(resolvedArg, resolve_expr(*arg));
        resolvedArg->set_constant_value(cee.evaluate(*resolvedArg, false));
        resolvedArguments.emplace_back(std::move(resolvedArg));
    }

    if (errGeneric && !dynamic_cast<const GenericExpr *>(call.callee.get())) {
        ResolvedGenericFunctionDecl *genFunc = nullptr;
        if (auto *resolvedDeclRefExpr = dynamic_cast<const ResolvedDeclRefExpr *>(resolvedCallee.get())) {
            genFunc =
                dynamic_cast<ResolvedGenericFunctionDecl *>(const_cast<ResolvedDecl *>(&resolvedDeclRefExpr->decl));
        }
        if (auto *resolvedMemberExpr = dynamic_cast<const ResolvedMemberExpr *>(resolvedCallee.get())) {
            genFunc =
                dynamic_cast<ResolvedGenericFunctionDecl *>(const_cast<ResolvedDecl *>(&resolvedMemberExpr->member));
        }

        if (genFunc) {
            varOrReturn(specializedTypes, infer_generic_types(call.location, *genFunc, resolvedArguments));
            auto specializedFunc = specialize_generic_function(call.location, *genFunc, *specializedTypes);
            if (!specializedFunc) {
                return report(call.location, "failed to specialize generic function");
            }

            // Re-resolve callee to point to the specialized function
            if (auto *resolvedDeclRefExpr = dynamic_cast<ResolvedDeclRefExpr *>(resolvedCallee.get())) {
                resolvedCallee =
                    makePtr<ResolvedDeclRefExpr>(resolvedDeclRefExpr->location, resolvedDeclRefExpr->identifier,
                                                 *specializedFunc, specializedFunc->type->clone());
            } else if (auto *resolvedMemberExpr = dynamic_cast<ResolvedMemberExpr *>(resolvedCallee.get())) {
                resolvedCallee = makePtr<ResolvedMemberExpr>(resolvedMemberExpr->location,
                                                             std::move(resolvedMemberExpr->base), *specializedFunc);
            }

            fnType = specializedFunc->getFnType();
            func_args_num = fnType->paramsTypes.size();
            if (isMemberCall) func_args_num -= 1;
            isVararg = (func_args_num != 0) ? fnType->paramsTypes.back()->kind == ResolvedTypeKind::VarArg : false;
            funcDeclArgs = isVararg ? (func_args_num - 1) : func_args_num;
        } else {
            // is not really generic function but has generic types, maybe a function pointer?
            // for now keep the error
            // return report(call.location, "try to call a generic function without specialization");
            // dmz_unreachable("TODO: not generic function but has generic types " + call.location.to_string());
        }
    }

    if (call_args_num != func_args_num) {
        if (!isVararg || (isVararg && call_args_num < funcDeclArgs)) {
            return report(call.location, "argument count mismatch in function call, expected " +
                                             std::to_string(func_args_num) + " actual " +
                                             std::to_string(call_args_num));
        }
    }

    if (isMemberCall) {
        if (resolvedArguments.size() == 0) {
            call.dump();
            dmz_unreachable(call.location, "unexpected member call without member pass as argument");
        }
    }

    ptr<ResolvedTypeFunction> resolvedFnType = nullptr;
    if (auto *resolvedDeclRefExpr = dynamic_cast<ResolvedDeclRefExpr *>(resolvedCallee.get())) {
        if (auto *builtinFn = dynamic_cast<const ResolvedBuiltinFunctionDecl *>(&resolvedDeclRefExpr->decl)) {
            resolvedFnType = resolve_builtin_function_expr(
                *resolvedCallee, *const_cast<ResolvedBuiltinFunctionDecl *>(builtinFn), resolvedArguments);
            if (!resolvedFnType) return nullptr;
            fnType = resolvedFnType.get();
        }
    }

    debug_msg(Dumper([&]() { resolvedCallee->dump(); }));
    debug_msg(call.location << " funcDeclArgs " << funcDeclArgs << " call_args_num " << call_args_num);
    for (size_t i = 0; i < call_args_num; ++i) {
        if (i < funcDeclArgs) {
            size_t paramIdx = isMemberCall ? i + 1 : i;
            if (!perform_implicit_cast(resolvedArguments[paramIdx], *fnType->paramsTypes[paramIdx])) return nullptr;
            if (!fnType->paramsTypes[paramIdx]->compare(*resolvedArguments[paramIdx]->type)) {
                return report(resolvedArguments[paramIdx]->location,
                              "unexpected type of argument '" + resolvedArguments[paramIdx]->type->to_str() +
                                  "' expected '" + fnType->paramsTypes[paramIdx]->to_str() + "'");
            }
        }
    }

    return makePtr<ResolvedCallExpr>(call.location, fnType->returnType->clone(), std::move(resolvedCallee),
                                     std::move(resolvedArguments));
}

ptr<ResolvedExpr> Sema::resolve_expr(const Expr &expr, bool isType) {
    debug_func((m_currentModule ? m_currentModule->module_path : "<no module>") << " " << expr.location);
    if (const auto *number = dynamic_cast<const IntLiteral *>(&expr)) {
        return makePtr<ResolvedIntLiteral>(number->location, std::stod(number->value));
    }
    if (const auto *number = dynamic_cast<const FloatLiteral *>(&expr)) {
        return makePtr<ResolvedFloatLiteral>(number->location, std::stod(number->value));
    }
    if (const auto *character = dynamic_cast<const CharLiteral *>(&expr)) {
        if (auto c = str_from_source(character->value.substr(1, character->value.size() - 1))) {
            return makePtr<ResolvedCharLiteral>(character->location, (*c)[0]);
        } else {
            return report(character->location, "malformed char");
        }
    }
    if (const auto *boolean = dynamic_cast<const BoolLiteral *>(&expr)) {
        return makePtr<ResolvedBoolLiteral>(boolean->location, boolean->value == "true");
    }
    if (const auto *str = dynamic_cast<const StringLiteral *>(&expr)) {
        if (auto c = str_from_source(str->value.substr(1, str->value.size() - 2))) {
            return makePtr<ResolvedStringLiteral>(str->location, *c);
        } else {
            return report(str->location, "malformed string");
        }
    }
    if (const auto *null = dynamic_cast<const NullLiteral *>(&expr)) {
        return makePtr<ResolvedNullLiteral>(null->location);
    }
    if (const auto *genericExpr = dynamic_cast<const GenericExpr *>(&expr)) {
        return resolve_generic_expr(*genericExpr);
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
        return resolve_unary_operator(*unaryOperator, isType);
    }
    if (const auto *refPtrExpr = dynamic_cast<const RefPtrExpr *>(&expr)) {
        return resolve_ref_ptr_expr(*refPtrExpr);
    }
    if (const auto *derefPtrExpr = dynamic_cast<const DerefPtrExpr *>(&expr)) {
        return resolve_deref_ptr_expr(*derefPtrExpr);
    }
    if (const auto *structInstantiation = dynamic_cast<const StructInstantiationExpr *>(&expr)) {
        return resolve_struct_instantiation(*structInstantiation);
    }
    if (const auto *tupleInstantiation = dynamic_cast<const TupleInstantiationExpr *>(&expr)) {
        return resolve_tuple_instantiation(*tupleInstantiation);
    }
    if (const auto *arrayInstantiation = dynamic_cast<const ArrayInstantiationExpr *>(&expr)) {
        return resolve_array_instantiation(*arrayInstantiation);
    }
    if (const auto *assignableExpr = dynamic_cast<const AssignableExpr *>(&expr)) {
        return resolve_assignable_expr(*assignableExpr, isType);
    }
    if (const auto *catchErrorExpr = dynamic_cast<const CatchErrorExpr *>(&expr)) {
        return resolve_catch_error_expr(*catchErrorExpr);
    }
    if (const auto *tryErrorExpr = dynamic_cast<const TryErrorExpr *>(&expr)) {
        return resolve_try_error_expr(*tryErrorExpr);
    }
    if (const auto *orelseExpr = dynamic_cast<const OrElseErrorExpr *>(&expr)) {
        return resolve_orelse_error_expr(*orelseExpr);
    }
    if (const auto *importExpr = dynamic_cast<const ImportExpr *>(&expr)) {
        return resolve_import_expr(*importExpr);
    }
    if (const auto *errorGroupExprDecl = dynamic_cast<const ErrorGroupExprDecl *>(&expr)) {
        return resolve_error_group_expr_decl(*errorGroupExprDecl);
    }
    if (const auto *errorInPlaceExpr = dynamic_cast<const ErrorInPlaceExpr *>(&expr)) {
        return makePtr<ResolvedErrorInPlaceExpr>(errorInPlaceExpr->location, errorInPlaceExpr->identifier);
    }
    if (const auto *rangeExpr = dynamic_cast<const RangeExpr *>(&expr)) {
        return resolve_range_expr(*rangeExpr);
    }
    if (const auto *autoMember = dynamic_cast<const AutoMemberExpr *>(&expr)) {
        return makePtr<ResolvedAutoMemberExpr>(autoMember->location, autoMember->field);
    }
    if (dynamic_cast<const Type *>(&expr)) {
        varOrReturn(resolvedType, resolve_type(expr));
        return makePtr<ResolvedTypeExpr>(expr.location, std::move(resolvedType));
    }
    expr.dump();
    dmz_unreachable(expr.location, "unexpected expression " + std::string(expr.className()));
}

ptr<ResolvedTypeExpr> Sema::resolve_type_expr(const Expr &expr) {
    debug_func(expr.location);
    varOrReturn(resolvedType, resolve_type(expr));
    ptr<ResolvedExpr> resolvedExpr = nullptr;

    if (const auto *typeExpr = dynamic_cast<const Type *>(&expr)) {
        if (auto typePtr = dynamic_cast<const TypePointer *>(typeExpr)) {
            debug_msg("TypePointer" << expr.location);
            varOrReturn(resolvedTypePtr, resolve_type_expr(*typePtr->pointerType));
            resolvedExpr =
                makePtr<ResolvedTypePointerExpr>(typePtr->location, resolvedType->clone(), std::move(resolvedTypePtr));
        }
        if (auto typeSlice = dynamic_cast<const TypeSlice *>(typeExpr)) {
            debug_msg("TypeSlice" << expr.location);
            varOrReturn(child, resolve_type_expr(*typeSlice->sliceType));
            resolvedExpr = makePtr<ResolvedTypeSliceExpr>(typeSlice->location, resolvedType->clone(), std::move(child));
        }
        if (auto typeOptional = dynamic_cast<const TypeOptional *>(typeExpr)) {
            debug_msg("TypeOptional" << expr.location);
            varOrReturn(child, resolve_type_expr(*typeOptional->optionalType));
            resolvedExpr =
                makePtr<ResolvedTypeOptionalExpr>(typeOptional->location, resolvedType->clone(), std::move(child));
        }
        if (auto typeArray = dynamic_cast<const TypeArray *>(typeExpr)) {
            debug_msg("TypeArray" << expr.location);
            varOrReturn(arrayType, resolve_type_expr(*typeArray->arrayType));
            varOrReturn(arraySize, resolve_expr(*typeArray->arraySize));
            resolvedExpr = makePtr<ResolvedTypeArrayExpr>(typeArray->location, resolvedType->clone(),
                                                          std::move(arrayType), std::move(arraySize));
        }
        if (auto typeVec = dynamic_cast<const TypeSimd *>(typeExpr)) {
            debug_msg("TypeSimd" << expr.location);
            varOrReturn(simdType, resolve_type_expr(*typeVec->simdType));
            varOrReturn(simdSize, resolve_expr(*typeVec->simdSize));
            resolvedExpr = makePtr<ResolvedTypeSimdExpr>(typeVec->location, resolvedType->clone(), std::move(simdType),
                                                         std::move(simdSize));
        }
        if (auto typeFunc = dynamic_cast<const TypeFunction *>(typeExpr)) {
            debug_msg("TypeFunction" << expr.location);
            vec<ptr<ResolvedTypeExpr>> argTypes;
            for (const auto &argType : typeFunc->paramsTypes) {
                argTypes.push_back(resolve_type_expr(*argType));
            }
            varOrReturn(returnType, resolve_type_expr(*typeFunc->returnType));
            resolvedExpr = makePtr<ResolvedTypeFunctionExpr>(typeFunc->location, resolvedType->clone(),
                                                             std::move(argTypes), std::move(returnType));
        }
    }
    if (!resolvedExpr) {
        debug_msg("Resolve expr as type" << expr.location << " " << expr.className());
        resolvedExpr = resolve_expr(expr, true);
    }
    if (!resolvedExpr) return report(expr.location, "unexpected expression " + std::string(expr.className()));

    auto ret = makePtr<ResolvedTypeExpr>(expr.location, std::move(resolvedType), std::move(resolvedExpr));
    // ret->dump();
    return ret;
}

ptr<ResolvedUnaryOperator> Sema::resolve_unary_operator(const UnaryOperator &unary, bool isType) {
    debug_func(unary.location);

    varOrReturn(resolvedRHS, resolve_expr(*unary.operand, isType));

    auto boolType = ResolvedTypeBool{SourceLocation{}};

    if (unary.op == TokenType::op_excla_mark) {
        if (!isType && !boolType.compare(*resolvedRHS->type)) {
            return report(resolvedRHS->location,
                          '\'' + resolvedRHS->type->to_str() + "' cannot be used as an operand to unary operator '!'");
        }
    } else {
        if (isType) return report(unary.location, "unexpected op in unary operator of a type");
        if (resolvedRHS->type->kind == ResolvedTypeKind::Void)
            return report(resolvedRHS->location, '\'' + resolvedRHS->type->to_str() +
                                                     "' cannot be used as an operand to unary operator '" +
                                                     get_op_str(unary.op) + "'");
    }

    ptr<DMZ::ResolvedType> resolvedType = nullptr;
    if (isType && unary.op == TokenType::op_excla_mark) {
        ptr<ResolvedType> innerType;
        if (auto te = dynamic_cast<ResolvedTypeExpr *>(resolvedRHS.get())) {
            innerType = te->resolvedType->clone();
        } else {
            varOrReturn(it, resolve_type(*unary.operand));
            innerType = std::move(it);
        }
        resolvedType = makePtr<ResolvedTypeOptional>(unary.location, std::move(innerType));
    } else if (op_generate_bool(unary.op)) {
        resolvedType = boolType.clone();
    } else if (unary.op == TokenType::amp) {
        resolvedType = makePtr<ResolvedTypePointer>(resolvedRHS->type->location, resolvedRHS->type->clone());
    } else {
        resolvedType = resolvedRHS->type->clone();
    }
    return makePtr<ResolvedUnaryOperator>(unary.location, std::move(resolvedType), unary.op, std::move(resolvedRHS));
}

ptr<ResolvedRefPtrExpr> Sema::resolve_ref_ptr_expr(const RefPtrExpr &refPtrExpr) {
    debug_func(refPtrExpr.location);
    varOrReturn(resolvedExpr, resolve_expr(*refPtrExpr.expr));
    return makePtr<ResolvedRefPtrExpr>(refPtrExpr.location, std::move(resolvedExpr));
}

ptr<ResolvedDerefPtrExpr> Sema::resolve_deref_ptr_expr(const DerefPtrExpr &derefPtrExpr) {
    debug_func(derefPtrExpr.location);
    varOrReturn(resolvedExpr, resolve_expr(*derefPtrExpr.expr));
    auto ptrType = dynamic_cast<const ResolvedTypePointer *>(resolvedExpr->type.get());
    if (!ptrType)
        return report(resolvedExpr->location, "expected pointer type, actual '" + resolvedExpr->type->to_str() + "'");
    return makePtr<ResolvedDerefPtrExpr>(derefPtrExpr.location, ptrType->pointerType->clone(), std::move(resolvedExpr));
}

ptr<ResolvedBinaryOperator> Sema::resolve_binary_operator(const BinaryOperator &binop) {
    debug_func(binop.location);
    varOrReturn(resolvedLHS, resolve_expr(*binop.lhs));
    varOrReturn(resolvedRHS, resolve_expr(*binop.rhs));

    auto lhsKind = resolvedLHS->type->kind;
    auto rhsKind = resolvedRHS->type->kind;
    if (lhsKind != ResolvedTypeKind::Number && lhsKind != ResolvedTypeKind::Bool &&
        lhsKind != ResolvedTypeKind::Error && lhsKind != ResolvedTypeKind::Pointer &&
        lhsKind != ResolvedTypeKind::Simd && lhsKind != ResolvedTypeKind::Generic) {
        return report(resolvedLHS->location,
                      '\'' + resolvedLHS->type->to_str() + "' cannot be used as LHS operand to binary operator");
    }
    if (rhsKind != ResolvedTypeKind::Number && rhsKind != ResolvedTypeKind::Bool &&
        rhsKind != ResolvedTypeKind::Error && rhsKind != ResolvedTypeKind::Pointer &&
        rhsKind != ResolvedTypeKind::Simd && rhsKind != ResolvedTypeKind::Generic) {
        return report(resolvedRHS->location,
                      '\'' + resolvedRHS->type->to_str() + "' cannot be used as RHS operand to binary operator");
    }
    if (!resolvedLHS->type->compare(*resolvedRHS->type)) {
        return report(binop.location, "unexpected type in binop, expected '" + resolvedLHS->type->to_str() +
                                          "' actual '" + resolvedRHS->type->to_str() + "' ");
    }

    auto ret =
        makePtr<ResolvedBinaryOperator>(binop.location, binop.op, std::move(resolvedLHS), std::move(resolvedRHS));
    if (op_generate_bool(binop.op)) {
        if (auto simdType = dynamic_cast<ResolvedTypeSimd *>(ret->type.get())) {
            simdType->simdType = makePtr<ResolvedTypeBool>(simdType->location);
        } else {
            ret->type = makePtr<ResolvedTypeBool>(binop.location);
        }
    }
    return ret;
}

ptr<ResolvedGroupingExpr> Sema::resolve_grouping_expr(const GroupingExpr &grouping) {
    debug_func(grouping.location);
    varOrReturn(resolvedExpr, resolve_expr(*grouping.expr));
    return makePtr<ResolvedGroupingExpr>(grouping.location, std::move(resolvedExpr));
}

ptr<ResolvedAssignableExpr> Sema::resolve_assignable_expr(const AssignableExpr &assignableExpr, bool isType) {
    debug_func(assignableExpr.location);
    if (const auto *declRefExpr = dynamic_cast<const DeclRefExpr *>(&assignableExpr))
        return resolve_decl_ref_expr(*declRefExpr);

    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(&assignableExpr))
        return resolve_member_expr(*memberExpr);

    if (const auto *arrayAtExpr = dynamic_cast<const ArrayAtExpr *>(&assignableExpr))
        return resolve_array_at_expr(*arrayAtExpr, isType);

    if (const auto *derefExpr = dynamic_cast<const DerefPtrExpr *>(&assignableExpr))
        return resolve_deref_ptr_expr(*derefExpr);

    assignableExpr.dump();
    dmz_unreachable(assignableExpr.location, "unexpected assignable expression");
}

ptr<ResolvedMemberExpr> Sema::resolve_member_expr(const MemberExpr &memberExpr) {
    debug_func(memberExpr.location);
    static ResolvedStructDecl sliceDecl = [this] {
        std::vector<ptr<ResolvedFieldDecl>> sliceFields;
        sliceFields.emplace_back(makePtr<ResolvedFieldDecl>(
            SourceLocation{}, "ptr",
            makePtr<ResolvedTypeExpr>(
                SourceLocation{},
                makePtr<ResolvedTypePointer>(SourceLocation{}, makePtr<ResolvedTypeVoid>(SourceLocation{}))),
            0, nullptr));
        sliceFields.emplace_back(makePtr<ResolvedFieldDecl>(
            SourceLocation{}, "len",
            makePtr<ResolvedTypeExpr>(
                SourceLocation{},
                makePtr<ResolvedTypeNumber>(SourceLocation{}, ResolvedNumberKind::UInt, CodegenUtils::ptrBitSize())),
            1, nullptr));
        ScopeRAII sliceScope(*this);
        return ResolvedStructDecl(SourceLocation{}, true, "slice", nullptr, false, std::move(sliceFields),
                                  std::vector<ptr<ResolvedMemberFunctionDecl>>{}, sliceScope.takeScope());
    }();
    const ResolvedDecl *decl = nullptr;
    auto resolvedBase = resolve_expr(*memberExpr.base);
    if (!resolvedBase) return nullptr;

    if (memberExpr.field.empty()) {
        static ResolvedFieldDecl dummyCompletionField(
            SourceLocation{}, "",
            makePtr<ResolvedTypeExpr>(SourceLocation{}, makePtr<ResolvedTypeVoid>(SourceLocation{})), 0, nullptr);
        ResolvedType *baseType = resolvedBase->type.get();
        if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(baseType)) {
            baseType = ptrType->pointerType.get();
            resolvedBase =
                makePtr<ResolvedDerefPtrExpr>(memberExpr.location, baseType->clone(), std::move(resolvedBase));
        }
        return makePtr<ResolvedMemberExpr>(memberExpr.location, std::move(resolvedBase), dummyCompletionField);
    }
    bool baseIsPointer = false;
    ResolvedType *baseType = resolvedBase->type.get();
    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(baseType)) {
        baseType = ptrType->pointerType.get();
        baseIsPointer = true;
        if (baseType->kind == ResolvedTypeKind::Pointer) {
            return report(memberExpr.location, "unable to access member of a ptr of a ptr '" +
                                                   resolvedBase->type->to_str() + "', deref with ptr.*");
        }
    }

    if (dynamic_cast<const ResolvedTypeStructDecl *>(baseType) || dynamic_cast<const ResolvedTypeStruct *>(baseType)) {
        ResolvedStructDecl *struTypeDecl = nullptr;
        bool isDecl = false;
        bool isUnion = false;
        bool isEnum = false;
        if (auto st = dynamic_cast<const ResolvedTypeUnion *>(baseType)) {
            struTypeDecl = st->decl;
            isUnion = true;
            isDecl = false;
        } else if (auto st = dynamic_cast<const ResolvedTypeUnionDecl *>(baseType)) {
            struTypeDecl = st->decl;
            isDecl = true;
            isUnion = true;
        } else if (auto st = dynamic_cast<const ResolvedTypeEnumDecl *>(baseType)) {
            struTypeDecl = st->decl;
            isDecl = true;
            isEnum = true;
        } else if (auto st = dynamic_cast<const ResolvedTypeEnum *>(baseType)) {
            struTypeDecl = st->decl;
            isDecl = false;
            isEnum = true;
        } else if (auto st = dynamic_cast<const ResolvedTypeStruct *>(baseType)) {
            struTypeDecl = st->decl;
            isDecl = false;
        } else if (auto st = dynamic_cast<const ResolvedTypeStructDecl *>(baseType)) {
            struTypeDecl = st->decl;
            isDecl = true;
        } else {
            dmz_unreachable(memberExpr.location, "unexpected type");
        }
        if (isDecl && baseType->is_generic()) {
            bool skip = false;
            if (m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction)) {
                skip = true;
            }
            if (m_currentStruct && dynamic_cast<ResolvedGenericStructDecl *>(m_currentStruct)) {
                skip = true;
            }

            if (!skip) {
                return report(memberExpr.location,
                              "cannot use generic struct '" + baseType->to_str() + "' without specialization");
            }
        }
        decl = lookup_in_struct(memberExpr.location, *struTypeDecl, memberExpr.field);
        if (!decl) {
            return report(memberExpr.location, "struct \'" + resolvedBase->type->to_str() + "' has no member called '" +
                                                   memberExpr.field + '\'');
        }

        if (auto memberFunc = dynamic_cast<const ResolvedMemberFunctionDecl *>(decl)) {
            if (!isDecl && memberFunc->isStatic) {
                return report(memberExpr.location, "cannot use static member with an instance of struct '" +
                                                       resolvedBase->type->to_str() + "'");
            }
        } else if (dynamic_cast<const ResolvedFieldDecl *>(decl)) {
            if (isDecl && !isUnion && !isEnum) {
                return report(memberExpr.location,
                              "cannot access non-static field '" + memberExpr.field + "' without an instance");
            }
        }
    } else if (dynamic_cast<const ResolvedTypeSlice *>(baseType)) {
        if (memberExpr.field == "ptr") {
            decl = sliceDecl.fields[0].get();
        } else if (memberExpr.field == "len") {
            decl = sliceDecl.fields[1].get();
        } else {
            return report(memberExpr.location, "slice only support 'len' and 'ptr' members");
        }
    } else if (auto modType = dynamic_cast<const ResolvedTypeModule *>(baseType)) {
        auto moduleDecl = modType->moduleDecl;
        if (!moduleDecl)
            return report(resolvedBase->location, "expected not null the decl in type to be a module decl");
        // moduleDecl->dump();
        decl = lookup_in_module(memberExpr.location, *moduleDecl, memberExpr.field);
        if (!decl) {
            return report(memberExpr.location, "module \'" + resolvedBase->type->to_str() + "' has no member called '" +
                                                   memberExpr.field + '\'');
        }

    } else if (auto modType = dynamic_cast<const ResolvedTypeErrorGroup *>(baseType)) {
        auto errorGroupDecl = modType->decl;
        if (!errorGroupDecl)
            return report(resolvedBase->location, "expected not null the decl in type to be a error group decl");
        for (auto &&err : errorGroupDecl->errors) {
            // println("Err " << err->identifier << " field " << memberExpr.field);
            if (err->identifier == memberExpr.field) {
                decl = err.get();
                break;
            }
        }
        if (!decl) return report(memberExpr.location, "error group has no member called '" + memberExpr.field + '\'');
    } else if (baseType->kind == ResolvedTypeKind::Generic) {
        // Return a dummy member expression for generic types to allow LSP highlighting
        static std::unordered_map<std::string, ptr<ResolvedFieldDecl>> genericFields;
        if (genericFields.find(memberExpr.field) == genericFields.end()) {
            genericFields[memberExpr.field] = makePtr<ResolvedFieldDecl>(
                SourceLocation{}, memberExpr.field,
                makePtr<ResolvedTypeExpr>(SourceLocation{}, makePtr<ResolvedTypeGeneric>(SourceLocation{}, nullptr)), 0,
                nullptr);
        }
        return makePtr<ResolvedMemberExpr>(memberExpr.location, std::move(resolvedBase),
                                           *genericFields[memberExpr.field]);
    } else {
        return report(memberExpr.base->location, "cannot access member of '" + resolvedBase->type->to_str() + '\'');
    }
    // Implicit deref of the pointer in members
    if (baseIsPointer) {
        resolvedBase = makePtr<ResolvedDerefPtrExpr>(memberExpr.location, baseType->clone(), std::move(resolvedBase));
    }
    auto res = makePtr<ResolvedMemberExpr>(memberExpr.location, std::move(resolvedBase), *decl);
    if (auto unionDecl = dynamic_cast<ResolvedTypeUnionDecl *>(res->base->type.get())) {
        if (dynamic_cast<const ResolvedFieldDecl *>(&res->member)) {
            res->type = unionDecl->unionDecl()->tag->type->clone();
        }
    }
    res->set_constant_value(cee.evaluate(*res, false));
    return res;
}

ptr<ResolvedAssignableExpr> Sema::resolve_array_at_expr(const ArrayAtExpr &arrayAtExpr, bool isType) {
    debug_func(arrayAtExpr.location);
    auto resolvedBase = resolve_expr(*arrayAtExpr.array, isType);
    if (!resolvedBase) return nullptr;

    if (resolvedBase->type->kind == ResolvedTypeKind::Generic) {
        return makePtr<ResolvedArrayAtExpr>(arrayAtExpr.location, resolvedBase->type->clone(), std::move(resolvedBase),
                                            nullptr);
    }

    if (resolvedBase->type->kind != ResolvedTypeKind::Array && resolvedBase->type->kind != ResolvedTypeKind::Pointer &&
        resolvedBase->type->kind != ResolvedTypeKind::Slice && resolvedBase->type->kind != ResolvedTypeKind::Simd &&
        isType) {
        varOrReturn(arrayType, resolve_type(arrayAtExpr));
        varOrReturn(index, resolve_expr(*arrayAtExpr.index));
        return makePtr<ResolvedArrayAtExpr>(arrayAtExpr.location, std::move(arrayType), std::move(resolvedBase),
                                            std::move(index));
    }
    ptr<ResolvedType> derefType = nullptr;
    if (auto arrType = dynamic_cast<const ResolvedTypeArray *>(resolvedBase->type.get())) {
        derefType = arrType->arrayType->clone();
    } else if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(resolvedBase->type.get())) {
        derefType = ptrType->pointerType->clone();
    } else if (auto sliceType = dynamic_cast<const ResolvedTypeSlice *>(resolvedBase->type.get())) {
        derefType = sliceType->sliceType->clone();
    } else if (auto vectorType = dynamic_cast<const ResolvedTypeSimd *>(resolvedBase->type.get())) {
        derefType = vectorType->simdType->clone();
    } else {
        dmz_unreachable(arrayAtExpr.location,
                        "TODO: type " + std::string(resolvedBase->type->className()) + " not supported");
    }

    varOrReturn(index, resolve_expr(*arrayAtExpr.index));
    if (dynamic_cast<ResolvedRangeExpr *>(index.get())) {
        derefType = makePtr<ResolvedTypeSlice>(index->location, std::move(derefType));
    }

    return makePtr<ResolvedArrayAtExpr>(arrayAtExpr.location, std::move(derefType), std::move(resolvedBase),
                                        std::move(index));
}

ptr<ResolvedExpr> Sema::resolve_struct_instantiation(const StructInstantiationExpr &structInstantiation) {
    debug_func(structInstantiation.location);

    varOrReturn(resolvedBase, resolve_expr(*structInstantiation.base));

    if (resolvedBase->type->kind == ResolvedTypeKind::UnionDecl ||
        resolvedBase->type->kind == ResolvedTypeKind::Union) {
        ResolvedUnionDecl *un = nullptr;
        if (auto unionDecl = dynamic_cast<ResolvedTypeUnionDecl *>(resolvedBase->type.get())) {
            un = unionDecl->unionDecl();
        } else if (auto unionType = dynamic_cast<ResolvedTypeUnion *>(resolvedBase->type.get())) {
            un = unionType->unionDecl();
        } else {
            dmz_unreachable(structInstantiation.location, "TODO: this should not happend");
        }

        // Lazy: ensure union members are resolved before accessing fields
        if (!ensure_struct_members_resolved(*un)) return nullptr;

        if (structInstantiation.fieldInitializers.size() != 1) {
            return report(structInstantiation.location, "union instantiation must initialize exactly one field");
        }

        auto &&initStmt = structInstantiation.fieldInitializers[0];
        std::string &id = initStmt->identifier;
        const SourceLocation &loc = initStmt->location;

        const ResolvedFieldDecl *fieldDecl = nullptr;
        for (auto &&f : un->fields) {
            if (f->identifier == id) {
                fieldDecl = f.get();
                break;
            }
        }

        if (!fieldDecl) {
            return report(loc, "'" + un->identifier + "' has no field named '" + id + "'");
        }

        varOrReturn(resolvedInitExpr, resolve_expr(*initStmt->initializer));

        if (!perform_implicit_cast(resolvedInitExpr, *fieldDecl->type)) return nullptr;
        if (!fieldDecl->type->compare(*resolvedInitExpr->type)) {
            return report(resolvedInitExpr->location, "'" + resolvedInitExpr->type->to_str() +
                                                          "' cannot be used to initialize a field of type '" +
                                                          fieldDecl->type->to_str() + "'");
        }

        auto init = makePtr<ResolvedFieldInitStmt>(loc, *fieldDecl, std::move(resolvedInitExpr));
        init->initializer->set_constant_value(cee.evaluate(*init->initializer, false));

        return makePtr<ResolvedUnionInstantiationExpr>(structInstantiation.location, *un, std::move(init));
    }

    if (resolvedBase->type->kind != ResolvedTypeKind::StructDecl) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }
    auto auxstruType = dynamic_cast<const ResolvedTypeStructDecl *>(resolvedBase->type.get());
    if (!auxstruType) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }
    auto &st = auxstruType->decl;
    if (!st) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }

    // Lazy: ensure struct members are resolved before accessing fields
    if (!ensure_struct_members_resolved(*st)) return nullptr;

    if (!dynamic_cast<const GenericExpr *>(structInstantiation.base.get()) &&
        dynamic_cast<ResolvedGenericStructDecl *>(st) && !dynamic_cast<ResolvedGenericStructDecl *>(m_currentStruct)) {
        return report(structInstantiation.location, "'" + st->name() + "' is a generic and need specialization");
    }

    std::vector<ptr<ResolvedFieldInitStmt>> resolvedFieldInits;
    std::unordered_map<std::string, const ResolvedFieldInitStmt *> inits;

    std::unordered_map<std::string, const ResolvedFieldDecl *> fields;
    for (auto &&fieldDecl : st->fields) fields[fieldDecl->identifier] = fieldDecl.get();

    bool error = false;
    for (auto &&initStmt : structInstantiation.fieldInitializers) {
        std::string &id = initStmt->identifier;
        const SourceLocation &loc = initStmt->location;
        debug_msg("Initialice " << id);

        if (inits.count(id)) {
            report(loc, "field '" + id + "' is already initialized");
            error = true;
            continue;
        }

        const ResolvedFieldDecl *fieldDecl = fields[id];
        if (!fieldDecl) {
            report(loc, "'" + st->identifier + "' has no field named '" + id + "'");
            error = true;
            continue;
        }

        auto resolvedInitExpr = resolve_expr(*initStmt->initializer);
        if (!resolvedInitExpr) {
            error = true;
            continue;
        }
        debug_msg("Resolved init expr:\n"
                  << Dumper([&]() {
                         resolvedInitExpr->dump();
                         if (auto dre = dynamic_cast<ResolvedDeclRefExpr *>(resolvedInitExpr.get())) {
                             debug_msg("Resolved init expr decl: " << &dre->decl << "\n"
                                                                   << Dumper([&]() { dre->decl.dump(); }));
                         }
                     }));

        if (!perform_implicit_cast(resolvedInitExpr, *fieldDecl->type)) return nullptr;
        if (!fieldDecl->type->compare(*resolvedInitExpr->type)) {
            report(resolvedInitExpr->location, "'" + resolvedInitExpr->type->to_str() +
                                                   "' cannot be used to initialize a field of type '" +
                                                   fieldDecl->type->to_str() + "'");
            error = true;
            continue;
        }

        auto init = makePtr<ResolvedFieldInitStmt>(loc, *fieldDecl, std::move(resolvedInitExpr));
        inits[id] = resolvedFieldInits.emplace_back(std::move(init)).get();
    }

    if (error) return nullptr;

    // Add the default initilizers if there was not initialized
    for (auto &&decl : st->structDecl->decls) {
        if (inits.count(decl->identifier)) continue;

        auto fieldDecl = dynamic_cast<const FieldDecl *>(decl.get());
        if (!fieldDecl) continue;
        if (fieldDecl->default_initializer) {
            debug_msg("Default initialice " << fieldDecl->identifier);
            const std::string &id = fieldDecl->identifier;
            ScopeRAII default_initializer(*this, st->scope.get());
            auto resolvedInitExpr = resolve_expr(*fieldDecl->default_initializer);
            if (!resolvedInitExpr) {
                error = true;
                continue;
            }

            const ResolvedFieldDecl *resolvedfieldDecl = fields[id];
            if (!resolvedfieldDecl) dmz_unreachable(structInstantiation.location, "field not found");
            auto init = makePtr<ResolvedFieldInitStmt>(fieldDecl->default_initializer->location, *resolvedfieldDecl,
                                                       std::move(resolvedInitExpr));
            inits[id] = resolvedFieldInits.emplace_back(std::move(init)).get();
        }
    }

    if (error) return nullptr;

    for (auto &&fieldDecl : st->fields) {
        if (!inits.count(fieldDecl->identifier)) {
            report(structInstantiation.location, "field '" + fieldDecl->identifier + "' is not initialized");
            error = true;
            continue;
        }
        debug_msg("Evaluate " << fieldDecl->identifier);
        auto &initStmt = inits[fieldDecl->identifier];
        if (!initStmt) dmz_unreachable(structInstantiation.location, "init not found");
        initStmt->initializer->set_constant_value(cee.evaluate(*initStmt->initializer, false));
    }

    if (error) return nullptr;

    return makePtr<ResolvedStructInstantiationExpr>(structInstantiation.location, std::move(resolvedBase), *st,
                                                    std::move(resolvedFieldInits), false);
}

ptr<ResolvedExpr> Sema::resolve_tuple_instantiation(const TupleInstantiationExpr &tupleInstantiation) {
    debug_func(tupleInstantiation.location);

    std::vector<ptr<ResolvedExpr>> resolvedElements;
    std::string tupleName = "tuple";

    for (auto &&element : tupleInstantiation.elements) {
        varOrReturn(resolvedElement, resolve_expr(*element));
        resolvedElement->set_constant_value(cee.evaluate(*resolvedElement, false));
        tupleName += "." + resolvedElement->type->to_str();
        resolvedElements.emplace_back(std::move(resolvedElement));
    }

    ResolvedStructDecl *structDeclPtr = nullptr;
    if (m_instantiatedTuples.count(tupleName)) {
        structDeclPtr = m_instantiatedTuples[tupleName];
    } else {
        std::vector<ptr<ResolvedFieldDecl>> tupleFields;
        unsigned index = 0;
        for (auto &&el : resolvedElements) {
            std::string fieldName = "elem" + std::to_string(index);
            tupleFields.emplace_back(makePtr<ResolvedFieldDecl>(
                el->location, fieldName, makePtr<ResolvedTypeExpr>(el->location, el->type->clone()), index, nullptr));
            index++;
        }
        ScopeRAII tupleScope(*this);
        auto structDecl = makePtr<ResolvedStructDecl>(
            tupleInstantiation.location, false, tupleName, nullptr, false, std::move(tupleFields),
            std::vector<ptr<ResolvedMemberFunctionDecl>>{}, tupleScope.takeScope());
        structDecl->isTuple = true;
        structDeclPtr = structDecl.get();
        m_instantiatedTuples[tupleName] = structDeclPtr;
        m_currentModule->declarations.emplace_back(std::move(structDecl));
    }

    std::vector<ptr<ResolvedFieldInitStmt>> resolvedFieldInits;
    for (unsigned i = 0; i < resolvedElements.size(); ++i) {
        resolvedFieldInits.emplace_back(makePtr<ResolvedFieldInitStmt>(
            resolvedElements[i]->location, *structDeclPtr->fields[i], std::move(resolvedElements[i])));
    }

    return makePtr<ResolvedStructInstantiationExpr>(tupleInstantiation.location, nullptr, *structDeclPtr,
                                                    std::move(resolvedFieldInits), true);
}

ptr<ResolvedExpr> Sema::resolve_array_instantiation(const ArrayInstantiationExpr &arrayInstantiation) {
    debug_func(arrayInstantiation.location);
    std::vector<ptr<ResolvedExpr>> resolvedinitializers;
    resolvedinitializers.reserve(arrayInstantiation.initializers.size());

    ptr<ResolvedType> type = makePtr<ResolvedTypeDefaultInit>(SourceLocation{});
    bool only_first = true;
    for (auto &&initializer : arrayInstantiation.initializers) {
        varOrReturn(resolvedExpr, resolve_expr(*initializer));

        if (!only_first) {
            if (!perform_implicit_cast(resolvedExpr, *type)) return nullptr;
        }

        auto *resolved = resolvedinitializers.emplace_back(std::move(resolvedExpr)).get();

        resolved->set_constant_value(cee.evaluate(*resolved, false));

        if (only_first) {
            only_first = false;
            type = resolved->type->clone();
        }

        if (!resolved->type->compare(*type)) {
            return report(initializer->location, "unexpected different types in array instantiation expected '" +
                                                     type->to_str() + "' actual '" + resolved->type->to_str() + "'");
        }
    }
    if (type->kind != ResolvedTypeKind::DefaultInit) {
        type = makePtr<ResolvedTypeArray>(SourceLocation{}, std::move(type), arrayInstantiation.initializers.size());
    }
    return makePtr<ResolvedArrayInstantiationExpr>(arrayInstantiation.location, std::move(type),
                                                   std::move(resolvedinitializers));
}

ptr<ResolvedCatchErrorExpr> Sema::resolve_catch_error_expr(const CatchErrorExpr &catchErrorExpr) {
    debug_func(catchErrorExpr.location);
    varOrReturn(errorToCatch, resolve_expr(*catchErrorExpr.errorToCatch));

    if (errorToCatch->type->kind != ResolvedTypeKind::Optional) {
        return report(catchErrorExpr.location, "catch operator must be used with an error union type, got '" +
                                                   errorToCatch->type->to_str() + "'");
    }
    auto optionalType = static_cast<const ResolvedTypeOptional *>(errorToCatch->type.get());
    auto resultType = optionalType->optionalType->clone();

    ptr<ResolvedVarDecl> errorVar = nullptr;
    std::optional<ScopeRAII> captureScope;
    if (!catchErrorExpr.captureIdentifier.empty()) {
        captureScope.emplace(*this);
        auto errorType = makePtr<ResolvedTypeError>(catchErrorExpr.location);
        auto errorTypeExpr = makePtr<ResolvedTypeExpr>(catchErrorExpr.location, std::move(errorType));
        errorVar = makePtr<ResolvedVarDecl>(catchErrorExpr.location, nullptr, true, catchErrorExpr.captureIdentifier,
                                            std::move(errorTypeExpr), false, nullptr);
        if (!insert_decl_to_current_scope(*errorVar)) {
            return report(catchErrorExpr.location, "capture identifier '" + catchErrorExpr.captureIdentifier +
                                                       "' is already defined in the current scope");
        }
        ScopeRAII varDeclScope(*this);
        errorVar->scope = varDeclScope.takeScope();
    }

    auto resolvedCatch = makePtr<ResolvedCatchErrorExpr>(
        catchErrorExpr.location, resultType->clone(), captureScope.has_value() ? captureScope->takeScope() : nullptr);
    m_catchStack.push_back(resolvedCatch.get());
    defer([&]() { m_catchStack.pop_back(); });

    ptr<ResolvedStmt> handler = resolve_stmt(*catchErrorExpr.handler);

    if (!handler) return nullptr;

    if (auto resolvedHandlerExpr = dynamic_cast<const ResolvedExpr *>(handler.get())) {
        if (!resultType->compare(*resolvedHandlerExpr->type)) {
            return report(catchErrorExpr.location, "unexpected mismatch of types in catch expression '" +
                                                       resultType->to_str() + "' and '" +
                                                       resolvedHandlerExpr->type->to_str() + "'");
        }
    }

    resolvedCatch->errorToCatch = std::move(errorToCatch);
    resolvedCatch->errorVar = std::move(errorVar);
    resolvedCatch->handler = std::move(handler);

    return resolvedCatch;
}

ptr<ResolvedTryErrorExpr> Sema::resolve_try_error_expr(const TryErrorExpr &tryErrorExpr) {
    debug_func(tryErrorExpr.location);
    varOrReturn(resolvedErr, resolve_expr(*tryErrorExpr.errorToTry));
    if (resolvedErr->type->kind != ResolvedTypeKind::Optional)
        return report(resolvedErr->location, "expect error union when using try");
    auto optType = static_cast<const ResolvedTypeOptional *>(resolvedErr->type.get());
    auto defers = resolve_defer_ref_stmt(false, true);

    ResolvedFunctionDecl *printErrorDecl = nullptr;
    for (auto &lazy_mod : m_lazy_modules) {
        auto mod = lazy_mod.get();
        auto pathStr = mod->module_path.string();
        debug_msg("Module " << mod->name() << " path " << pathStr);
        if (pathStr.ends_with("std/builtin.dmz")) {
            for (auto &decl : mod->declarations) {
                debug_msg("Target " << "printErrorTrace" << " search " << decl->identifier);
                if (decl->identifier == "printErrorTrace") {
                    if (auto funcDecl = dynamic_cast<ResolvedFunctionDecl *>(decl.get())) {
                        printErrorDecl = funcDecl;
                        break;
                    }
                }
            }
        }
        if (printErrorDecl != nullptr) break;
    }

    return makePtr<ResolvedTryErrorExpr>(tryErrorExpr.location, optType->optionalType->clone(), std::move(resolvedErr),
                                         std::move(defers));
}

ptr<ResolvedOrElseErrorExpr> Sema::resolve_orelse_error_expr(const OrElseErrorExpr &orelseExpr) {
    debug_func(orelseExpr.location);
    varOrReturn(resolvedErr, resolve_expr(*orelseExpr.errorToOrElse));
    if (resolvedErr->type->kind != ResolvedTypeKind::Optional)
        return report(resolvedErr->location, "expect error union when using orelse");
    varOrReturn(resolvedOrelse, resolve_expr(*orelseExpr.orElseExpr));
    auto resolvedErrOptional = static_cast<const ResolvedTypeOptional *>(resolvedErr->type.get());
    if (!resolvedErrOptional->optionalType->compare(*resolvedOrelse->type)) {
        return report(orelseExpr.location, "unexpected mismatch of types in orelse expresion '" +
                                               resolvedErrOptional->optionalType->to_str() + "' and '" +
                                               resolvedOrelse->type->to_str() + "'");
    }
    return makePtr<ResolvedOrElseErrorExpr>(orelseExpr.location, resolvedErrOptional->optionalType->clone(),
                                            std::move(resolvedErr), std::move(resolvedOrelse));
}

ptr<ResolvedImportExpr> Sema::resolve_import_expr(const ImportExpr &importExpr) {
    debug_func(importExpr.location);

    std::string identifier = "";
    std::filesystem::path module_path;
    std::string imported = importExpr.identifier;

    if (imported.ends_with(".dmz")) {
        auto directory = m_driver.m_options.source.parent_path();
        debug_msg("imported " << importExpr.identifier << " " << m_currentModule);
        if (m_currentModule) {
            debug_msg("module " << m_currentModule->identifier << " " << m_currentModule->module_path);
            directory = m_currentModule->module_path.parent_path();
        }
        module_path = directory.append(imported);

        debug_msg("module_path " << module_path);
        if (!std::filesystem::exists(module_path)) {
            report(importExpr.location,
                   std::string("file '") + imported + "' doesn't exist (" + module_path.string() + ")");
            return nullptr;
        }
        module_path = std::filesystem::canonical(module_path);

        std::filesystem::path parent_path = "";
        std::string module_path_str = module_path.string();
        for (auto &&[k, v] : m_driver.m_options.imports) {
            auto imported_dir = std::filesystem::canonical(v.parent_path());
            if (module_path_str.find(imported_dir.string()) != std::string::npos) {
                parent_path = imported_dir;
                identifier = k;
                break;
            }
        }
        if (parent_path.empty()) {
            std::filesystem::path project_path = std::filesystem::canonical(m_driver.m_options.source.parent_path());
            if (module_path_str.find(project_path.string()) != std::string::npos) {
                parent_path = project_path;
            }
        }
        std::string termination = ".dmz";
        if (parent_path.empty()) {
            std::string name = module_path.filename();
            identifier = name.substr(0, name.size() - termination.size());
        } else {
            std::string parent_path_str = parent_path.string();
            auto diff = module_path_str.substr(parent_path_str.size());
            int start_pos = (diff.size() > 0 && diff[0] == '/') ? 1 : 0;
            diff = diff.substr(start_pos, diff.size() - (termination.size() + start_pos));
            std::replace(diff.begin(), diff.end(), '/', '.');
            if (!identifier.empty() && !diff.empty()) {
                identifier += ".";
            }
            identifier += diff;
        }
    } else {
        auto it = m_driver.m_options.imports.find(imported);
        if (it == m_driver.m_options.imports.end()) {
            if (imported == "std" || imported == "builtin" || imported == "types" || imported == "atomic" ||
                imported == "simd") {
                std::string module_name_str(imported);
                module_name_str += ".dmz";
                std::filesystem::path stdPath = m_driver.m_options.source.parent_path() / "std" / module_name_str;
                if (std::filesystem::exists(stdPath)) {
                    module_path = std::filesystem::canonical(stdPath);
                }
                if (module_path.empty()) {
                    stdPath = std::filesystem::absolute("std/" + module_name_str);
                    if (std::filesystem::exists(stdPath)) {
                        module_path = std::filesystem::canonical(stdPath);
                    }
                }
                if (!module_path.empty() && imported == "std") {
                    m_driver.m_options.imports.emplace(imported, module_path);
                }
            }
            if (module_path.empty()) {
                report(importExpr.location, "The required module '" + imported +
                                                "' could not be found.\n Please ensure the module is specified and its "
                                                "path is included using the '-I' flag during compilation.");
                return nullptr;
            }

            if (imported == "builtin" || imported == "types" || imported == "atomic" || imported == "simd") {
                identifier = "std." + imported;
            } else {
                identifier = imported;
            }
        } else {
            module_path = (*it).second;
            identifier = imported;
        }
    }

    ResolvedModuleDecl *im = nullptr;
    auto it_pre = m_pre_resolved_modules.find(module_path.string());
    if (it_pre != m_pre_resolved_modules.end()) {
        im = it_pre->second;
    }

    if (!im) {
        for (auto &lazy_mod : m_lazy_modules) {
            if (lazy_mod->module_path == module_path) {
                im = lazy_mod.get();
                break;
            }
        }
    }

    if (!im) {
        auto resolvedModule = resolve_module_decl(nullptr, identifier, module_path);
        im = resolvedModule.get();
        m_lazy_modules.push_back(std::move(resolvedModule));
    }

    if (!im) {
        return report(importExpr.location, "not resolved module '" + identifier + "'");
    }

    if (!ensure_module_discovered(*im)) return nullptr;
    im->symbolName = identifier;

    return makePtr<ResolvedImportExpr>(importExpr.location, *im);
}

ptr<ResolvedRangeExpr> Sema::resolve_range_expr(const RangeExpr &rangeExpr) {
    debug_func(rangeExpr.location);

    varOrReturn(startExpr, resolve_expr(*rangeExpr.startExpr));
    if (startExpr->type->kind != ResolvedTypeKind::Number) {
        return report(rangeExpr.location, "unexpected type in start of a range '" + startExpr->type->to_str() + "'");
    }

    startExpr->set_constant_value(cee.evaluate(*startExpr, false));

    varOrReturn(endExpr, resolve_expr(*rangeExpr.endExpr));
    if (endExpr->type->kind != ResolvedTypeKind::Number) {
        return report(rangeExpr.location, "unexpected type in end of a range '" + endExpr->type->to_str() + "'");
    }

    endExpr->set_constant_value(cee.evaluate(*endExpr, false));

    return makePtr<ResolvedRangeExpr>(rangeExpr.location, std::move(startExpr), std::move(endExpr));
}
}  // namespace DMZ