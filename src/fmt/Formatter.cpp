#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ptr<Node> Formatter::fmt_ast(const ModuleDecl& modDecl) {
    debug_func("");
    return fmt_decl(modDecl);
}

ptr<Node> Formatter::fmt_decoration(const Decoration& decl) {
    debug_func("");

    if (auto comment = dynamic_cast<const Comment*>(&decl)) {
        return fmt_comment(*comment);
    } else if (auto emptyLine = dynamic_cast<const EmptyLine*>(&decl)) {
        return fmt_empty_line(*emptyLine);
    }

    decl.dump();
    dmz_unreachable("TODO");
}

ptr<Node> Formatter::fmt_comment(const Comment& decl) {
    debug_func("");
    return makePtr<Text>(decl.comment);
}

ptr<Node> Formatter::fmt_empty_line([[maybe_unused]] const EmptyLine& decl) {
    debug_func("");
    return makePtr<Line>();
}

ptr<Node> Formatter::fmt_block(const Block& block) {
    debug_func("");
    auto ret = makePtr<Group>(build.new_id(), vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("{"));
    ret->nodes.emplace_back(makePtr<Line>());

    auto& _stmtList = ret->nodes.emplace_back(makePtr<Indent>(vec<ptr<Node>>{}));
    auto stmtList = static_cast<Indent*>(_stmtList.get());
    for (size_t i = 0; i < block.statements.size(); i++) {
        stmtList->nodes.emplace_back(fmt_stmt(*block.statements[i]));
        stmtList->nodes.emplace_back(makePtr<Line>());
    }
    ret->nodes.emplace_back(makePtr<Text>("}"));
    return ret;
}

ptr<Node> Formatter::fmt_case_block(const Block& block) {
    debug_func("");

    auto ret = makePtr<Group>(build.new_id(), vec<ptr<Node>>{});
    vec<ptr<Node>>* stmts = &ret->nodes;
    if (block.statements.size() != 1) {
        ret->nodes.emplace_back(makePtr<Text>("{"));
        if (block.statements.size() != 0) {
            ret->nodes.emplace_back(makePtr<Line>());
            auto indt = makePtr<Indent>(vec<ptr<Node>>{});
            stmts = &indt->nodes;
            ret->nodes.emplace_back(std::move(indt));
        }
    }

    for (size_t i = 0; i < block.statements.size(); i++) {
        stmts->emplace_back(fmt_stmt(*block.statements[i]));
        if (i < block.statements.size() - 1) {
            stmts->emplace_back(makePtr<Line>());
        }
    }

    if (block.statements.size() != 1) {
        ret->nodes.emplace_back(makePtr<Text>("}"));
        ret->nodes.emplace_back(makePtr<Line>());
    }
    return ret;
}
}  // namespace fmt
}  // namespace DMZ