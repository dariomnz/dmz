#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ptr<Node> Formatter::fmt_stmt(const Stmt& stmt) {
    debug_func("");
    bool needSemicolon = true;
    ptr<Node> node = nullptr;

    if (auto cast_expr = dynamic_cast<const Decoration*>(&stmt)) {
        node = fmt_decoration(*cast_expr);
        needSemicolon = false;
    } else if (auto cast_stmt = dynamic_cast<const Expr*>(&stmt)) {
        node = fmt_expr(*cast_stmt);
    } else if (auto cast_stmt = dynamic_cast<const DeclStmt*>(&stmt)) {
        node = fmt_decl_stmt(*cast_stmt);
    } else if (auto cast_stmt = dynamic_cast<const Assignment*>(&stmt)) {
        node = fmt_assignment_stmt(*cast_stmt);
    } else if (auto cast_stmt = dynamic_cast<const ReturnStmt*>(&stmt)) {
        node = fmt_return_stmt(*cast_stmt);
    } else if (auto cast_stmt = dynamic_cast<const SwitchStmt*>(&stmt)) {
        node = fmt_switch_stmt(*cast_stmt);
        needSemicolon = false;
    } else if (auto cast_stmt = dynamic_cast<const CaseStmt*>(&stmt)) {
        node = fmt_case_stmt(*cast_stmt);
        needSemicolon = false;
    } else if (auto cast_stmt = dynamic_cast<const WhileStmt*>(&stmt)) {
        node = fmt_while_stmt(*cast_stmt);
        needSemicolon = false;
    } else if (auto cast_stmt = dynamic_cast<const ForStmt*>(&stmt)) {
        node = fmt_for_stmt(*cast_stmt);
        needSemicolon = false;
    } else if (auto cast_stmt = dynamic_cast<const IfStmt*>(&stmt)) {
        node = fmt_if_stmt(*cast_stmt);
        needSemicolon = false;
    } else if (auto cast_stmt = dynamic_cast<const FieldInitStmt*>(&stmt)) {
        node = fmt_field_init_stmt(*cast_stmt);
        needSemicolon = false;
    } else if (auto cast_stmt = dynamic_cast<const Block*>(&stmt)) {
        node = fmt_block(*cast_stmt, false);
        needSemicolon = false;
    } else if (auto cast_stmt = dynamic_cast<const DeferStmt*>(&stmt)) {
        node = fmt_defer_stmt(*cast_stmt);
        needSemicolon = false;
    } else {
        stmt.dump();
        dmz_unreachable("TODO");
    }

    auto ret = makePtr<Group>(build.new_id(), vec<ptr<Node>>{});
    ret->nodes.emplace_back(std::move(node));
    if (needSemicolon) {
        ret->nodes.emplace_back(makePtr<Text>(";"));
    }

    return ret;
}

ptr<Node> Formatter::fmt_decl_stmt(const DeclStmt& stmt) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    if (stmt.isPublic) {
        ret->nodes.emplace_back(makePtr<Text>("pub"));
        ret->nodes.emplace_back(makePtr<Space>());
    }
    ret->nodes.emplace_back(makePtr<Text>(stmt.varDecl->isMutable ? "let" : "const"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>(stmt.varDecl->identifier));
    if (stmt.varDecl->type) {
        ret->nodes.emplace_back(makePtr<Text>(":"));
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(fmt_expr(*stmt.varDecl->type));
    }

    if (stmt.varDecl->initializer) {
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(makePtr<Text>("="));
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(fmt_expr(*stmt.varDecl->initializer));
    }
    return ret;
}

ptr<Node> Formatter::fmt_assignment_stmt(const Assignment& stmt) {
    std::string op = "=";
    if (auto asOp = dynamic_cast<const AssignmentOperator*>(&stmt)) {
        op = get_op_str(asOp->op);
    }

    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(fmt_expr(*stmt.assignee));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>(op));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_expr(*stmt.expr));
    return ret;
}

ptr<Node> Formatter::fmt_return_stmt(const ReturnStmt& stmt) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("return"));
    if (stmt.expr) {
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(fmt_expr(*stmt.expr));
    }
    return ret;
}

