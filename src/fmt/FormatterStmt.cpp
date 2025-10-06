#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ref<Node> Formatter::fmt_stmt(const Stmt& stmt) {
    debug_func("");
    bool needSemicolon = true;
    bool needNewLine = true;
    ref<Node> node = nullptr;

    if (auto cast_expr = dynamic_cast<const Decoration*>(&stmt)) {
        node = fmt_decoration(*cast_expr);
        needSemicolon = false;
        needNewLine = false;
    } else if (auto cast_stmt = dynamic_cast<const Expr*>(&stmt)) {
        node = fmt_expr(*cast_stmt);
    } else if (auto cast_stmt = dynamic_cast<const DeclStmt*>(&stmt)) {
        node = fmt_decl_stmt(*cast_stmt);
    } else if (auto cast_stmt = dynamic_cast<const Assignment*>(&stmt)) {
        node = fmt_assignment_stmt(*cast_stmt);
    } else if (auto cast_stmt = dynamic_cast<const ReturnStmt*>(&stmt)) {
        node = fmt_return_stmt(*cast_stmt);
    } else {
        stmt.dump();
        dmz_unreachable("TODO");
    }

    if (needSemicolon && !needNewLine) {
        node = makeRef<Nodes>(vec<ref<Node>>{std::move(node), makeRef<Text>(";")});
    } else if (!needSemicolon && needNewLine) {
        node = makeRef<Nodes>(vec<ref<Node>>{std::move(node), makeRef<Line>()});
    } else if (needSemicolon && needNewLine) {
        node = makeRef<Nodes>(vec<ref<Node>>{std::move(node), makeRef<Text>(";"), makeRef<Line>()});
    }

    return node;
}

ref<Node> Formatter::fmt_decl_stmt(const DeclStmt& stmt) {
    auto let_const = makeRef<Text>(stmt.varDecl->isMutable ? "let" : "const");
    auto name = makeRef<Text>(stmt.varDecl->identifier);

    if (stmt.varDecl->initializer) {
        return makeRef<Nodes>(vec<ref<Node>>{std::move(let_const), makeRef<Space>(), std::move(name), makeRef<Space>(),
                                             makeRef<Text>("="), makeRef<Space>(),
                                             fmt_expr(*stmt.varDecl->initializer)});
    } else {
        return makeRef<Nodes>(vec<ref<Node>>{std::move(let_const), makeRef<Space>(), std::move(name)});
    }
}

ref<Node> Formatter::fmt_assignment_stmt(const Assignment& stmt) {
    auto assignee = fmt_expr(*stmt.assignee);
    auto expr = fmt_expr(*stmt.expr);

    std::string op = "=";
    if (auto asOp = dynamic_cast<const AssignmentOperator*>(&stmt)) {
        op = get_op_str(asOp->op);
    }

    return makeRef<Nodes>(
        vec<ref<Node>>{std::move(assignee), makeRef<Space>(), makeRef<Text>(op), makeRef<Space>(), std::move(expr)});
}

ref<Node> Formatter::fmt_return_stmt(const ReturnStmt& stmt) {
    auto expr = fmt_expr(*stmt.expr);
    return makeRef<Nodes>(vec<ref<Node>>{makeRef<Text>("return"), makeRef<Space>(), std::move(expr)});
}
}  // namespace fmt
}  // namespace DMZ