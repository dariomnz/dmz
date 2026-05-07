#pragma once

#include "semantic/ComptimeValue.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

class ConstantExpressionEvaluator {
   public:
    static std::optional<bool> to_bool(const std::optional<ComptimeValue> &d);
    std::optional<ComptimeValue> evaluate(const ResolvedExpr &expr, bool allowSideEffects = false);
    std::optional<ComptimeValue> evaluate_call_expr(const ResolvedCallExpr &expr, bool allowSideEffects = false);
    std::optional<ComptimeValue> evaluate_type(const ResolvedType &type);
    std::optional<ComptimeValue> evaluate_unary_operator(const ResolvedUnaryOperator &unop, bool allowSideEffects);
    std::optional<ComptimeValue> evaluate_binary_operator(const ResolvedBinaryOperator &binop, bool allowSideEffects);
    std::optional<ComptimeValue> evaluate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool allowSideEffects);

    std::optional<ComptimeValue> evaluate_decl(const ResolvedDecl &decl, bool allowSideEffects = false);

   private:
    size_t m_depth = 0;
    static constexpr size_t MAX_RECURSION_DEPTH = 1000;
};
}  // namespace DMZ