ptr<Node> Formatter::fmt_switch_stmt(const SwitchStmt& stmt) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("switch"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>("("));
    ret->nodes.emplace_back(fmt_expr(*stmt.condition));
    ret->nodes.emplace_back(makePtr<Text>(")"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>("{"));
    ret->nodes.emplace_back(makePtr<Line>());

    auto indt = makePtr<Indent>(vec<ptr<Node>>{});
    for (size_t i = 0; i < stmt.cases.size(); i++) {
        indt->nodes.emplace_back(fmt_stmt(*stmt.cases[i]));
        indt->nodes.emplace_back(makePtr<Line>());
    }

    if (stmt.elseBlock) {
        auto nodes = makePtr<Nodes>(vec<ptr<Node>>{});
        nodes->nodes.emplace_back(makePtr<Text>("else"));
        nodes->nodes.emplace_back(makePtr<Space>());
        nodes->nodes.emplace_back(makePtr<Text>("=>"));
        nodes->nodes.emplace_back(makePtr<Space>());
        nodes->nodes.emplace_back(fmt_block(*stmt.elseBlock, true, false));

        indt->nodes.emplace_back(std::move(nodes));
        indt->nodes.emplace_back(makePtr<Line>());
    }

    ret->nodes.emplace_back(std::move(indt));

    ret->nodes.emplace_back(makePtr<Text>("}"));

    return ret;
}

ptr<Node> Formatter::fmt_case_stmt(const CaseStmt& stmt) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("case"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_expr(*stmt.condition));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>("=>"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_block(*stmt.block, true, false));
    return ret;
}

ptr<Node> Formatter::fmt_while_stmt(const WhileStmt& stmt) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("while"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>("("));
    auto cond_group = makePtr<Group>(build.new_id(), vec<ptr<Node>>{});
    cond_group->nodes.emplace_back(fmt_expr(*stmt.condition));
    ret->nodes.emplace_back(std::move(cond_group));
    ret->nodes.emplace_back(makePtr<Text>(")"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_block(*stmt.body, false));
    return ret;
}

ptr<Node> Formatter::fmt_for_stmt(const ForStmt& stmt) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("for"));
    ret->nodes.emplace_back(makePtr<Space>());
    vec<ptr<Node>> conditions;
    for (auto&& cond : stmt.conditions) {
        conditions.emplace_back(fmt_expr(*cond));
    }

    ret->nodes.emplace_back(build.comma_separated_list("(", ")", std::move(conditions)));
    ret->nodes.emplace_back(makePtr<Space>());
    vec<ptr<Node>> captures;
    for (auto&& cap : stmt.captures) {
        captures.emplace_back(fmt_decl(*cap));
    }

    ret->nodes.emplace_back(build.comma_separated_list("|", "|", std::move(captures)));
    ret->nodes.emplace_back(makePtr<Space>());

    ret->nodes.emplace_back(fmt_block(*stmt.body, false));
    return ret;
}

ptr<Node> Formatter::fmt_if_stmt(const IfStmt& stmt) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("if"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>("("));
    auto cond_group = makePtr<Group>(build.new_id(), vec<ptr<Node>>{});
    cond_group->nodes.emplace_back(fmt_expr(*stmt.condition));
    ret->nodes.emplace_back(std::move(cond_group));
    ret->nodes.emplace_back(makePtr<Text>(")"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_block(*stmt.trueBlock, false));
    if (stmt.falseBlock) {
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(makePtr<Text>("else"));
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(fmt_block(*stmt.falseBlock, false));
    }
    return ret;
}

ptr<Node> Formatter::fmt_field_init_stmt(const FieldInitStmt& stmt) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>(stmt.identifier));
    ret->nodes.emplace_back(makePtr<Text>(":"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_expr(*stmt.initializer));
    return ret;
}

ptr<Node> Formatter::fmt_defer_stmt(const DeferStmt& stmt) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    if (stmt.isErrDefer) {
        ret->nodes.emplace_back(makePtr<Text>("errdefer"));
    } else {
        ret->nodes.emplace_back(makePtr<Text>("defer"));
    }
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_block(*stmt.block, true, false));
    return ret;
}
}  // namespace fmt
}  // namespace DMZ