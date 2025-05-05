#pragma once

#include "AST.hpp"
#include "PH.hpp"

namespace C {
class ConstantExpressionEvaluator {
   public:
    std::optional<int> evaluate(const ResolvedExpr &expr, bool allowSideEffects = false);
    std::optional<int> evaluate_unary_operator(const ResolvedUnaryOperator &unop, bool allowSideEffects);
    std::optional<int> evaluate_binary_operator(const ResolvedBinaryOperator &binop, bool allowSideEffects);
};
}  // namespace C
