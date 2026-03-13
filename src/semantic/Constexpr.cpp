#include "semantic/Constexpr.hpp"

#include <optional>

#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

std::optional<bool> ConstantExpressionEvaluator::to_bool(std::optional<int> &d) {
    if (!d) return std::nullopt;
    return *d != 0;
}

std::optional<int> ConstantExpressionEvaluator::evaluate(const ResolvedExpr &expr, bool allowSideEffects) {
    if (std::optional<int> val = expr.get_constant_value()) {
        return val;
    }
    if (const auto *intLiteral = dynamic_cast<const ResolvedIntLiteral *>(&expr)) {
        return intLiteral->value;
    }
    if (const auto *charLiteral = dynamic_cast<const ResolvedCharLiteral *>(&expr)) {
        return charLiteral->value;
    }
    if (const auto *boolLiteral = dynamic_cast<const ResolvedBoolLiteral *>(&expr)) {
        return boolLiteral->value;
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
        return evaluate_decl(memberExpr->member, allowSideEffects);
    }
    if (const auto *typeidExpr = dynamic_cast<const ResolvedTypeidExpr *>(&expr)) {
        return evaluate(*typeidExpr, allowSideEffects);
    }
    if (const auto *typeExpr = dynamic_cast<const ResolvedTypeExpr *>(&expr)) {
        return evaluate(*typeExpr, allowSideEffects);
    }
    return std::nullopt;
}

std::optional<int> ConstantExpressionEvaluator::evaluate(const ResolvedTypeidExpr &expr,
                                                         [[maybe_unused]] bool allowSideEffects) {
    return evaluate_type(*expr.typeidExpr->type);
}

std::optional<int> ConstantExpressionEvaluator::evaluate(const ResolvedTypeExpr &expr,
                                                         [[maybe_unused]] bool allowSideEffects) {
    return evaluate_type(*expr.resolvedType);
}

std::optional<int> ConstantExpressionEvaluator::evaluate_type(const ResolvedType &type) {
    switch (type.kind) {
        case ResolvedTypeKind::Void:
            return 0;
        case ResolvedTypeKind::Number: {
            auto &nt = static_cast<const ResolvedTypeNumber &>(type);
            if (nt.numberKind == ResolvedNumberKind::Int) return 1;
            if (nt.numberKind == ResolvedNumberKind::UInt) return 2;
            if (nt.numberKind == ResolvedNumberKind::Float) return 3;
            break;
        }
        case ResolvedTypeKind::Bool:
            return 4;
        case ResolvedTypeKind::Struct:
        case ResolvedTypeKind::StructDecl:
            return 5;
        case ResolvedTypeKind::Pointer:
            return 6;
        case ResolvedTypeKind::Slice:
            return 7;
        case ResolvedTypeKind::Range:
            return 8;
        case ResolvedTypeKind::Array:
            return 9;
        case ResolvedTypeKind::Function:
            return 10;
        case ResolvedTypeKind::Error:
        case ResolvedTypeKind::ErrorGroup:
            return 11;
        case ResolvedTypeKind::Optional:
            return 12;
        default:
            break;
    }
    return 99;
}

std::optional<int> ConstantExpressionEvaluator::evaluate_unary_operator(const ResolvedUnaryOperator &unop,
                                                                        bool allowSideEffects) {
    std::optional<int> operand = evaluate(*unop.operand, allowSideEffects);
    if (!operand) return std::nullopt;

    int val = *operand;
    switch (unop.op) {
        case TokenType::op_minus:
            return -val;
        case TokenType::op_excla_mark:
            return !val;
        default:
            dmz_unreachable("unexpected binary operator");
    }

    dmz_unreachable("unexpected unary operator");
}

std::optional<int> ConstantExpressionEvaluator::evaluate_binary_operator(const ResolvedBinaryOperator &binop,
                                                                         bool allowSideEffects) {
    std::optional<int> lhs = evaluate(*binop.lhs);

    if (!lhs && !allowSideEffects) return std::nullopt;

    if (binop.op == TokenType::pipepipe) {
        if (to_bool(lhs) == true) return true;

        std::optional<int> rhs = evaluate(*binop.rhs, allowSideEffects);
        if (to_bool(rhs) == true) return true;
        if (lhs && rhs) return false;

        return std::nullopt;
    }
    if (binop.op == TokenType::ampamp) {
        if (to_bool(lhs) == false) return false;

        std::optional<int> rhs = evaluate(*binop.rhs, allowSideEffects);
        if (to_bool(rhs) == false) return false;

        if (lhs && rhs) return true;

        return std::nullopt;
    }
    if (!lhs) return std::nullopt;

    std::optional<int> rhs = evaluate(*binop.rhs);
    if (!rhs) return std::nullopt;

    int val1 = *lhs;
    int val2 = *rhs;
    switch (binop.op) {
        case TokenType::asterisk:
            return val1 * val2;
        case TokenType::op_div:
            return val1 / val2;
        case TokenType::op_percent:
            return val1 % val2;
        case TokenType::op_plus:
            return val1 + val2;
        case TokenType::op_minus:
            return val1 - val2;
        case TokenType::op_less:
            return val1 < val2;
        case TokenType::op_less_eq:
            return val1 <= val2;
        case TokenType::op_more:
            return val1 > val2;
        case TokenType::op_more_eq:
            return val1 >= val2;
        case TokenType::op_equal:
            return val1 == val2;
        case TokenType::op_not_equal:
            return val1 != val2;
        default:
            dmz_unreachable("unexpected binary operator");
    }
}

std::optional<int> ConstantExpressionEvaluator::evaluate_decl_ref_expr(const ResolvedDeclRefExpr &dre,
                                                                       bool allowSideEffects) {
    return evaluate_decl(dre.decl, allowSideEffects);
}

std::optional<int> ConstantExpressionEvaluator::evaluate_decl(const ResolvedDecl &decl, bool allowSideEffects) {
    if (const auto *rvd = dynamic_cast<const ResolvedVarDecl *>(&decl)) {
        if (rvd->isMutable || !rvd->initializer) return std::nullopt;
        return evaluate(*rvd->initializer, allowSideEffects);
    } else if (const auto *rds = dynamic_cast<const ResolvedDeclStmt *>(&decl)) {
        if (!rds->varDecl || rds->isMutable || !rds->varDecl->initializer) return std::nullopt;
        return evaluate(*rds->varDecl->initializer, allowSideEffects);
    }

    return std::nullopt;
}
}  // namespace DMZ
