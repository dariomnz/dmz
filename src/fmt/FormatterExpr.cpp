#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ref<Node> Formatter::fmt_expr(const Expr& expr) {
    debug_func("");
    if (auto cast_expr = dynamic_cast<const Decoration*>(&expr)) {
        return fmt_decoration(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const Type*>(&expr)) {
        return makeRef<Text>(cast_expr->to_str());
    } else if (auto cast_expr = dynamic_cast<const IntLiteral*>(&expr)) {
        return makeRef<Text>(cast_expr->value);
    } else if (auto cast_expr = dynamic_cast<const StringLiteral*>(&expr)) {
        return makeRef<Text>(cast_expr->value);
    } else if (auto cast_expr = dynamic_cast<const DeclRefExpr*>(&expr)) {
        return fmt_decl_ref_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const CallExpr*>(&expr)) {
        return fmt_call_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const DerefPtrExpr*>(&expr)) {
        return fmt_deref_ptr_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const MemberExpr*>(&expr)) {
        return fmt_member_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const ImportExpr*>(&expr)) {
        return fmt_import_expr(*cast_expr);
    }
    expr.dump();
    dmz_unreachable("TODO");
}

ref<Node> Formatter::fmt_decl_ref_expr(const DeclRefExpr& declRefExpr) {
    debug_func("");
    return makeRef<Text>(declRefExpr.identifier);
}

ref<Node> Formatter::fmt_call_expr(const CallExpr& callExpr) {
    debug_func("");
    auto callee = fmt_expr(*callExpr.callee);
    vec<ref<Node>> args;
    for (auto&& arg : callExpr.arguments) {
        args.emplace_back(fmt_expr(*arg));
    }

    return build.call(callee, args);
}

ref<Node> Formatter::fmt_deref_ptr_expr(const DerefPtrExpr& expr) {
    debug_func("");
    return makeRef<Nodes>(vec<ref<Node>>{makeRef<Text>("*"), fmt_expr(*expr.expr)});
}

ref<Node> Formatter::fmt_member_expr(const MemberExpr& expr) {
    debug_func("");
    return makeRef<Nodes>(vec<ref<Node>>{fmt_expr(*expr.base), makeRef<Text>("."), makeRef<Text>(expr.field)});
}

ref<Node> Formatter::fmt_import_expr(const ImportExpr& expr) {
    debug_func("");
    return makeRef<Nodes>(
        vec<ref<Node>>{makeRef<Text>("import"), makeRef<Text>("("), build.string(expr.identifier), makeRef<Text>(")")});
}
}  // namespace fmt
}  // namespace DMZ