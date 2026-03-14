#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif
#include "DMZPCH.hpp"
#include "Utils.hpp"
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
        dmz_unreachable(resolvedBase->location.to_string() + " unexpected base expresion in generic expresion");
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
        dmz_unreachable("unexpected there are no decl to specialize");
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

    if (!decl) {
        resolvedBase->dump();
        genericExpr.dump();
        specializedType->dump();
        // dmz_unreachable("FIX");
        return report(genericExpr.location,
                      "cannot specialize '" + resolvedBase->type->to_str() + "' with " + genericExpr.to_str());
    } else {
        return makePtr<ResolvedGenericExpr>(genericExpr.location, std::move(resolvedBase), *decl,
                                            std::move(specializedType));
    }
}

ptr<ResolvedDeclRefExpr> Sema::resolve_decl_ref_expr(const DeclRefExpr &declRefExpr) {
    debug_func(declRefExpr.location);
    // Search in the module scope
    ResolvedDecl *decl = lookup(declRefExpr.location, declRefExpr.identifier);
    if (!decl) {
#ifdef DEBUG
        dump_scopes();
#endif
        return report(declRefExpr.location, "symbol '" + declRefExpr.identifier + "' not found");
    }

    auto type = decl->type->clone();
    if (declRefExpr.identifier == "@This") {
        if (auto *st = dynamic_cast<ResolvedTypeStruct *>(type.get())) {
            st->is_this = true;
        } else if (auto *std = dynamic_cast<ResolvedTypeStructDecl *>(type.get())) {
            std->is_this = true;
        }
    }
    auto resolvedDeclRefExpr = makePtr<ResolvedDeclRefExpr>(declRefExpr.location, *decl, std::move(type));

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
        if (resolvedBase->type->kind == ResolvedTypeKind::Struct) {
            argsToAdd = makePtr<ResolvedRefPtrExpr>(resolvedBase->location, std::move(resolvedBase));
        } else if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(resolvedBase->type.get())) {
            if (ptrType->pointerType->kind == ResolvedTypeKind::Struct) {
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
                resolvedCallee = makePtr<ResolvedDeclRefExpr>(resolvedDeclRefExpr->location, *specializedFunc,
                                                              specializedFunc->type->clone());
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
            dmz_unreachable("unexpected member call without member pass as argument");
        }
    }

    for (size_t i = 0; i < call_args_num; ++i) {
        size_t paramIdx = isMemberCall ? i + 1 : i;
        if (paramIdx < funcDeclArgs) {
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

ptr<ResolvedExpr> Sema::resolve_expr(const Expr &expr) {
    debug_func(expr.location);
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
        return resolve_unary_operator(*unaryOperator);
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
        return resolve_assignable_expr(*assignableExpr);
    }
    if (const auto *typeExpr = dynamic_cast<const Type *>(&expr)) {
        if (auto typePtr = dynamic_cast<const TypePointer *>(typeExpr)) {
            varOrReturn(resType, resolve_type(*typePtr));
            varOrReturn(child, resolve_expr(*typePtr->pointerType));
            return makePtr<ResolvedTypePointerExpr>(typePtr->location, std::move(resType), std::move(child));
        }
        if (auto typeSlice = dynamic_cast<const TypeSlice *>(typeExpr)) {
            varOrReturn(resType, resolve_type(*typeSlice));
            varOrReturn(child, resolve_expr(*typeSlice->sliceType));
            return makePtr<ResolvedTypeSliceExpr>(typeSlice->location, std::move(resType), std::move(child));
        }
        varOrReturn(resolvedType, resolve_type(*typeExpr));
        return makePtr<ResolvedTypeExpr>(typeExpr->location, std::move(resolvedType));
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
    if (const auto *sizeofExpr = dynamic_cast<const SizeofExpr *>(&expr)) {
        return resolve_sizeof_expr(*sizeofExpr);
    }
    if (const auto *typeidExpr = dynamic_cast<const TypeidExpr *>(&expr)) {
        return resolve_typeid_expr(*typeidExpr);
    }
    if (const auto *typeinfoExpr = dynamic_cast<const TypeinfoExpr *>(&expr)) {
        return resolve_typeinfo_expr(*typeinfoExpr);
    }
    if (const auto *rangeExpr = dynamic_cast<const RangeExpr *>(&expr)) {
        return resolve_range_expr(*rangeExpr);
    }
    expr.dump();
    dmz_unreachable("unexpected expression");
}

ptr<ResolvedUnaryOperator> Sema::resolve_unary_operator(const UnaryOperator &unary) {
    debug_func(unary.location);
    varOrReturn(resolvedRHS, resolve_expr(*unary.operand));

    auto boolType = ResolvedTypeBool{SourceLocation{}};

    if (resolvedRHS->type->kind == ResolvedTypeKind::Void ||
        (unary.op == TokenType::op_excla_mark && !boolType.compare(*resolvedRHS->type)))
        return report(resolvedRHS->location, '\'' + resolvedRHS->type->to_str() +
                                                 "' cannot be used as an operand to unary operator '" +
                                                 get_op_str(unary.op) + "'");
    ptr<DMZ::ResolvedType> resolvedType = nullptr;
    if (op_generate_bool(unary.op)) {
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

    if (resolvedLHS->type->kind != ResolvedTypeKind::Number && resolvedLHS->type->kind != ResolvedTypeKind::Bool &&
        resolvedLHS->type->kind != ResolvedTypeKind::Pointer && resolvedLHS->type->kind != ResolvedTypeKind::Generic) {
        return report(resolvedLHS->location,
                      '\'' + resolvedLHS->type->to_str() + "' cannot be used as LHS operand to binary operator");
    }
    if (resolvedRHS->type->kind != ResolvedTypeKind::Number && resolvedRHS->type->kind != ResolvedTypeKind::Bool &&
        resolvedRHS->type->kind != ResolvedTypeKind::Pointer && resolvedRHS->type->kind != ResolvedTypeKind::Generic) {
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
        ret->type = makePtr<ResolvedTypeBool>(binop.location);
    }
    return ret;
}

ptr<ResolvedGroupingExpr> Sema::resolve_grouping_expr(const GroupingExpr &grouping) {
    debug_func(grouping.location);
    varOrReturn(resolvedExpr, resolve_expr(*grouping.expr));
    return makePtr<ResolvedGroupingExpr>(grouping.location, std::move(resolvedExpr));
}

ptr<ResolvedAssignableExpr> Sema::resolve_assignable_expr(const AssignableExpr &assignableExpr) {
    debug_func(assignableExpr.location);
    if (const auto *declRefExpr = dynamic_cast<const DeclRefExpr *>(&assignableExpr))
        return resolve_decl_ref_expr(*declRefExpr);

    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(&assignableExpr))
        return resolve_member_expr(*memberExpr);

    if (const auto *arrayAtExpr = dynamic_cast<const ArrayAtExpr *>(&assignableExpr))
        return resolve_array_at_expr(*arrayAtExpr);

    if (const auto *derefExpr = dynamic_cast<const DerefPtrExpr *>(&assignableExpr))
        return resolve_deref_ptr_expr(*derefExpr);

    assignableExpr.dump();
    dmz_unreachable("unexpected assignable expression");
}

ptr<ResolvedMemberExpr> Sema::resolve_member_expr(const MemberExpr &memberExpr) {
    debug_func(memberExpr.location);
    static ResolvedStructDecl sliceDecl = [] {
        std::vector<ptr<ResolvedFieldDecl>> sliceFields;
        sliceFields.emplace_back(makePtr<ResolvedFieldDecl>(
            SourceLocation{}, "ptr",
            makePtr<ResolvedTypePointer>(SourceLocation{}, makePtr<ResolvedTypeVoid>(SourceLocation{})), 0, nullptr));
        sliceFields.emplace_back(makePtr<ResolvedFieldDecl>(
            SourceLocation{}, "len",
            makePtr<ResolvedTypeNumber>(SourceLocation{}, ResolvedNumberKind::UInt, Driver::instance().ptrBitSize()), 1,
            nullptr));
        return ResolvedStructDecl(SourceLocation{}, true, "slice", nullptr, false, std::move(sliceFields),
                                  std::vector<ptr<ResolvedMemberFunctionDecl>>{});
    }();
    const ResolvedDecl *decl = nullptr;
    auto resolvedBase = resolve_expr(*memberExpr.base);
    if (!resolvedBase) return nullptr;

    if (memberExpr.field.empty()) {
        static ResolvedFieldDecl dummyCompletionField(SourceLocation{}, "", makePtr<ResolvedTypeVoid>(SourceLocation{}),
                                                      0, nullptr);
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

    if (auto struType = dynamic_cast<const ResolvedTypeStructDecl *>(baseType)) {
        decl = lookup_in_struct(memberExpr.location, *struType->decl, memberExpr.field);
        if (!decl) {
            return report(memberExpr.location, "struct \'" + resolvedBase->type->to_str() + "' has no member called '" +
                                                   memberExpr.field + '\'');
        }
        // auto memberFunc = dynamic_cast<const ResolvedMemberFunctionDecl *>(decl);
        // auto fieldDecl = dynamic_cast<const ResolvedFieldDecl *>(decl);
        // if ((memberFunc && !memberFunc->isStatic) || fieldDecl) {
        //     return report(memberExpr.location, "expected an instance of '" + struType->to_str() + "'");
        // }
    } else if (dynamic_cast<const ResolvedTypeSlice *>(baseType)) {
        if (memberExpr.field == "ptr") {
            decl = sliceDecl.fields[0].get();
        } else if (memberExpr.field == "len") {
            decl = sliceDecl.fields[1].get();
        } else {
            return report(memberExpr.location, "slice only support 'len' and 'ptr' members");
        }
    } else if (auto struType = dynamic_cast<const ResolvedTypeStruct *>(baseType)) {
        decl = lookup_in_struct(memberExpr.location, *struType->decl, memberExpr.field);
        if (!decl) {
            return report(memberExpr.location, "struct \'" + resolvedBase->type->to_str() + "' has no member called '" +
                                                   memberExpr.field + '\'');
        }
        if (auto memberFunc = dynamic_cast<const ResolvedMemberFunctionDecl *>(decl)) {
            if (memberFunc->isStatic) {
                return report(memberExpr.location, "cannot call a static member with a instance of struct \'" +
                                                       resolvedBase->type->to_str() + "'");
            }
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
        static std::map<std::string, ptr<ResolvedFieldDecl>> genericFields;
        if (genericFields.find(memberExpr.field) == genericFields.end()) {
            genericFields[memberExpr.field] = makePtr<ResolvedFieldDecl>(
                SourceLocation{}, memberExpr.field, makePtr<ResolvedTypeGeneric>(SourceLocation{}, nullptr), 0, nullptr);
        }
        return makePtr<ResolvedMemberExpr>(memberExpr.location, std::move(resolvedBase), *genericFields[memberExpr.field]);
    } else {
        return report(memberExpr.base->location, "cannot access member of '" + resolvedBase->type->to_str() + '\'');
    }
    // Implicit deref of the pointer in members
    if (baseIsPointer) {
        resolvedBase = makePtr<ResolvedDerefPtrExpr>(memberExpr.location, baseType->clone(), std::move(resolvedBase));
    }
    auto res = makePtr<ResolvedMemberExpr>(memberExpr.location, std::move(resolvedBase), *decl);
    res->set_constant_value(cee.evaluate(*res, false));
    return res;
}

ptr<ResolvedAssignableExpr> Sema::resolve_array_at_expr(const ArrayAtExpr &arrayAtExpr) {
    debug_func(arrayAtExpr.location);
    auto resolvedBase = resolve_expr(*arrayAtExpr.array);
    if (!resolvedBase) return nullptr;

    if (resolvedBase->type->kind != ResolvedTypeKind::Array && resolvedBase->type->kind != ResolvedTypeKind::Pointer &&
        resolvedBase->type->kind != ResolvedTypeKind::Slice) {
        bool isTypeBase = dynamic_cast<ResolvedTypeExpr *>(resolvedBase.get()) != nullptr ||
                          dynamic_cast<ResolvedTypePointerExpr *>(resolvedBase.get()) ||
                          dynamic_cast<ResolvedTypeSliceExpr *>(resolvedBase.get()) ||
                          dynamic_cast<ResolvedTypeOptionalExpr *>(resolvedBase.get()) ||
                          dynamic_cast<ResolvedTypeArrayExpr *>(resolvedBase.get()) ||
                          dynamic_cast<ResolvedGenericExpr *>(resolvedBase.get());
        if (!isTypeBase) {
            auto kind = resolvedBase->type->kind;
            if (kind == ResolvedTypeKind::StructDecl || kind == ResolvedTypeKind::Module ||
                kind == ResolvedTypeKind::ErrorGroup || kind == ResolvedTypeKind::Generic ||
                kind == ResolvedTypeKind::Void || kind == ResolvedTypeKind::Number || kind == ResolvedTypeKind::Bool) {
                isTypeBase = true;
            }
        }

        if (isTypeBase) {
            varOrReturn(arrayType, resolve_type(arrayAtExpr));
            varOrReturn(index, resolve_expr(*arrayAtExpr.index));
            return makePtr<ResolvedTypeArrayExpr>(arrayAtExpr.location, std::move(arrayType), std::move(resolvedBase),
                                                  std::move(index));
        }

        return report(arrayAtExpr.array->location, "cannot access element of '" + resolvedBase->type->to_str() + '\'');
    }
    ptr<ResolvedType> derefType = nullptr;
    if (auto arrType = dynamic_cast<const ResolvedTypeArray *>(resolvedBase->type.get())) {
        derefType = arrType->arrayType->clone();
    } else if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(resolvedBase->type.get())) {
        derefType = ptrType->pointerType->clone();
    } else if (auto sliceType = dynamic_cast<const ResolvedTypeSlice *>(resolvedBase->type.get())) {
        derefType = sliceType->sliceType->clone();
    } else {
        dmz_unreachable("TODO");
    }

    varOrReturn(index, resolve_expr(*arrayAtExpr.index));
    if (dynamic_cast<ResolvedRangeExpr *>(index.get())) {
        derefType = makePtr<ResolvedTypeSlice>(index->location, std::move(derefType));
    }

    return makePtr<ResolvedArrayAtExpr>(arrayAtExpr.location, std::move(derefType), std::move(resolvedBase),
                                        std::move(index));
}

ptr<ResolvedStructInstantiationExpr> Sema::resolve_struct_instantiation(
    const StructInstantiationExpr &structInstantiation) {
    debug_func(structInstantiation.location);

    varOrReturn(resolvedBase, resolve_expr(*structInstantiation.base));

    if (resolvedBase->type->kind != ResolvedTypeKind::StructDecl) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }
    auto auxstruType = static_cast<const ResolvedTypeStructDecl *>(resolvedBase->type.get());
    auto st = auxstruType->decl;
    if (!st) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }

    bool is_this = false;
    if (auto declRefExpr = dynamic_cast<const DeclRefExpr *>(structInstantiation.base.get())) {
        if (declRefExpr->identifier == "@This") {
            is_this = true;
        }
    }
    if (is_this == false && !dynamic_cast<const GenericExpr *>(structInstantiation.base.get()) &&
        dynamic_cast<ResolvedGenericStructDecl *>(st)) {
        return report(structInstantiation.location, "'" + st->identifier + "' is a generic and need specialization");
    }

    std::vector<ptr<ResolvedFieldInitStmt>> resolvedFieldInits;
    std::map<std::string, const ResolvedFieldInitStmt *> inits;

    std::map<std::string, const ResolvedFieldDecl *> fields;
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

    // Add the default initilizers if there was not initialized
    for (auto &&decl : st->structDecl->decls) {
        if (inits.count(decl->identifier)) continue;

        auto fieldDecl = dynamic_cast<const FieldDecl *>(decl.get());
        if (!fieldDecl) continue;
        if (fieldDecl->default_initializer) {
            const std::string &id = fieldDecl->identifier;
            auto resolvedInitExpr = resolve_expr(*fieldDecl->default_initializer);
            if (!resolvedInitExpr) {
                error = true;
                continue;
            }

            const ResolvedFieldDecl *resolvedfieldDecl = fields[id];
            auto init = makePtr<ResolvedFieldInitStmt>(fieldDecl->default_initializer->location, *resolvedfieldDecl,
                                                       std::move(resolvedInitExpr));
            inits[id] = resolvedFieldInits.emplace_back(std::move(init)).get();
        }
    }

    for (auto &&fieldDecl : st->fields) {
        if (!inits.count(fieldDecl->identifier)) {
            report(structInstantiation.location, "field '" + fieldDecl->identifier + "' is not initialized");
            error = true;
            continue;
        }

        auto &initStmt = inits[fieldDecl->identifier];
        initStmt->initializer->set_constant_value(cee.evaluate(*initStmt->initializer, false));
    }

    if (error) return nullptr;

    auto res = makePtr<ResolvedStructInstantiationExpr>(structInstantiation.location, *st,
                                                        std::move(resolvedFieldInits), false);
    if (auxstruType->is_this) {
        if (auto *resStruType = dynamic_cast<ResolvedTypeStruct *>(res->type.get())) {
            resStruType->is_this = true;
        }
    }
    return res;
}

ptr<ResolvedStructInstantiationExpr> Sema::resolve_tuple_instantiation(
    const TupleInstantiationExpr &tupleInstantiation) {
    debug_func(tupleInstantiation.location);

    std::vector<ptr<ResolvedFieldDecl>> tupleFields;
    std::vector<ptr<ResolvedFieldInitStmt>> resolvedFieldInits;

    unsigned index = 0;
    for (auto &&element : tupleInstantiation.elements) {
        varOrReturn(resolvedElement, resolve_expr(*element));
        resolvedElement->set_constant_value(cee.evaluate(*resolvedElement, false));

        std::string fieldName = "elem" + std::to_string(index);

        auto fieldDecl = makePtr<ResolvedFieldDecl>(resolvedElement->location, fieldName,
                                                    resolvedElement->type->clone(), index, nullptr);
        auto initStmt =
            makePtr<ResolvedFieldInitStmt>(resolvedElement->location, *fieldDecl, std::move(resolvedElement));
        tupleFields.emplace_back(std::move(fieldDecl));
        resolvedFieldInits.emplace_back(std::move(initStmt));
        index++;
    }

    std::string tupleName = "tuple." + std::to_string(m_currentModule->tuple_counter++);
    auto structDecl =
        makePtr<ResolvedStructDecl>(tupleInstantiation.location, false, tupleName, nullptr, false,
                                    std::move(tupleFields), std::vector<ptr<ResolvedMemberFunctionDecl>>{});
    structDecl->isTuple = true;
    auto *structDeclPtr = structDecl.get();
    m_currentModule->declarations.emplace_back(std::move(structDecl));
    add_dependency(structDeclPtr);

    return makePtr<ResolvedStructInstantiationExpr>(tupleInstantiation.location, *structDeclPtr,
                                                    std::move(resolvedFieldInits), true);
}

ptr<ResolvedArrayInstantiationExpr> Sema::resolve_array_instantiation(
    const ArrayInstantiationExpr &arrayInstantiation) {
    debug_func(arrayInstantiation.location);
    std::vector<ptr<ResolvedExpr>> resolvedinitializers;
    resolvedinitializers.reserve(arrayInstantiation.initializers.size());

    ptr<ResolvedType> type = makePtr<ResolvedTypeDefaultInit>(SourceLocation{});
    bool only_first = true;
    for (auto &&initializer : arrayInstantiation.initializers) {
        varOrReturn(resolvedExpr, resolve_expr(*initializer));
        auto *resolved = resolvedinitializers.emplace_back(std::move(resolvedExpr)).get();

        resolved->set_constant_value(cee.evaluate(*resolved, false));

        if (only_first) {
            only_first = false;
            type = resolved->type->clone();
        }

        if (!resolved->type->compare(*type)) {
            return report(initializer->location, "unexpected different types in array instantiation expected '" +
                                                     resolved->type->to_str() + "' actual '" + type->to_str() + "'");
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
    auto resolvedErr = resolve_expr(*catchErrorExpr.errorToCatch);
    return makePtr<ResolvedCatchErrorExpr>(catchErrorExpr.location, std::move(resolvedErr));
}

ptr<ResolvedTryErrorExpr> Sema::resolve_try_error_expr(const TryErrorExpr &tryErrorExpr) {
    debug_func(tryErrorExpr.location);
    varOrReturn(resolvedErr, resolve_expr(*tryErrorExpr.errorToTry));
    if (resolvedErr->type->kind != ResolvedTypeKind::Optional)
        return report(resolvedErr->location, "expect error union when using try");
    auto optType = static_cast<const ResolvedTypeOptional *>(resolvedErr->type.get());
    auto defers = resolve_defer_ref_stmt(false, true);
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

    if (importExpr.module_path.empty()) {
        if (importExpr.identifier.ends_with(".dmz")) {
            return report(importExpr.location, "file '" + importExpr.identifier + "' doesn't exists");
        } else {
            return report(importExpr.location, "The required module '" + importExpr.identifier +
                                                   "' could not be found.\n Please ensure the module is specified and "
                                                   "its path is included using the '-I' "
                                                   "flag during compilation, e.g., `-I <module> <path>`.");
        }
    }

    auto it = m_modules_for_import.find(importExpr.module_path);
    if (it == m_modules_for_import.end()) {
        return report(importExpr.location, "not resolved module '" + importExpr.identifier + "'");
    }

    auto im = (*it).second;
    im->symbolName = importExpr.module_id;

    return makePtr<ResolvedImportExpr>(importExpr.location, *im);
}

ptr<ResolvedSizeofExpr> Sema::resolve_sizeof_expr(const SizeofExpr &sizeofExpr) {
    debug_func(sizeofExpr.location);
    auto type = resolve_type(*sizeofExpr.sizeofType);
    if (!type)
        return report(sizeofExpr.sizeofType->location, "cannot resolve type '" + sizeofExpr.sizeofType->to_str() + "'");

    return makePtr<ResolvedSizeofExpr>(sizeofExpr.location, std::move(type));
}

ptr<ResolvedTypeidExpr> Sema::resolve_typeid_expr(const TypeidExpr &typeidExpr) {
    debug_func(typeidExpr.location);
    varOrReturn(expr, resolve_expr(*typeidExpr.typeidExpr));
    auto resolved = makePtr<ResolvedTypeidExpr>(typeidExpr.location, std::move(expr));
    resolved->set_constant_value(cee.evaluate(*resolved, false));
    return resolved;
}

ptr<ResolvedTypeinfoExpr> Sema::resolve_typeinfo_expr(const TypeinfoExpr &typeinfoExpr) {
    debug_func(typeinfoExpr.location);
    varOrReturn(expr, resolve_expr(*typeinfoExpr.typeinfoExpr));

    std::string targetStructName;
    if (expr->type->kind == ResolvedTypeKind::StructDecl || expr->type->kind == ResolvedTypeKind::Struct ||
        expr->type->kind == ResolvedTypeKind::Generic) {
        targetStructName = "TypeInfoStruct";
    } else if (expr->type->kind == ResolvedTypeKind::Number) {
        targetStructName = "TypeInfoNumber";
    } else if (expr->type->kind == ResolvedTypeKind::Bool || expr->type->kind == ResolvedTypeKind::Void) {
        targetStructName = "TypeInfo";
    } else {
        return report(typeinfoExpr.location, "unsupported type for @typeinfo: " + expr->type->to_str());
    }

    ResolvedTypeStructDecl *typeInfoDecl = nullptr;
    for (auto &[path, mod] : m_modules_for_import) {
        if (mod->name() == "std.builtin") {
            for (auto &decl : mod->declarations) {
                if (decl->identifier == targetStructName) {
                    if (auto structDeclRef = dynamic_cast<ResolvedTypeStructDecl *>(decl->type.get())) {
                        typeInfoDecl = structDeclRef;
                        break;
                    }
                }
            }
        }
    }

    if (!typeInfoDecl) {
        return report(typeinfoExpr.location, "could not find " + targetStructName + " in builtin.dmz");
    }
    add_dependency(typeInfoDecl->decl);
    auto returnType = makePtr<ResolvedTypePointer>(typeinfoExpr.location, typeInfoDecl->clone());
    return makePtr<ResolvedTypeinfoExpr>(typeinfoExpr.location, std::move(returnType), std::move(expr));
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