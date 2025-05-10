#include "Constexpr.hpp"

namespace C {

bool ConstantExpressionEvaluator::to_bool(std::optional<ConstValue> d) {
    if (!d) return false;
    bool retval;
    std::visit([&retval](auto val) { retval = val != 0; }, *d);
    return retval;
}

std::optional<ConstValue> ConstantExpressionEvaluator::evaluate(const ResolvedExpr &expr, bool allowSideEffects) {
    if (std::optional<ConstValue> val = expr.get_constant_value()) {
        return val;
    }
    if (const auto *numberLiteral = dynamic_cast<const ResolvedIntLiteral *>(&expr)) {
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

template <typename T>
T do_unary_op(TokenType type, ConstValue &cval) {
    T val = std::get<T>(cval);
    switch (type) {
        case TokenType::op_minus:
            return -val;
        default:
            llvm_unreachable("unexpected binary operator");
    }
}

std::optional<ConstValue> ConstantExpressionEvaluator::evaluate_unary_operator(const ResolvedUnaryOperator &unop,
                                                                               bool allowSideEffects) {
    std::optional<ConstValue> operand = evaluate(*unop.operand, allowSideEffects);
    if (!operand) return std::nullopt;

    if (unop.op == TokenType::op_not) return !to_bool(operand);

    if (unop.op == TokenType::op_minus) {
        if (std::holds_alternative<int>(*operand)) {
            return do_unary_op<int>(unop.op, *operand);
        } else if (std::holds_alternative<char>(*operand)) {
            return do_unary_op<char>(unop.op, *operand);
        } else {
            llvm_unreachable("unexpected type in ConstValue");
        }
    }

    llvm_unreachable("unexpected unary operator");
}

template <typename T>
T do_binary_op(TokenType type, ConstValue &cval1, ConstValue &cval2) {
    T val1 = std::get<T>(cval1);
    T val2 = std::get<T>(cval2);
    switch (type) {
        case TokenType::op_mult:
            return val1 * val2;
        case TokenType::op_div:
            return val1 / val2;
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
            llvm_unreachable("unexpected binary operator");
    }
}

std::optional<ConstValue> ConstantExpressionEvaluator::evaluate_binary_operator(const ResolvedBinaryOperator &binop,
                                                                                bool allowSideEffects) {
    std::optional<ConstValue> lhs = evaluate(*binop.lhs);

    if (!lhs && !allowSideEffects) return std::nullopt;

    if (binop.op == TokenType::op_or) {
        if (to_bool(lhs) == true) return 1;

        std::optional<ConstValue> rhs = evaluate(*binop.rhs, allowSideEffects);
        if (to_bool(rhs) == true) return 1;
        if (lhs && rhs) return 0;

        return std::nullopt;
    }
    if (binop.op == TokenType::op_and) {
        if (to_bool(lhs) == false) return 0;

        std::optional<ConstValue> rhs = evaluate(*binop.rhs, allowSideEffects);
        if (to_bool(rhs) == false) return 0;

        if (lhs && rhs) return 1;

        return std::nullopt;
    }
    if (!lhs) return std::nullopt;

    std::optional<ConstValue> rhs = evaluate(*binop.rhs);
    if (!rhs) return std::nullopt;

    if ((*lhs).index() != (*rhs).index()) return std::nullopt;

    if (std::holds_alternative<int>(*lhs)) {
        return do_binary_op<int>(binop.op, *lhs, *rhs);
    } else if (std::holds_alternative<char>(*lhs)) {
        return do_binary_op<char>(binop.op, *lhs, *rhs);
    } else {
        llvm_unreachable("unexpected type in ConstValue");
    }
}

std::optional<ConstValue> ConstantExpressionEvaluator::evaluate_decl_ref_expr(const ResolvedDeclRefExpr &dre,
                                                                              bool allowSideEffects) {
    const auto *rvd = dynamic_cast<const ResolvedVarDecl *>(&dre.decl);
    if (!rvd || rvd->isMutable || !rvd->initializer) return std::nullopt;

    return evaluate(*rvd->initializer, allowSideEffects);
}
}  // namespace C
