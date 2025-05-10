#pragma once

#include "AST.hpp"
#include "PH.hpp"

namespace C {

class ConstantExpressionEvaluator {
   public:
    static bool to_bool(std::optional<ConstValue> d);
    std::optional<ConstValue> evaluate(const ResolvedExpr &expr, bool allowSideEffects = false);
    std::optional<ConstValue> evaluate_unary_operator(const ResolvedUnaryOperator &unop, bool allowSideEffects);
    std::optional<ConstValue> evaluate_binary_operator(const ResolvedBinaryOperator &binop, bool allowSideEffects);
    std::optional<ConstValue> evaluate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool allowSideEffects);
};
}  // namespace C
