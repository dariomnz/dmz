#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ref<Node> Formatter::fmt_ast(const ModuleDecl& modDecl) {
    debug_func("");
    // std::vector<ptr<Expr>> args;
    // args.emplace_back(makePtr<IntLiteral>(SourceLocation{}, "100000000000000"));
    // auto call = makePtr<CallExpr>(SourceLocation{}, makePtr<DeclRefExpr>(SourceLocation{}, "foo"), std::move(args));
    return fmt_decl(modDecl);
}

ref<Node> Formatter::fmt_decoration(const Decoration& decl) {
    debug_func("");

    if (auto comment = dynamic_cast<const Comment*>(&decl)) {
        return fmt_comment(*comment);
    } else if (auto emptyLine = dynamic_cast<const EmptyLine*>(&decl)) {
        return fmt_empty_line(*emptyLine);
    }

    decl.dump();
    dmz_unreachable("TODO");
}

ref<Node> Formatter::fmt_comment(const Comment& decl) {
    debug_func("");
    return makeRef<Nodes>(vec<ref<Node>>{makeRef<Text>(decl.comment), makeRef<Line>()});
}

ref<Node> Formatter::fmt_empty_line([[maybe_unused]] const EmptyLine& decl) {
    debug_func("");
    return makeRef<Line>();
}

ref<Node> Formatter::fmt_block(const Block& block) {
    debug_func("");
    vec<ref<Node>> stmtList;

    for (auto&& stmt : block.statements) {
        stmtList.emplace_back(fmt_stmt(*stmt));
    }

    return makeRef<Nodes>(
        vec<ref<Node>>{makeRef<Text>("{"), makeRef<Line>(), makeRef<Indent>(std::move(stmtList)), makeRef<Text>("}")});
}
}  // namespace fmt
}  // namespace DMZ