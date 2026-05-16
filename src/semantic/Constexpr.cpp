#include "semantic/Constexpr.hpp"

#include "Debug.hpp"
#include "codegen/CodegenUtils.hpp"
#include "semantic/Semantic.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

std::optional<bool> ConstantExpressionEvaluator::to_bool(const std::optional<ComptimeValue> &d) {
    if (!d) return std::nullopt;
    if (d->isBool()) return d->getBool();
    if (d->isInt()) return d->getInt() != 0;
    return std::nullopt;
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate(const ResolvedExpr &expr, bool allowSideEffects) {
    debug_func(expr.location << " " << expr.className() << " allowSideEffects: " << allowSideEffects);
    if (m_depth >= MAX_RECURSION_DEPTH) {
        if (allowSideEffects) report(expr.location, "maximum recursion depth reached in comptime evaluation");
        return std::nullopt;
    }
    m_depth++;
    defer([&]() { m_depth--; });

    if (std::optional<ComptimeValue> val = expr.get_constant_value()) {
        return val;
    }
    if (const auto *intLiteral = dynamic_cast<const ResolvedIntLiteral *>(&expr)) {
        return ComptimeValue((int64_t)intLiteral->value);
    }
    if (const auto *charLiteral = dynamic_cast<const ResolvedCharLiteral *>(&expr)) {
        return ComptimeValue((int64_t)charLiteral->value);
    }
    if (const auto *boolLiteral = dynamic_cast<const ResolvedBoolLiteral *>(&expr)) {
        return ComptimeValue(boolLiteral->value);
    }
    if (const auto *floatLiteral = dynamic_cast<const ResolvedFloatLiteral *>(&expr)) {
        return ComptimeValue(floatLiteral->value);
    }
    if (const auto *stringLiteral = dynamic_cast<const ResolvedStringLiteral *>(&expr)) {
        return ComptimeValue(stringLiteral->value);
    }
    if (const auto *groupingExpr = dynamic_cast<const ResolvedGroupingExpr *>(&expr)) {
        return evaluate(*groupingExpr->expr, allowSideEffects);
    }
    if (const auto *unaryOperator = dynamic_cast<const ResolvedUnaryOperator *>(&expr)) {
        return evaluate_unary_operator(*unaryOperator, allowSideEffects);
    }
    if (const auto *binaryOperator = dynamic_cast<const ResolvedBinaryOperator *>(&expr)) {
        return evaluate_binary_operator(*binaryOperator, allowSideEffects);
    }
    if (const auto *declRefExpr = dynamic_cast<const ResolvedDeclRefExpr *>(&expr)) {
        return evaluate_decl_ref_expr(*declRefExpr, allowSideEffects);
    }
    if (const auto *memberExpr = dynamic_cast<const ResolvedMemberExpr *>(&expr)) {
        if (memberExpr->base->type->kind == ResolvedTypeKind::UnionDecl) {
            if (auto field = dynamic_cast<const ResolvedFieldDecl *>(&memberExpr->member)) {
                return ComptimeValue((int64_t)field->index);
            }
        }

        if (memberExpr->member.identifier == "len") {
            auto baseVal = evaluate(*memberExpr->base, allowSideEffects);
            if (baseVal && baseVal->isArray()) {
                return ComptimeValue((int64_t)baseVal->getArray().elements.size());
            } else if (baseVal && baseVal->isSlice()) {
                return ComptimeValue((int64_t)baseVal->getSlice().elements.size());
            } else if (baseVal && baseVal->isString()) {
                return ComptimeValue((int64_t)baseVal->getString().size());
            }
        }
        if (memberExpr->member.identifier == "ptr") {
            auto baseVal = evaluate(*memberExpr->base, allowSideEffects);
            if (baseVal && baseVal->isString()) {
                return baseVal;
            }
        }
        {
            auto baseVal = evaluate(*memberExpr->base, allowSideEffects);
            if (baseVal && baseVal->isStruct()) {
                const auto &fields = baseVal->getStruct().fields;
                for (auto &&[key, val] : fields) {
                    if (key == memberExpr->member.identifier) {
                        return val;
                    }
                }
            }
            if (baseVal && baseVal->isUnion()) {
                if (memberExpr->member.identifier == "tag") {
                    return ComptimeValue(baseVal->getUnion().activeTag);
                }
                auto &unionPayload = baseVal->getUnion().payload;
                if (unionPayload) return *unionPayload;
                return ComptimeValue();
            }
        }
        return evaluate_decl(memberExpr->member, allowSideEffects);
    }
    if (const auto *callExpr = dynamic_cast<const ResolvedCallExpr *>(&expr)) {
        return evaluate_call_expr(*callExpr, allowSideEffects);
    }
    if (const auto *arrayAtExpr = dynamic_cast<const ResolvedArrayAtExpr *>(&expr)) {
        if (!arrayAtExpr->array || !arrayAtExpr->index) {
            return std::nullopt;
        }
        auto containerVal = evaluate(*arrayAtExpr->array, allowSideEffects);
        auto indexVal = evaluate(*arrayAtExpr->index, allowSideEffects);
        if (containerVal && indexVal && indexVal->isInt()) {
            int64_t idx = indexVal->getInt();
            if (containerVal->isArray()) {
                const auto &arr = containerVal->getArray();
                if (idx >= 0 && (size_t)idx < arr.elements.size()) {
                    return arr.elements[idx];
                }
            } else if (containerVal->isString()) {
                const auto &str = containerVal->getString();
                if (idx >= 0 && (size_t)idx < str.size()) {
                    return ComptimeValue((int64_t)str[idx]);
                }
            }
        }
        return std::nullopt;
    }
    if (const auto *derefExpr = dynamic_cast<const ResolvedDerefPtrExpr *>(&expr)) {
        return evaluate(*derefExpr->expr, allowSideEffects);
    }
    if (const auto *comptimeExpr = dynamic_cast<const ResolvedComptimeExpr *>(&expr)) {
        return evaluate(*comptimeExpr->expr, true);
    }
    if (const auto *typeExpr = dynamic_cast<const ResolvedTypeExpr *>(&expr)) {
        return ComptimeValue(typeExpr->resolvedType->clone());
    }
    if (const auto *unionInst = dynamic_cast<const ResolvedUnionInstantiationExpr *>(&expr)) {
        auto fieldVal = evaluate(*unionInst->fieldInitializer->initializer, allowSideEffects);
        if (!fieldVal) return std::nullopt;
        int64_t tag = unionInst->fieldInitializer->field.index;
        auto fieldName = unionInst->fieldInitializer->field.identifier;
        return ComptimeValue(ComptimeValue::Union{tag, fieldName, makePtr<ComptimeValue>(std::move(*fieldVal))});
    }
    if (const auto *structInst = dynamic_cast<const ResolvedStructInstantiationExpr *>(&expr)) {
        ComptimeValue::Struct structVal;
        for (auto &&init : structInst->fieldInitializers) {
            auto fieldVal = evaluate(*init->initializer, allowSideEffects);
            if (!fieldVal) return std::nullopt;
            structVal.fields.emplace_back(init->field.identifier, *fieldVal);
        }
        return ComptimeValue(std::move(structVal));
    }
    if (const auto *arrayInst = dynamic_cast<const ResolvedArrayInstantiationExpr *>(&expr)) {
        if (!arrayInst->initializers.empty()) {
            ComptimeValue::Array arrayVal;
            for (auto &&init : arrayInst->initializers) {
                auto fieldVal = evaluate(*init, allowSideEffects);
                if (!fieldVal) return std::nullopt;
                arrayVal.elements.emplace_back(std::move(*fieldVal));
            }
            return ComptimeValue(std::move(arrayVal));
        }
    }
    return expr.get_constant_value();
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_call_expr(const ResolvedCallExpr &expr,
                                                                             bool allowSideEffects) {
    debug_func(expr.location << " " << expr.callee->className() << " allowSideEffects: " << allowSideEffects);
    if (auto constVal = expr.callee->get_constant_value()) {
        return constVal;
    }
    const ResolvedDecl *resolvedDecl = nullptr;
    if (auto memberExpr = dynamic_cast<ResolvedMemberExpr *>(expr.callee.get())) {
        resolvedDecl = &memberExpr->member;
    } else if (auto declRef = dynamic_cast<ResolvedDeclRefExpr *>(expr.callee.get())) {
        resolvedDecl = &declRef->decl;
    }

    if (!resolvedDecl) {
        return std::nullopt;
    }

    while (auto varDecl = dynamic_cast<const ResolvedVarDecl *>(resolvedDecl)) {
        if (varDecl->isMutable) break;
        if (varDecl->initializer) {
            if (auto dre = dynamic_cast<const ResolvedDeclRefExpr *>(varDecl->initializer.get())) {
                resolvedDecl = &dre->decl;

            } else if (auto me = dynamic_cast<const ResolvedMemberExpr *>(varDecl->initializer.get())) {
                resolvedDecl = &me->member;

            } else {
                break;
            }
        } else {
            break;
        }
    }

    if (auto builtin = dynamic_cast<const ResolvedBuiltinFunctionDecl *>(resolvedDecl)) {
        if (builtin->identifier == "@hasMethod") {
            return expr.get_constant_value();
        } else if (builtin->identifier == "@typeid") {
            auto &typeArg = expr.arguments[0];
            ptr<ResolvedType> targetType = typeArg->type->clone();
            if (auto typeExpr = dynamic_cast<ResolvedTypeExpr *>(typeArg.get())) {
                targetType = typeExpr->resolvedType->clone();
            }
            if (dynamic_cast<const ResolvedTypeType *>(targetType.get())) {
                if (auto comptimeVal = typeArg->get_constant_value()) {
                    if (!comptimeVal->isType()) {
                        dmz_unreachable(typeArg->location, "need to have const value");
                    }
                    targetType = comptimeVal->getType()->clone();
                } else {
                    dmz_unreachable(typeArg->location, "need to have const value");
                }
            }
            return evaluate_type(*targetType);
        } else if (builtin->identifier == "@sizeof") {
            auto &typeArg = expr.arguments[0];
            ptr<ResolvedType> targetType = typeArg->type->clone();
            if (auto typeExpr = dynamic_cast<ResolvedTypeExpr *>(typeArg.get())) {
                targetType = typeExpr->resolvedType->clone();
            }
            if (dynamic_cast<const ResolvedTypeType *>(targetType.get())) {
                if (auto comptimeVal = typeArg->get_constant_value()) {
                    if (!comptimeVal->isType()) {
                        dmz_unreachable(typeArg->location, "need to have const value");
                    }
                    targetType = comptimeVal->getType()->clone();
                } else {
                    dmz_unreachable(typeArg->location, "need to have const value");
                }
            }
            return ComptimeValue((int64_t)std::max(CodegenUtils::typeBitSize(*targetType) / 8, 1));
        } else if (builtin->identifier == "@simd") {
            auto &typeArg = expr.arguments[0];
            ptr<ResolvedType> targetType;
            if (auto constVal = typeArg->get_constant_value()) {
                if (!constVal->isType()) {
                    report(typeArg->location, "cannot deduce vector type, expected constant type");
                    return std::nullopt;
                }
                targetType = constVal->getType();
            } else {
                report(typeArg->location, "cannot deduce vector type, expected constant type");
                return std::nullopt;
            }
            auto &sizeArg = expr.arguments[1];
            int64_t vectorSize;
            if (auto constVal = sizeArg->get_constant_value()) {
                if (!constVal->isInt()) {
                    report(sizeArg->location, "cannot deduce vector size, expected constant integer");
                    return std::nullopt;
                }
                vectorSize = constVal->getInt();
            } else {
                report(sizeArg->location, "cannot deduce vector size, expected constant integer");
                return std::nullopt;
            }

            if (vectorSize <= 0) {
                report(expr.location, "vector size must be greater than 0");
                return std::nullopt;
            }
            return ComptimeValue(makePtr<ResolvedTypeSimd>(expr.location, std::move(targetType), vectorSize));
        } else if (builtin->identifier == "@simdSize") {
            auto &typeArg = expr.arguments[0];
            ptr<ResolvedType> targetType = typeArg->type->clone();
            if (auto typeExpr = dynamic_cast<ResolvedTypeExpr *>(typeArg.get())) {
                targetType = typeExpr->resolvedType->clone();
            }
            if (dynamic_cast<const ResolvedTypeType *>(targetType.get())) {
                if (auto val = typeArg->get_constant_value()) {
                    if (val && val->isType()) {
                        targetType = val->getType()->clone();
                    }
                } else {
                    dmz_unreachable(typeArg->location, "need to have const value");
                }
            }
            int bit_simd_size = CodegenUtils::target_simd_size();
            int bit_type_size = CodegenUtils::typeBitSize(*targetType);
            return ComptimeValue((int64_t)(bit_simd_size / bit_type_size));
        } else if (builtin->identifier == "@typeinfo") {
            auto &typeArg = expr.arguments[0];
            ptr<ResolvedType> targetType = typeArg->type->clone();
            if (auto typeExpr = dynamic_cast<ResolvedTypeExpr *>(typeArg.get())) {
                targetType = typeExpr->resolvedType->clone();
            }
            if (dynamic_cast<const ResolvedTypeType *>(targetType.get())) {
                if (auto comptimeVal = typeArg->get_constant_value()) {
                    if (!comptimeVal->isType()) {
                        dmz_unreachable(typeArg->location, "need to have const value");
                    }
                    targetType = comptimeVal->getType()->clone();
                } else {
                    dmz_unreachable(typeArg->location, "need to have const value");
                }
            }

            int tag = evaluate_type(*targetType)->getInt();
            std::string fieldName;
            if (auto unionType = dynamic_cast<const ResolvedTypeUnion *>(expr.type.get())) {
                auto *unionDecl = unionType->unionDecl();
                if (m_sema && !m_sema->ensure_struct_members_resolved(*unionDecl)) return std::nullopt;
                if (tag >= 0 && tag < (int)unionDecl->fields.size()) {
                    fieldName = unionDecl->fields[tag]->identifier;
                }
            } else {
                dmz_unreachable(expr.location, "need to have union type");
            }

            ComptimeValue payload;

            switch (targetType->kind) {
                case ResolvedTypeKind::Number: {
                    auto &nt = static_cast<const ResolvedTypeNumber &>(*targetType);
                    ComptimeValue::Struct numberPayload;
                    numberPayload.fields.emplace_back("bits", ComptimeValue((int64_t)nt.bitSize));
                    payload = ComptimeValue(std::move(numberPayload));
                    break;
                }
                case ResolvedTypeKind::StructDecl:
                case ResolvedTypeKind::Struct: {
                    ResolvedStructDecl *structDecl = nullptr;
                    if (auto sd = dynamic_cast<const ResolvedTypeStructDecl *>(targetType.get()))
                        structDecl = sd->decl;
                    else if (auto sd = dynamic_cast<const ResolvedTypeStruct *>(targetType.get()))
                        structDecl = sd->decl;
                    if (structDecl) {
                        if (m_sema) {
                            if (!m_sema->ensure_struct_members_resolved(*structDecl)) return std::nullopt;
                            if (!m_sema->ensure_struct_funcs_resolved(*structDecl)) return std::nullopt;
                        }

                        ComptimeValue::Struct structPayload;
                        structPayload.fields.emplace_back("name", ComptimeValue(structDecl->name()));

                        ComptimeValue::Slice fieldSlice;
                        for (auto &fieldName : structDecl->fields_strs) {
                            fieldSlice.elements.emplace_back(fieldName);
                        }
                        structPayload.fields.emplace_back("fields", ComptimeValue(std::move(fieldSlice)));

                        ComptimeValue::Slice methodSlice;
                        for (auto &methodName : structDecl->functions_strs) {
                            methodSlice.elements.emplace_back(methodName);
                        }
                        structPayload.fields.emplace_back("methods", ComptimeValue(std::move(methodSlice)));

                        payload = ComptimeValue(std::move(structPayload));
                    }
                    break;
                }
                case ResolvedTypeKind::Slice: {
                    auto &st = static_cast<const ResolvedTypeSlice &>(*targetType);
                    ComptimeValue::Struct slicePayload;
                    slicePayload.fields.emplace_back("inner", ComptimeValue(st.sliceType->clone()));
                    payload = ComptimeValue(std::move(slicePayload));
                    break;
                }
                case ResolvedTypeKind::Simd: {
                    auto &st = static_cast<const ResolvedTypeSimd &>(*targetType);
                    ComptimeValue::Struct simdPayload;
                    simdPayload.fields.emplace_back("name", ComptimeValue(st.to_str()));
                    simdPayload.fields.emplace_back("len", ComptimeValue((int64_t)st.simdSize));
                    payload = ComptimeValue(std::move(simdPayload));
                    break;
                }
                default:
                    break;
            }

            return ComptimeValue(ComptimeValue::Union{tag, fieldName, makePtr<ComptimeValue>(std::move(payload))});
        }
    } else if (auto func = dynamic_cast<const ResolvedFunctionDecl *>(resolvedDecl)) {
        if (!allowSideEffects) return std::nullopt;
        if (!func->body) {
            if (m_sema) {
                m_sema->ensure_fully_resolved(const_cast<ResolvedFunctionDecl &>(*func));
            }
            if (!func->body) {
                report(expr.location,
                       "function '" + func->name() + "' has no body and cannot be evaluated at compile time");
                return std::nullopt;
            }
        }
        debug_msg(expr.location << func->className() << " " << func->name()
                                << " allowSideEffects: " << allowSideEffects);
        std::vector<ComptimeValue> argValues;
        for (auto &&arg : expr.arguments) {
            auto val = evaluate(*arg, allowSideEffects);
            if (!val) return std::nullopt;
            argValues.push_back(*val);
            debug_msg("arg: " << *val);
        }

        // Check cache
        auto it = m_callCache.find(func);
        if (it != m_callCache.end()) {
            if (it->second.args.size() == argValues.size()) {
                bool match = true;
                for (size_t i = 0; i < argValues.size(); ++i) {
                    if (!(it->second.args[i] == argValues[i])) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    debug_msg("CACHE HIT! " << func->name() << " " << it->second.result);
                    return it->second.result;
                }
            }
        }

        auto oldEnv = m_env;
        auto oldShouldReturn = m_shouldReturn;
        auto oldReturnValue = m_returnValue;

        m_shouldReturn = false;
        m_returnValue = std::nullopt;

        for (size_t i = 0; i < func->params.size(); i++) {
            m_env[func->params[i].get()] = argValues[i];
        }

        evaluate_block(*func->body, allowSideEffects);

        auto result = m_returnValue;

        m_env = oldEnv;
        m_shouldReturn = oldShouldReturn;
        m_returnValue = oldReturnValue;

        if (result) {
            debug_msg(" NEW RESULT! " << func->name() << " " << *result);
            m_callCache.emplace(func, CallCacheEntry{func, std::move(argValues), *result});
        }

        return result;
    }
    return expr.get_constant_value();
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_type(const ResolvedType &type) {
    debug_func(type.location << " " << type.className());
    switch (type.kind) {
        case ResolvedTypeKind::Void:
            return ComptimeValue((int64_t)0);
        case ResolvedTypeKind::Number: {
            auto &nt = static_cast<const ResolvedTypeNumber &>(type);
            if (nt.numberKind == ResolvedNumberKind::Int) return ComptimeValue((int64_t)1);
            if (nt.numberKind == ResolvedNumberKind::UInt) return ComptimeValue((int64_t)2);
            if (nt.numberKind == ResolvedNumberKind::Float) return ComptimeValue((int64_t)3);
            break;
        }
        case ResolvedTypeKind::Bool:
            return ComptimeValue((int64_t)4);
        case ResolvedTypeKind::Struct: {
            auto &st = static_cast<const ResolvedTypeStruct &>(type);
            if (st.decl->isTuple) return ComptimeValue((int64_t)6);
            return ComptimeValue((int64_t)5);
        }
        case ResolvedTypeKind::StructDecl: {
            auto &st = static_cast<const ResolvedTypeStructDecl &>(type);
            if (st.decl->isTuple) return ComptimeValue((int64_t)6);
            return ComptimeValue((int64_t)5);
        }
        case ResolvedTypeKind::Pointer:
            return ComptimeValue((int64_t)7);
        case ResolvedTypeKind::Slice:
            return ComptimeValue((int64_t)8);
        case ResolvedTypeKind::Range:
            return ComptimeValue((int64_t)9);
        case ResolvedTypeKind::Array:
            return ComptimeValue((int64_t)10);
        case ResolvedTypeKind::Function:
            return ComptimeValue((int64_t)11);
        case ResolvedTypeKind::Error:
        case ResolvedTypeKind::ErrorGroup:
            return ComptimeValue((int64_t)12);
        case ResolvedTypeKind::Optional:
            return ComptimeValue((int64_t)13);
        case ResolvedTypeKind::Simd:
            return ComptimeValue((int64_t)14);
        case ResolvedTypeKind::UnionDecl:
        case ResolvedTypeKind::Union:
            return ComptimeValue((int64_t)15);
        default:
            break;
    }
    return ComptimeValue((int64_t)99);
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_unary_operator(const ResolvedUnaryOperator &unop,
                                                                                  bool allowSideEffects) {
    debug_func(unop.location << " " << unop.op << " " << unop.operand << " allowSideEffects: " << allowSideEffects);
    std::optional<ComptimeValue> operandVal = evaluate(*unop.operand, allowSideEffects);
    if (!operandVal) return std::nullopt;

    if (unop.op == TokenType::op_plusplus || unop.op == TokenType::op_minusminus) {
        if (!allowSideEffects) return std::nullopt;
        if (auto dre = dynamic_cast<const ResolvedDeclRefExpr *>(unop.operand.get())) {
            auto currentVal = evaluate_decl(dre->decl, allowSideEffects);
            if (currentVal && currentVal->isInt()) {
                int64_t newVal = currentVal->getInt() + (unop.op == TokenType::op_plusplus ? 1 : -1);
                m_env[&dre->decl] = ComptimeValue(newVal);
                return ComptimeValue(newVal);
            }
        }
        report(unop.location, "increment/decrement only supported on integer variables in comptime");
        return std::nullopt;
    }

    auto optVal = operandVal->toInt();
    if (!optVal.has_value()) {
        if (allowSideEffects) report(unop.location, "unary operand is not an integer");
        return std::nullopt;
    }
    int64_t val = optVal.value();
    switch (unop.op) {
        case TokenType::op_minus:
            return ComptimeValue(-val);
        case TokenType::op_excla_mark:
            return ComptimeValue(!val);
        case TokenType::op_tilde:
            return ComptimeValue(~val);
        default:
            dmz_unreachable(unop.location, "unexpected binary operator");
    }

    dmz_unreachable(unop.location, "unexpected unary operator");
}

template <typename T>
ComptimeValue binop_value(SourceLocation loc, TokenType op, T val1, T val2) {
    switch (op) {
        case TokenType::asterisk:
        case TokenType::op_asterisk_equal:
            return ComptimeValue(val1 * val2);
        case TokenType::op_div:
        case TokenType::op_div_equal:
            return ComptimeValue(val1 / val2);
        case TokenType::op_plus:
        case TokenType::op_plus_equal:
            return ComptimeValue(val1 + val2);
        case TokenType::op_minus:
        case TokenType::op_minus_equal:
            return ComptimeValue(val1 - val2);
        case TokenType::op_less:
            return ComptimeValue(val1 < val2);
        case TokenType::op_less_eq:
            return ComptimeValue(val1 <= val2);
        case TokenType::op_more:
            return ComptimeValue(val1 > val2);
        case TokenType::op_more_eq:
            return ComptimeValue(val1 >= val2);
        case TokenType::op_equal:
            return ComptimeValue(val1 == val2);
        case TokenType::op_not_equal:
            return ComptimeValue(val1 != val2);
        default:
            break;
    }
    if constexpr (std::is_integral_v<T>) {
        switch (op) {
            case TokenType::op_percent:
                return ComptimeValue(val1 % val2);
            case TokenType::amp:
                return ComptimeValue(val1 & val2);
            case TokenType::pipe:
                return ComptimeValue(val1 | val2);
            case TokenType::caret:
                return ComptimeValue(val1 ^ val2);
            case TokenType::op_shl:
                return ComptimeValue(val1 << val2);
            case TokenType::op_shr:
                return ComptimeValue(val1 >> val2);
            default:
                break;
        }
    }
    dmz_unreachable(loc, "unexpected binary operator");
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_binary_operator(const ResolvedBinaryOperator &binop,
                                                                                   bool allowSideEffects) {
    debug_func(binop.location << " " << binop.op << " " << binop.lhs->className() << " " << binop.rhs->className()
                              << " allowSideEffects: " << allowSideEffects);
    std::optional<ComptimeValue> lhs = evaluate(*binop.lhs, allowSideEffects);

    if (!lhs && !allowSideEffects) return std::nullopt;

    if (binop.op == TokenType::pipepipe) {
        if (to_bool(lhs) == true) return ComptimeValue(true);

        std::optional<ComptimeValue> rhs = evaluate(*binop.rhs, allowSideEffects);
        if (to_bool(rhs) == true) return ComptimeValue(true);
        if (lhs && rhs) return ComptimeValue(false);

        return std::nullopt;
    }
    if (binop.op == TokenType::ampamp) {
        if (to_bool(lhs) == false) return ComptimeValue(false);

        std::optional<ComptimeValue> rhs = evaluate(*binop.rhs, allowSideEffects);
        if (to_bool(rhs) == false) return ComptimeValue(false);

        if (lhs && rhs) return ComptimeValue(true);

        return std::nullopt;
    }
    if (!lhs) {
        if (allowSideEffects) report(binop.lhs->location, "left operand of binary operator cannot be evaluated");
        return std::nullopt;
    }

    std::optional<ComptimeValue> rhs = evaluate(*binop.rhs, allowSideEffects);
    if (!rhs) {
        if (allowSideEffects) report(binop.rhs->location, "right operand of binary operator cannot be evaluated");
        return std::nullopt;
    }
    bool isFloatOp = lhs->isFloat() || rhs->isFloat();
    if (isFloatOp) {
        double val1 = lhs->getFloat();
        double val2 = rhs->getFloat();
        return binop_value<double>(binop.location, binop.op, val1, val2);
    } else {
        auto lhsInt = lhs->toInt();
        if (!lhsInt.has_value()) {
            if (allowSideEffects) report(binop.lhs->location, "left operand of binary operator is not an integer");
            return std::nullopt;
        }
        auto rhsInt = rhs->toInt();
        if (!rhsInt.has_value()) {
            if (allowSideEffects) report(binop.rhs->location, "right operand of binary operator is not an integer");
            return std::nullopt;
        }
        int64_t val1 = lhsInt.value();
        int64_t val2 = rhsInt.value();
        return binop_value<int64_t>(binop.location, binop.op, val1, val2);
    }
}

bool ConstantExpressionEvaluator::should_report_error() const {
    if (!m_sema) return true;
    if (m_sema->getCurrentFunction() && dynamic_cast<ResolvedGenericFunctionDecl *>(m_sema->getCurrentFunction())) {
        return false;
    }
    return true;
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_decl_ref_expr(const ResolvedDeclRefExpr &dre,
                                                                                 bool allowSideEffects) {
    debug_func(dre.location << " " << dre.decl.identifier << " allowSideEffects: " << allowSideEffects);
    auto res = evaluate_decl(dre.decl, allowSideEffects);
    if (!res && allowSideEffects) {
        if (should_report_error()) {
            report(dre.location, "declaration '" + dre.decl.identifier + "' cannot be evaluated at compile time");
        }
    }
    return res;
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_decl(const ResolvedDecl &decl,
                                                                        bool allowSideEffects) {
    debug_func(decl.location << " " << decl.className() << " " << decl.identifier
                             << " allowSideEffects: " << allowSideEffects);
    if (m_env.count(&decl)) {
        return m_env[&decl];
    }
    if (const auto *rvd = dynamic_cast<const ResolvedVarDecl *>(&decl)) {
        if (!rvd->initializer) {
            if (allowSideEffects) report(rvd->location, "variable '" + rvd->identifier + "' has no initializer");
            return std::nullopt;
        }
        if (rvd->isMutable && !allowSideEffects) return std::nullopt;
        auto val = evaluate(*rvd->initializer, allowSideEffects);
        if (val && allowSideEffects) {
            m_env[&decl] = *val;
        }
        return val;
    } else if (const auto *rds = dynamic_cast<const ResolvedDeclStmt *>(&decl)) {
        if (!rds->varDecl || !rds->varDecl->initializer) {
            if (allowSideEffects) report(rds->location, "declaration has no initializer");
            return std::nullopt;
        }
        if (rds->varDecl->isMutable && !allowSideEffects) return std::nullopt;
        auto val = evaluate(*rds->varDecl->initializer, allowSideEffects);
        if (val && allowSideEffects) {
            m_env[rds->varDecl.get()] = *val;
        }
        return val;
    } else if (const auto *field = dynamic_cast<const ResolvedFieldDecl *>(&decl)) {
        return field->get_constant_value();
    } else if (const auto *param = dynamic_cast<const ResolvedParamDecl *>(&decl)) {
        return param->get_constant_value();
    }

    if (allowSideEffects && should_report_error())
        report(decl.location, "cannot evaluate declaration at compile time: " + decl.identifier);
    return std::nullopt;
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_block(const ResolvedBlock &block,
                                                                         bool allowSideEffects) {
    for (auto &&stmt : block.statements) {
        evaluate_stmt(*stmt, allowSideEffects);
        if (m_shouldReturn || m_shouldBreak || m_shouldContinue) break;
    }
    return std::nullopt;
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_stmt(const ResolvedStmt &stmt,
                                                                        bool allowSideEffects) {
    debug_func(stmt.location << " " << stmt.className() << " allowSideEffects: " << allowSideEffects);
    if (const auto *expr = dynamic_cast<const ResolvedExpr *>(&stmt)) {
        evaluate(*expr, allowSideEffects);
    } else if (const auto *ret = dynamic_cast<const ResolvedReturnStmt *>(&stmt)) {
        if (ret->expr) {
            m_returnValue = evaluate(*ret->expr, allowSideEffects);
        }
        m_shouldReturn = true;
    } else if (const auto *ifStmt = dynamic_cast<const ResolvedIfStmt *>(&stmt)) {
        auto cond = to_bool(evaluate(*ifStmt->condition, allowSideEffects));
        if (cond == true) {
            evaluate_block(*ifStmt->trueBlock, allowSideEffects);
        } else if (ifStmt->falseBlock) {
            evaluate_block(*ifStmt->falseBlock, allowSideEffects);
        }
    } else if (const auto *whileStmt = dynamic_cast<const ResolvedWhileStmt *>(&stmt)) {
        while (to_bool(evaluate(*whileStmt->condition, allowSideEffects)) == true) {
            evaluate_block(*whileStmt->body, allowSideEffects);
            if (m_shouldReturn || m_shouldBreak) break;
            m_shouldContinue = false;
        }
        m_shouldBreak = false;
    } else if (const auto *forStmt = dynamic_cast<const ResolvedForStmt *>(&stmt)) {
        if (forStmt->conditions.size() == 1) {
            if (const auto *range = dynamic_cast<const ResolvedRangeExpr *>(forStmt->conditions[0].get())) {
                auto startVal = evaluate(*range->startExpr, allowSideEffects);
                auto endVal = evaluate(*range->endExpr, allowSideEffects);
                if (startVal && endVal && startVal->isInt() && endVal->isInt()) {
                    int64_t start = startVal->getInt();
                    int64_t end = endVal->getInt();
                    for (int64_t i = start; i < end; ++i) {
                        m_env[forStmt->captures[0].get()] = ComptimeValue(i);
                        evaluate_block(*forStmt->body, allowSideEffects);
                        if (m_shouldReturn || m_shouldBreak) break;
                        m_shouldContinue = false;
                    }
                    m_shouldBreak = false;
                }
            } else {
                auto containerVal = evaluate(*forStmt->conditions[0], allowSideEffects);
                if (containerVal && containerVal->isArray()) {
                    const auto &arr = containerVal->getArray();
                    for (auto &&elem : arr.elements) {
                        m_env[forStmt->captures[0].get()] = elem;
                        evaluate_block(*forStmt->body, allowSideEffects);
                        if (m_shouldReturn || m_shouldBreak) break;
                        m_shouldContinue = false;
                    }
                    m_shouldBreak = false;
                } else if (containerVal && containerVal->isString()) {
                    const auto &str = containerVal->getString();
                    for (auto &&elem : str) {
                        m_env[forStmt->captures[0].get()] = ComptimeValue((int64_t)elem);
                        evaluate_block(*forStmt->body, allowSideEffects);
                        if (m_shouldReturn || m_shouldBreak) break;
                        m_shouldContinue = false;
                    }
                    m_shouldBreak = false;
                }
            }
        }
    } else if (const auto *switchStmt = dynamic_cast<const ResolvedSwitchStmt *>(&stmt)) {
        auto condVal = evaluate(*switchStmt->condition, allowSideEffects);
        if (condVal) {
            bool matched = false;
            for (auto &&caseStmt : switchStmt->cases) {
                for (auto &&caseCond : caseStmt->conditions) {
                    auto caseCondVal = evaluate(*caseCond, allowSideEffects);
                    if (caseCondVal && *caseCondVal == *condVal) {
                        evaluate_block(*caseStmt->block, allowSideEffects);
                        matched = true;
                        break;
                    }
                }
                if (matched) break;
            }
            if (!matched && switchStmt->elseBlock) {
                evaluate_block(*switchStmt->elseBlock, allowSideEffects);
            }
        }
    } else if (dynamic_cast<const ResolvedBreakStmt *>(&stmt)) {
        m_shouldBreak = true;
    } else if (dynamic_cast<const ResolvedContinueStmt *>(&stmt)) {
        m_shouldContinue = true;
    } else if (const auto *declStmt = dynamic_cast<const ResolvedDeclStmt *>(&stmt)) {
        if (declStmt->varDecl && declStmt->varDecl->initializer) {
            auto val = evaluate(*declStmt->varDecl->initializer, allowSideEffects);
            if (val) {
                m_env[declStmt->varDecl.get()] = *val;
            }
        }
    } else if (const auto *assign = dynamic_cast<const ResolvedAssignment *>(&stmt)) {
        auto val = evaluate(*assign->expr, allowSideEffects);
        if (val) {
            if (auto dre = dynamic_cast<const ResolvedDeclRefExpr *>(assign->assignee.get())) {
                m_env[&dre->decl] = *val;
            }
        }
    }
    return std::nullopt;
}
}  // namespace DMZ
