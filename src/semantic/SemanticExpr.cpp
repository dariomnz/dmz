#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif

#include <algorithm>

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

    if (decl->state != ResolvedState::FullyResolved && decl->state != ResolvedState::Error) {
        debug_msg("Adding decl ref " << decl->name() << " to pending decls");
        m_pending_decls.emplace(decl);
    }
    if (!decl->type) {
        decl->dump();
        return report(declRefExpr.location, "could not resolve type for '" + declRefExpr.identifier + "'");
    }

    debug_msg("Resolving decl ref " << declRefExpr.identifier << " with type "
                                    << (decl->type ? decl->type->className() : "NULL") << " "
                                    << (decl->type ? decl->type->to_str() : "NULL"));
    auto type = decl->type->clone();
    auto resolvedDeclRefExpr =
        makePtr<ResolvedDeclRefExpr>(declRefExpr.location, declRefExpr.identifier, *decl, std::move(type));

    resolvedDeclRefExpr->set_constant_value(cee.evaluate(*resolvedDeclRefExpr, false));

    return resolvedDeclRefExpr;
}
ptr<ResolvedTypeSpecialized> Sema::infer_generic_types(const SourceLocation &location,
                                                       ResolvedGenericFunctionDecl &funcDecl,
                                                       std::vector<ptr<ResolvedExpr>> &arguments) {
    debug_func(location << " inferring generics for function: " << funcDecl.name());

    std::unordered_map<int, ptr<ResolvedType>> inferredTypes;

    for (size_t i = 0; i < funcDecl.params.size() && i < arguments.size(); ++i) {
        ptr<ResolvedType> ownedArgType = nullptr;
        const ResolvedType *argType = arguments[i]->type.get();

        // Special case: if it's a comptime type parameter, we infer the generic type from the VALUE
        if (funcDecl.params[i]->isComptime && funcDecl.params[i]->type->kind == ResolvedTypeKind::AnyType) {
            auto val = arguments[i]->get_constant_value();
            if (val && val->isType()) {
                ownedArgType = val->getType();
                argType = ownedArgType.get();
            }
        }

        debug_msg(location << "  examining param[" << i << "] (" << funcDecl.params[i]->name() << "): expected "
                           << funcDecl.params[i]->type->to_str() << ", got " << argType->to_str());

        if (!internal_infer_type(inferredTypes, *funcDecl.params[i]->type, *argType, &arguments[i])) {
            return report(arguments[i]->location, "type mismatch during generic inference: expected '" +
                                                      funcDecl.params[i]->type->to_str() + "', actual '" +
                                                      argType->to_str() + "'");
        }
    }

    // Count max slot to build specializedTypes in order
    int maxSlot = -1;
    for (auto &[slot, _] : inferredTypes) {
        if (slot > maxSlot) maxSlot = slot;
    }
    std::vector<ptr<ResolvedType>> specializedTypes;
    if (maxSlot >= 0) {
        specializedTypes.resize(maxSlot + 1);
        for (auto &[slot, type] : inferredTypes) {
            if (slot < 0 || (size_t)slot >= specializedTypes.size()) continue;
            if (!type) continue;
            specializedTypes[slot] = type->clone();
        }
    }

    for (size_t i = 0; i < funcDecl.params.size(); i++) {
        if (funcDecl.params[i]->isComptime) {
            if (funcDecl.params[i]->type->kind == ResolvedTypeKind::AnyType && funcDecl.params[i]->resolvedTypeExpr &&
                funcDecl.params[i]->resolvedTypeExpr->resolvedType->kind == ResolvedTypeKind::Type)
                continue;
            if (i >= arguments.size()) {
                return report(location,
                              "missing argument for comptime parameter '" + funcDecl.params[i]->identifier + "'");
            }
            auto val = arguments[i]->get_constant_value();
            if (!val) {
                if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
                    return report(arguments[i]->location, "argument for comptime parameter '" +
                                                              funcDecl.params[i]->identifier +
                                                              "' must be a compile-time constant");
                }
            }
            specializedTypes.emplace_back(makePtr<ResolvedTypeComptimeValue>(
                arguments[i]->location, makePtr<ComptimeValue>(val.value_or(ComptimeValue()))));
        }
    }

    return makePtr<ResolvedTypeSpecialized>(location, std::move(specializedTypes));
}

