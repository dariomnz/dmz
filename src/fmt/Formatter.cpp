#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ref<Node> Formatter::fmt_ast(const ModuleDecl& modDecl) {
    debug_func("");
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

    return makeRef<Nodes>(vec<ref<Node>>{
        makeRef<Text>("{"),
        makeRef<Line>(),
        makeRef<Indent>(std::move(stmtList)),
        makeRef<Text>("}"),
    });
}

ref<Node> Formatter::fmt_case_block(const Block& block) {
    debug_func("");

    auto ret = makeRef<Group>(build.new_id(), vec<ref<Node>>{});
    vec<ref<Node>>* stmts = &ret->nodes;
    if (block.statements.size() != 1) {
        ret->nodes.emplace_back(makeRef<Text>("{"));
        if (block.statements.size() != 0) {
            ret->nodes.emplace_back(makeRef<Line>());
            auto indt = makeRef<Indent>(vec<ref<Node>>{});
            stmts = &indt->nodes;
            ret->nodes.emplace_back(std::move(indt));
        }
    }

    for (auto&& stmt : block.statements) {
        stmts->emplace_back(fmt_stmt(*stmt));
    }

    if (block.statements.size() != 1) {
        ret->nodes.emplace_back(makeRef<Text>("}"));
        ret->nodes.emplace_back(makeRef<Line>());
    }
    return ret;
}
}  // namespace fmt
}  // namespace DMZ