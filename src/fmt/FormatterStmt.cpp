#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ref<Node> Formatter::fmt_stmt(const Stmt& stmt) {
    debug_func("");
    if (auto cast_stmt = dynamic_cast<const Decoration*>(&stmt)) {
        return fmt_decoration(*cast_stmt);
    } else if (auto cast_stmt = dynamic_cast<const DeclStmt*>(&stmt)) {
        return fmt_decl_stmt(*cast_stmt);
    }
    stmt.dump();
    dmz_unreachable("TODO");
}

ref<Node> Formatter::fmt_decl_stmt(const DeclStmt& stmt) {
    auto let_const = makeRef<Text>(stmt.varDecl->isMutable ? "let" : "const");
    auto name = makeRef<Text>(stmt.varDecl->identifier);

    if (stmt.varDecl->initializer) {
        return makeRef<Nodes>(vec<ref<Node>>{std::move(let_const), makeRef<Space>(), std::move(name), makeRef<Space>(),
                                             fmt_expr(*stmt.varDecl->initializer), makeRef<Text>(";"),
                                             makeRef<Line>()});
    } else {
        return makeRef<Nodes>(vec<ref<Node>>{std::move(let_const), makeRef<Space>(), std::move(name),
                                             makeRef<Text>(";"), makeRef<Line>()});
    }
}
}  // namespace fmt
}  // namespace DMZ