bool Sema::internal_infer_type(std::unordered_map<int, ptr<ResolvedType>> &inferredTypes, const ResolvedType &paramType,
                               const ResolvedType &argType, ptr<ResolvedExpr> *argExpr) {
    debug_msg("    internal_infer: " << paramType.to_str() << " <-> " << argType.to_str());

    if (auto anyType = dynamic_cast<const ResolvedTypeAnyType *>(&paramType)) {
        int slot = anyType->genericSlot;
        if (slot >= 0 && inferredTypes.count(slot)) {
            bool matches = inferredTypes[slot]->compare(argType);
            if (!matches) {
                ptr<ResolvedType> normalizedArg;
                if (auto argDecl = dynamic_cast<const ResolvedTypeStructDecl *>(&argType)) {
                    normalizedArg = makePtr<ResolvedTypeStruct>(argType.location, argDecl->decl);
                    matches = inferredTypes[slot]->compare(*normalizedArg);
                }
            }
            if (!matches && argExpr) {
                if (perform_implicit_cast(*argExpr, *inferredTypes[slot])) {
                    return true;
                }
            }
            if (!matches) {
                std::string typeName = anyType->name.empty() ? "anytype" : anyType->name;
                report(anyType->location, "conflict for '" + typeName + "': already '" + inferredTypes[slot]->to_str() +
                                              "', cannot be '" + argType.to_str() + "'");
            }
            return matches;
        }

        std::string typeName = anyType->name.empty() ? "anytype" : anyType->name;
        debug_msg("    assigning slot " << slot << " ('" << typeName << "') = " << argType.to_str());
        if (auto argDecl = dynamic_cast<const ResolvedTypeStructDecl *>(&argType)) {
            inferredTypes[slot] = makePtr<ResolvedTypeStruct>(argType.location, argDecl->decl);
        } else {
            inferredTypes[slot] = argType.clone();
        }
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

    // return paramType.compare(argType);
    return true;
}

ptr<ResolvedCallExpr> Sema::resolve_call_expr(const CallExpr &call) {
    debug_func(call.location);
    bool isMemberCall = false;
    ptr<ResolvedExpr> resolvedBase = nullptr;

    varOrReturn(resolvedCallee, resolve_expr(*call.callee));

    ResolvedMemberExpr *memberExpr = dynamic_cast<ResolvedMemberExpr *>(resolvedCallee.get());
    if (memberExpr) {
        if (auto memFunc = dynamic_cast<const ResolvedFunctionDecl *>(&memberExpr->member)) {
            if (memFunc->parentDecl && !memFunc->isStatic) {
                isMemberCall = true;
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
        if (functionType && functionType->kind == ResolvedTypeKind::AnyType) {
            std::vector<ptr<ResolvedExpr>> resolvedArguments;
            for (auto &&arg : call.arguments) {
                varOrReturn(resolvedArg, resolve_expr(*arg));
                resolvedArguments.emplace_back(std::move(resolvedArg));
            }
            return makePtr<ResolvedCallExpr>(call.location, makePtr<ResolvedTypeAnyType>(call.location),
                                             std::move(resolvedCallee), std::move(resolvedArguments));
        }
        // call.dump();
        return report(call.location, "calling non-function symbol");
    }

    bool errGeneric = fnType->is_generic();
    errGeneric |= fnType->returnType->is_generic();
    for (auto &&param : fnType->paramsTypes) {
        if (errGeneric) break;
        errGeneric |= param->is_generic();
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

    if (errGeneric) {
        ResolvedGenericFunctionDecl *genFunc = nullptr;
        const ResolvedDecl *currentDecl = nullptr;
        if (auto *resolvedDeclRefExpr = dynamic_cast<const ResolvedDeclRefExpr *>(resolvedCallee.get())) {
            currentDecl = &resolvedDeclRefExpr->decl;
        } else if (auto *resolvedMemberExpr = dynamic_cast<const ResolvedMemberExpr *>(resolvedCallee.get())) {
            currentDecl = &resolvedMemberExpr->member;
        }

        while (auto varDecl = dynamic_cast<const ResolvedVarDecl *>(currentDecl)) {
            if (varDecl->isMutable) break;
            if (varDecl->initializer) {
                if (auto idre = dynamic_cast<const ResolvedDeclRefExpr *>(varDecl->initializer.get())) {
                    currentDecl = &idre->decl;
                } else if (auto ime = dynamic_cast<const ResolvedMemberExpr *>(varDecl->initializer.get())) {
                    currentDecl = &ime->member;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        if (currentDecl) {
            genFunc = dynamic_cast<ResolvedGenericFunctionDecl *>(const_cast<ResolvedDecl *>(currentDecl));
        }

        if (genFunc) {
            varOrReturn(specializedTypes, infer_generic_types(call.location, *genFunc, resolvedArguments));
            auto specializedFunc = specialize_generic_function(call.location, *genFunc, *specializedTypes);
            if (!specializedFunc) {
                if (specializedTypes->is_generic()) {
                    if (genFunc->getFnType()->returnType->kind == ResolvedTypeKind::Type) {
                        auto genericType = makePtr<ResolvedTypeAnyType>(call.location);
                        auto callExpr =
                            makePtr<ResolvedCallExpr>(call.location, genFunc->getFnType()->returnType->clone(),
                                                      std::move(resolvedCallee), std::move(resolvedArguments));
                        callExpr->set_constant_value(ComptimeValue(std::move(genericType)));
                        return callExpr;
                    }
                    return makePtr<ResolvedCallExpr>(call.location, genFunc->getFnType()->returnType->clone(),
                                                     std::move(resolvedCallee), std::move(resolvedArguments));
                }
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

    auto resolvedCallExpr = makePtr<ResolvedCallExpr>(call.location, fnType->returnType->clone(),
                                                      std::move(resolvedCallee), std::move(resolvedArguments));

    if (fnType->returnType->kind == ResolvedTypeKind::Type) {
        auto val = cee.evaluate(*resolvedCallExpr, true);
        if (val) {
            resolvedCallExpr->set_constant_value(val);
        } else {
            return report(call.location, "expression must be a compile-time constant because it returns a 'type'");
        }
    }

    return resolvedCallExpr;
}

ptr<ResolvedComptimeExpr> Sema::resolve_comptime_expr(const ComptimeExpr &comptimeExpr) {
    debug_func(comptimeExpr.location);
    varOrReturn(resolvedExpr, resolve_expr(*comptimeExpr.expr));

    auto resolvedComptimeExpr = makePtr<ResolvedComptimeExpr>(comptimeExpr.location, std::move(resolvedExpr));
    resolvedComptimeExpr->set_constant_value(cee.evaluate(*resolvedComptimeExpr, true));

    if (!resolvedComptimeExpr->get_constant_value().has_value()) {
        if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
            return report(comptimeExpr.location, "expression cannot be evaluated at compile time");
        }
    }

    return resolvedComptimeExpr;
}

ptr<ResolvedExpr> Sema::resolve_expr(const Expr &expr, bool isType) {
    debug_func((m_currentModule ? m_currentModule->module_path : "<no module>")
               << " " << expr.location << " " << expr.className());
    if (const auto *number = dynamic_cast<const IntLiteral *>(&expr)) {
        int64_t val = 0;
        if (number->value.size() > 2 && number->value[0] == '0' && number->value[1] == 'x') {
            val = std::stoll(number->value, nullptr, 16);
        } else if (number->value.size() > 2 && number->value[0] == '0' && number->value[1] == 'b') {
            val = std::stoll(number->value, nullptr, 2);
        } else {
            val = std::stoll(number->value, nullptr, 10);
        }
        return makePtr<ResolvedIntLiteral>(number->location, val);
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
    if (const auto *declRefExpr = dynamic_cast<const DeclRefExpr *>(&expr)) {
        return resolve_decl_ref_expr(*declRefExpr);
    }
    if (const auto *callExpr = dynamic_cast<const CallExpr *>(&expr)) {
        return resolve_call_expr(*callExpr);
    }
    if (const auto *comptimeExpr = dynamic_cast<const ComptimeExpr *>(&expr)) {
        return resolve_comptime_expr(*comptimeExpr);
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
    } else if (unary.op == TokenType::op_tilde) {
        if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(resolvedRHS->type.get())) {
            if (numType->numberKind == ResolvedNumberKind::Float) {
                return report(resolvedRHS->location, '\'' + resolvedRHS->type->to_str() +
                                                         "' cannot be used as an operand to unary operator '~'");
            }
        } else {
            return report(resolvedRHS->location,
                          '\'' + resolvedRHS->type->to_str() + "' cannot be used as an operand to unary operator '~'");
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
    if (resolvedExpr->type->kind == ResolvedTypeKind::AnyType) {
        return makePtr<ResolvedDerefPtrExpr>(derefPtrExpr.location, resolvedExpr->type->clone(),
                                             std::move(resolvedExpr));
    }
    auto ptrType = dynamic_cast<const ResolvedTypePointer *>(resolvedExpr->type.get());
    if (!ptrType)
        return report(resolvedExpr->location, "expected pointer type, actual '" + resolvedExpr->type->to_str() + "'");

    return makePtr<ResolvedDerefPtrExpr>(derefPtrExpr.location, ptrType->pointerType->clone(), std::move(resolvedExpr));
}

ptr<ResolvedBinaryOperator> Sema::resolve_binary_operator(const BinaryOperator &binop) {
    debug_func(binop.location);
    varOrReturn(resolvedLHS, resolve_expr(*binop.lhs));
    varOrReturn(resolvedRHS, resolve_expr(*binop.rhs));

    if (resolvedLHS->type->kind == ResolvedTypeKind::Simd && resolvedRHS->type->kind == ResolvedTypeKind::Array) {
        if (!perform_implicit_cast(resolvedRHS, *resolvedLHS->type)) return nullptr;
        if (auto lhsSimd = dynamic_cast<const ResolvedTypeSimd *>(resolvedLHS->type.get())) {
            if (dynamic_cast<const ResolvedTypeSimd *>(resolvedRHS->type.get())) {
                if (lhsSimd->simdSize == 0) {
                    resolvedLHS->type = resolvedRHS->type->clone();
                }
            }
        }
    } else if (resolvedLHS->type->kind == ResolvedTypeKind::Array &&
               resolvedRHS->type->kind == ResolvedTypeKind::Simd) {
        if (!perform_implicit_cast(resolvedLHS, *resolvedRHS->type)) return nullptr;
        if (auto rhsSimd = dynamic_cast<const ResolvedTypeSimd *>(resolvedRHS->type.get())) {
            if (dynamic_cast<const ResolvedTypeSimd *>(resolvedLHS->type.get())) {
                if (rhsSimd->simdSize == 0) {
                    resolvedRHS->type = resolvedLHS->type->clone();
                }
            }
        }
    } else {
        if (!perform_implicit_cast(resolvedLHS, *resolvedRHS->type)) return nullptr;
        if (!perform_implicit_cast(resolvedRHS, *resolvedLHS->type)) return nullptr;
    }

    auto lhsKind = resolvedLHS->type->kind;
    auto rhsKind = resolvedRHS->type->kind;
    if (lhsKind != ResolvedTypeKind::Number && lhsKind != ResolvedTypeKind::Bool &&
        lhsKind != ResolvedTypeKind::Error && lhsKind != ResolvedTypeKind::Pointer &&
        lhsKind != ResolvedTypeKind::Simd && lhsKind != ResolvedTypeKind::AnyType &&
        lhsKind != ResolvedTypeKind::AnyType) {
        return report(resolvedLHS->location,
                      '\'' + resolvedLHS->type->to_str() + "' cannot be used as LHS operand to binary operator");
    }
    if (rhsKind != ResolvedTypeKind::Number && rhsKind != ResolvedTypeKind::Bool &&
        rhsKind != ResolvedTypeKind::Error && rhsKind != ResolvedTypeKind::Pointer &&
        rhsKind != ResolvedTypeKind::Simd && rhsKind != ResolvedTypeKind::AnyType &&
        rhsKind != ResolvedTypeKind::AnyType) {
        return report(resolvedRHS->location,
                      '\'' + resolvedRHS->type->to_str() + "' cannot be used as RHS operand to binary operator");
    }

    if (binop.op == TokenType::amp || binop.op == TokenType::pipe || binop.op == TokenType::caret ||
        binop.op == TokenType::op_shl || binop.op == TokenType::op_shr) {
        if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(resolvedLHS->type.get())) {
            if (numType->numberKind == ResolvedNumberKind::Float) {
                return report(binop.location,
                              "bitwise operator '" + get_op_str(binop.op) + "' cannot be used on float type");
            }
        }
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
                                  vec<ptr<ResolvedFunctionDecl>>{}, sliceScope.takeScope());
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

    ptr<ResolvedType> representedType = nullptr;
    if (baseType->kind == ResolvedTypeKind::Type) {
        auto cv = resolvedBase->get_constant_value();
        if (!cv) {
            return report(memberExpr.location, "expected type constant expression");
        }
        if (cv && cv->isType()) {
            representedType = cv->getType();
            baseType = representedType.get();
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

        if (auto memberFunc = dynamic_cast<const ResolvedFunctionDecl *>(decl)) {
            if (memberFunc->parentDecl && !isDecl && memberFunc->isStatic) {
                return report(memberExpr.location, "cannot use static member with an instance of struct '" +
                                                       resolvedBase->type->to_str() + "'");
            }
        } else if (dynamic_cast<const ResolvedFieldDecl *>(decl)) {
            if (isDecl && !isUnion && !isEnum) {
                return report(memberExpr.location,
                              "cannot access non-static field '" + memberExpr.field + "' without an instance");
            }
        }
    } else if (auto sliceType = dynamic_cast<const ResolvedTypeSlice *>(baseType)) {
        if (memberExpr.field == "ptr") {
            sliceDecl.fields[0]->type = makePtr<ResolvedTypePointer>(SourceLocation{}, sliceType->sliceType->clone());
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
            debug_msg("Err " << err->identifier << " field " << memberExpr.field);
            if (err->identifier == memberExpr.field) {
                decl = err.get();
                break;
            }
        }
        if (!decl) return report(memberExpr.location, "error group has no member called '" + memberExpr.field + '\'');
    } else if (baseType->kind == ResolvedTypeKind::AnyType) {
        // Return a dummy member expression for generic types to allow LSP highlighting
        static std::unordered_map<std::string, ptr<ResolvedFieldDecl>> genericFields;
        if (genericFields.find(memberExpr.field) == genericFields.end()) {
            genericFields[memberExpr.field] = makePtr<ResolvedFieldDecl>(
                SourceLocation{}, memberExpr.field,
                makePtr<ResolvedTypeExpr>(SourceLocation{}, makePtr<ResolvedTypeAnyType>(SourceLocation{})), 0,
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

    if (resolvedBase->type->kind == ResolvedTypeKind::AnyType) {
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

    ptr<ResolvedType> instType = nullptr;
    if (resolvedBase->type->kind == ResolvedTypeKind::Type) {
        if (auto typeExpr = dynamic_cast<const ResolvedTypeExpr *>(resolvedBase.get())) {
            instType = typeExpr->resolvedType->clone();
        } else {
            auto cv = resolvedBase->get_constant_value();
            if (!cv) {
                return report(structInstantiation.location, "expected type constant expression");
            }
            if (cv && cv->isType()) {
                instType = cv->getType();
            }
        }
    } else {
        instType = resolvedBase->type->clone();
    }

    if (!instType) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }

    if (instType->kind == ResolvedTypeKind::UnionDecl || instType->kind == ResolvedTypeKind::Union) {
        ResolvedUnionDecl *un = nullptr;
        if (auto unionDecl = dynamic_cast<ResolvedTypeUnionDecl *>(instType.get())) {
            un = unionDecl->unionDecl();
        } else if (auto unionType = dynamic_cast<ResolvedTypeUnion *>(instType.get())) {
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

    if (instType->kind == ResolvedTypeKind::AnyType) {
        ResolvedStructDecl *genericStructDecl = nullptr;
        if (auto *callExpr = dynamic_cast<ResolvedCallExpr *>(resolvedBase.get())) {
            if (auto *declRef = dynamic_cast<ResolvedDeclRefExpr *>(callExpr->callee.get())) {
                if (auto *genFunc =
                        dynamic_cast<ResolvedGenericFunctionDecl *>(const_cast<ResolvedDecl *>(&declRef->decl))) {
                    if (ensure_fully_resolved(*genFunc) && genFunc->body) {
                        for (auto &stmt : genFunc->body->statements) {
                            if (auto *retStmt = dynamic_cast<ResolvedReturnStmt *>(stmt.get())) {
                                if (auto *typeExpr = dynamic_cast<ResolvedTypeExpr *>(retStmt->expr.get())) {
                                    if (auto *structType =
                                            dynamic_cast<ResolvedTypeStructDecl *>(typeExpr->resolvedType.get())) {
                                        genericStructDecl = structType->decl;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (genericStructDecl) {
            if (!ensure_struct_members_resolved(*genericStructDecl)) return nullptr;
            std::vector<ptr<ResolvedFieldInitStmt>> resolvedFieldInits;
            std::unordered_map<std::string, const ResolvedFieldDecl *> fields;
            for (auto &&fieldDecl : genericStructDecl->fields) fields[fieldDecl->identifier] = fieldDecl.get();
            bool error = false;
            for (auto &&initStmt : structInstantiation.fieldInitializers) {
                auto resolvedInitExpr = resolve_expr(*initStmt->initializer);
                if (!resolvedInitExpr) {
                    error = true;
                    continue;
                }
                const ResolvedFieldDecl *fieldDecl = fields[initStmt->identifier];
                if (!fieldDecl) {
                    error = true;
                    continue;
                }
                resolvedInitExpr->set_constant_value(cee.evaluate(*resolvedInitExpr, false));
                auto init = makePtr<ResolvedFieldInitStmt>(initStmt->location, *fieldDecl, std::move(resolvedInitExpr));
                resolvedFieldInits.emplace_back(std::move(init));
            }
            if (error) return nullptr;
            return makePtr<ResolvedStructInstantiationExpr>(structInstantiation.location, std::move(resolvedBase),
                                                            *genericStructDecl, std::move(resolvedFieldInits), false);
        }
        return makePtr<ResolvedTypeExpr>(structInstantiation.location, std::move(instType));
    }

    if (instType->kind != ResolvedTypeKind::StructDecl) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }
    auto auxstruType = dynamic_cast<const ResolvedTypeStructDecl *>(instType.get());
    if (!auxstruType) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }
    auto &st = auxstruType->decl;
    if (!st) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }

    // Lazy: ensure struct members are resolved before accessing fields
    if (!ensure_struct_members_resolved(*st)) return nullptr;

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
        auto structDecl = makePtr<ResolvedStructDecl>(tupleInstantiation.location, false, tupleName, nullptr, false,
                                                      std::move(tupleFields), vec<ptr<ResolvedFunctionDecl>>{},
                                                      tupleScope.takeScope());
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
        type = makePtr<ResolvedTypeArray>(SourceLocation{}, std::move(type), nullptr,
                                          arrayInstantiation.initializers.size());
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
    if (resolvedErr->type->kind == ResolvedTypeKind::AnyType) {
        return makePtr<ResolvedTryErrorExpr>(tryErrorExpr.location, resolvedErr->type->clone(), std::move(resolvedErr),
                                             std::vector<ptr<ResolvedDeferRefStmt>>{});
    }
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
    if (!perform_implicit_cast(resolvedOrelse, *resolvedErrOptional)) return nullptr;
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
                imported == "simd" || imported == "start" || imported == "math") {
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

            if (imported == "builtin" || imported == "types" || imported == "atomic" || imported == "simd" ||
                imported == "start" || imported == "math") {
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
    for (auto &lazy_mod : m_lazy_modules) {
        if (lazy_mod->module_path == module_path) {
            im = lazy_mod.get();
            break;
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

    if (m_currentModule && m_currentModule != im) {
        if (std::find(m_currentModule->dependsOn.begin(), m_currentModule->dependsOn.end(), im) ==
            m_currentModule->dependsOn.end()) {
            m_currentModule->dependsOn.emplace_back(im);
            im->isUsedBy.emplace_back(m_currentModule);
        }
    }

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
        if (endExpr->type->kind != ResolvedTypeKind::AnyType) {
            return report(rangeExpr.location, "unexpected type in end of a range '" + endExpr->type->to_str() + "'");
        }
        return makePtr<ResolvedRangeExpr>(rangeExpr.location, std::move(startExpr), std::move(endExpr));
    }

    endExpr->set_constant_value(cee.evaluate(*endExpr, false));

    if (!perform_implicit_cast(startExpr, *endExpr->type)) return nullptr;
    if (!perform_implicit_cast(endExpr, *startExpr->type)) return nullptr;

    return makePtr<ResolvedRangeExpr>(rangeExpr.location, std::move(startExpr), std::move(endExpr));
}
}  // namespace DMZ