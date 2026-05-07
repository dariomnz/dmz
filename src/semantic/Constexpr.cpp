#include "semantic/Constexpr.hpp"

#include "codegen/CodegenUtils.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

std::optional<bool> ConstantExpressionEvaluator::to_bool(const std::optional<ComptimeValue> &d) {
    if (!d) return std::nullopt;
    if (d->isBool()) return d->getBool();
    if (d->isInt()) return d->getInt() != 0;
    return std::nullopt;
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate(const ResolvedExpr &expr, bool allowSideEffects) {
    if (m_depth >= MAX_RECURSION_DEPTH) {
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
        return evaluate_decl(memberExpr->member, allowSideEffects);
    }
    if (const auto *callExpr = dynamic_cast<const ResolvedCallExpr *>(&expr)) {
        return evaluate_call_expr(*callExpr, allowSideEffects);
    }
    return expr.get_constant_value();
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_call_expr(const ResolvedCallExpr &expr,
                                                                    [[maybe_unused]] bool allowSideEffects) {
    if (auto constVal = expr.callee->get_constant_value()) {
        return constVal;
    }
    if (auto declRef = dynamic_cast<ResolvedDeclRefExpr *>(expr.callee.get())) {
        if (auto func = dynamic_cast<const ResolvedBuiltinFunctionDecl *>(&declRef->decl)) {
            if (func->identifier == "@hasMethod") {
                return expr.get_constant_value();
            } else if (func->identifier == "@typeid") {
                auto &typeArg = expr.arguments[0];
                return evaluate_type(*typeArg->type);
            } else if (func->identifier == "@sizeof") {
                auto &typeArg = expr.arguments[0];
                return ComptimeValue((int64_t)std::max(CodegenUtils::typeBitSize(*typeArg->type) / 8, 1));
            } else if (func->identifier == "@simdSize") {
                auto &typeArg = expr.arguments[0];
                int bit_simd_size = CodegenUtils::target_simd_size();
                int bit_type_size = CodegenUtils::typeBitSize(*typeArg->type);
                return ComptimeValue((int64_t)(bit_simd_size / bit_type_size));
            }
        }
    }
    return expr.get_constant_value();
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_type(const ResolvedType &type) {
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
        default:
            break;
    }
    return ComptimeValue((int64_t)99);
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_unary_operator(const ResolvedUnaryOperator &unop,
                                                                         bool allowSideEffects) {
    std::optional<ComptimeValue> operand = evaluate(*unop.operand, allowSideEffects);
    auto optVal = operand->toInt();
    if (!optVal.has_value()) return std::nullopt;
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

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_binary_operator(const ResolvedBinaryOperator &binop,
                                                                          bool allowSideEffects) {
    std::optional<ComptimeValue> lhs = evaluate(*binop.lhs);

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
    auto lhsInt = lhs->toInt();
    if (!lhs || !lhsInt.has_value()) return std::nullopt;

    std::optional<ComptimeValue> rhs = evaluate(*binop.rhs);
    auto rhsInt = rhs->toInt();
    if (!rhs || !rhsInt.has_value()) return std::nullopt;

    int64_t val1 = lhsInt.value();
    int64_t val2 = rhsInt.value();
    switch (binop.op) {
        case TokenType::asterisk:
            return ComptimeValue(val1 * val2);
        case TokenType::op_div:
            return ComptimeValue(val1 / val2);
        case TokenType::op_percent:
            return ComptimeValue(val1 % val2);
        case TokenType::op_plus:
            return ComptimeValue(val1 + val2);
        case TokenType::op_minus:
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
            dmz_unreachable(binop.location, "unexpected binary operator");
    }
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_decl_ref_expr(const ResolvedDeclRefExpr &dre,
                                                                        bool allowSideEffects) {
    return evaluate_decl(dre.decl, allowSideEffects);
}

std::optional<ComptimeValue> ConstantExpressionEvaluator::evaluate_decl(const ResolvedDecl &decl, bool allowSideEffects) {
    if (const auto *rvd = dynamic_cast<const ResolvedVarDecl *>(&decl)) {
        if (rvd->isMutable || !rvd->initializer) return std::nullopt;
        return evaluate(*rvd->initializer, allowSideEffects);
    } else if (const auto *rds = dynamic_cast<const ResolvedDeclStmt *>(&decl)) {
        if (!rds->varDecl || rds->isMutable || !rds->varDecl->initializer) return std::nullopt;
        return evaluate(*rds->varDecl->initializer, allowSideEffects);
    } else if (const auto *field = dynamic_cast<const ResolvedFieldDecl *>(&decl)) {
        return field->get_constant_value();
    }

    return std::nullopt;
}
}  // namespace DMZ
