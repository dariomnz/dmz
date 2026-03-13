#pragma once

#include "DMZPCH.hpp"

#include "DMZPCHSymbols.hpp"

namespace DMZ {

class ConstantExpressionEvaluator {
   public:
    static std::optional<bool> to_bool(std::optional<int> &d);
    std::optional<int> evaluate(const ResolvedExpr &expr, bool allowSideEffects = false);
    std::optional<int> evaluate(const ResolvedTypeofExpr &expr, bool allowSideEffects = false);
    std::optional<int> evaluate(const ResolvedTypeExpr &expr, bool allowSideEffects = false);
    std::optional<int> evaluate_type(const ResolvedType &type);
    std::optional<int> evaluate_unary_operator(const ResolvedUnaryOperator &unop, bool allowSideEffects);
    std::optional<int> evaluate_binary_operator(const ResolvedBinaryOperator &binop, bool allowSideEffects);
    std::optional<int> evaluate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool allowSideEffects);

    std::optional<int> evaluate_decl(const ResolvedDecl &decl, bool allowSideEffects = false);
};
}  // namespace DMZ
