#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ref<Node> Formatter::fmt_expr(const Expr& expr) {
    debug_func("");
    if (auto type = dynamic_cast<const Type*>(&expr)) {
        return makeRef<Text>(type->to_str());
    } else if (auto literal = dynamic_cast<const IntLiteral*>(&expr)) {
        return makeRef<Text>(literal->value);
    } else if (auto declRef = dynamic_cast<const DeclRefExpr*>(&expr)) {
        return fmt_decl_ref_expr(*declRef);
    } else if (auto callExpr = dynamic_cast<const CallExpr*>(&expr)) {
        return fmt_call_expr(*callExpr);
    } else {
        expr.dump();
        dmz_unreachable("TODO");
    }
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
}  // namespace fmt
}  // namespace DMZ