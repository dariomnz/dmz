#pragma once

#include "semantic/ComptimeValue.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

class Sema;  // forward declaration
class ConstantExpressionEvaluator {
   public:
    Sema *m_sema;
    static std::optional<bool> to_bool(const std::optional<ComptimeValue> &d);
    bool should_report_error() const;
    std::optional<ComptimeValue> evaluate(const ResolvedExpr &expr, bool allowSideEffects = false);
    std::optional<ComptimeValue> evaluate_call_expr(const ResolvedCallExpr &expr, bool allowSideEffects = false);
    std::optional<ComptimeValue> evaluate_type(const ResolvedType &type);
    std::optional<ComptimeValue> evaluate_unary_operator(const ResolvedUnaryOperator &unop, bool allowSideEffects);
    std::optional<ComptimeValue> evaluate_binary_operator(const ResolvedBinaryOperator &binop, bool allowSideEffects);
    std::optional<ComptimeValue> evaluate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool allowSideEffects);

    std::optional<ComptimeValue> evaluate_decl(const ResolvedDecl &decl, bool allowSideEffects = false);
    std::optional<ComptimeValue> evaluate_block(const ResolvedBlock &block, bool allowSideEffects = false);
    std::optional<ComptimeValue> evaluate_stmt(const ResolvedStmt &stmt, bool allowSideEffects = false);

   private:
    struct CallCacheEntry {
        const ResolvedDecl *func;
        std::vector<ComptimeValue> args;
        ComptimeValue result;
    };
    std::unordered_map<const ResolvedDecl *, CallCacheEntry> m_callCache;
    std::unordered_map<const ResolvedDecl *, ComptimeValue> m_env;
    std::optional<ComptimeValue> m_returnValue;
    bool m_shouldReturn = false;
    bool m_shouldBreak = false;
    bool m_shouldContinue = false;

    size_t m_depth = 0;
    static constexpr size_t MAX_RECURSION_DEPTH = 1000;
};
}  // namespace DMZ
