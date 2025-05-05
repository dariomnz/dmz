#include "Constexpr.hpp"

namespace C {

static inline std::optional<bool> to_bool(std::optional<int> d) {
    if (!d) return std::nullopt;

    return d != 0.0;
}

std::optional<int> ConstantExpressionEvaluator::evaluate(const ResolvedExpr &expr, bool allowSideEffects) {
    if (std::optional<int> val = expr.get_constant_value()) return val;

    if (const auto *numberLiteral = dynamic_cast<const ResolvedNumberLiteral *>(&expr)) {
        return numberLiteral->value;
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

    return std::nullopt;
}

std::optional<int> ConstantExpressionEvaluator::evaluate_unary_operator(const ResolvedUnaryOperator &unop,
                                                                        bool allowSideEffects) {
    std::optional<int> operand = evaluate(*unop.operand, allowSideEffects);
    if (!operand) return std::nullopt;

    if (unop.op == TokenType::op_not) return !*to_bool(operand);

    if (unop.op == TokenType::op_minus) return -*operand;

    llvm_unreachable("unexpected unary operator");
}

std::optional<int> ConstantExpressionEvaluator::evaluate_binary_operator(const ResolvedBinaryOperator &binop,
                                                                         bool allowSideEffects) {
    std::optional<int> lhs = evaluate(*binop.lhs);

    if (!lhs && !allowSideEffects) return std::nullopt;

    if (binop.op == TokenType::op_or) {
        if (to_bool(lhs) == true) return 1.0;

        std::optional<int> rhs = evaluate(*binop.rhs, allowSideEffects);
        if (to_bool(rhs) == true) return 1.0;
        if (lhs && rhs) return 0.0;

        return std::nullopt;
    }
    if (binop.op == TokenType::op_and) {
        if (to_bool(lhs) == false) return 0.0;

        std::optional<int> rhs = evaluate(*binop.rhs, allowSideEffects);
        if (to_bool(rhs) == false) return 0.0;

        if (lhs && rhs) return 1.0;

        return std::nullopt;
    }
    if (!lhs) return std::nullopt;

    std::optional<int> rhs = evaluate(*binop.rhs);
    if (!rhs) return std::nullopt;

    switch (binop.op) {
        case TokenType::op_mult:
            return *lhs * *rhs;
        case TokenType::op_div:
            return *lhs / *rhs;
        case TokenType::op_plus:
            return *lhs + *rhs;
        case TokenType::op_minus:
            return *lhs - *rhs;
        case TokenType::op_less:
            return *lhs < *rhs;
        case TokenType::op_less_eq:
            return *lhs <= *rhs;
        case TokenType::op_more:
            return *lhs > *rhs;
        case TokenType::op_more_eq:
            return *lhs >= *rhs;
        case TokenType::op_equal:
            return *lhs == *rhs;
        case TokenType::op_not_equal:
            return *lhs != *rhs;
        default:
            llvm_unreachable("unexpected binary operator");
    }
}

std::optional<int> ConstantExpressionEvaluator::evaluate_decl_ref_expr(const ResolvedDeclRefExpr &dre,
                                                                       bool allowSideEffects) {
    const auto *rvd = dynamic_cast<const ResolvedVarDecl *>(&dre.decl);
    if (!rvd || rvd->isMutable || !rvd->initializer) return std::nullopt;

    return evaluate(*rvd->initializer, allowSideEffects);
}
}  // namespace C
