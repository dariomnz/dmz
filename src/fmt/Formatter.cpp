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

ptr<Node> Formatter::fmt_block(const Block& block, bool wantWrap, bool needBracket) {
    debug_func("");
    auto ret = makePtr<Group>(build.new_id(), vec<ptr<Node>>{});
    if (needBracket || block.statements.size() == 0 || block.statements.size() > 1) {
        ret->nodes.emplace_back(makePtr<Text>("{"));
    }
    if (block.statements.size() < 2 && wantWrap) {
        if (needBracket) {
            ret->nodes.emplace_back(makePtr<SpaceOrLineIfWrap>(ret->group_id));
        } else {
            ret->nodes.emplace_back(makePtr<LineIfWrap>(ret->group_id));
        }
    } else {
        ret->nodes.emplace_back(makePtr<Line>());
    }

    if (block.statements.size() < 2 && wantWrap) {
        auto stmtList = makePtr<IndentIfWrap>(ret->group_id, vec<ptr<Node>>{});
        for (size_t i = 0; i < block.statements.size(); i++) {
            stmtList->nodes.emplace_back(fmt_stmt(*block.statements[i]));
            if (needBracket) {
                stmtList->nodes.emplace_back(makePtr<SpaceOrLineIfWrap>(ret->group_id));
            } else {
                stmtList->nodes.emplace_back(makePtr<LineIfWrap>(ret->group_id));
            }
        }
        ret->nodes.emplace_back(std::move(stmtList));
    } else {
        auto stmtList = makePtr<Indent>(vec<ptr<Node>>{});
        for (size_t i = 0; i < block.statements.size(); i++) {
            stmtList->nodes.emplace_back(fmt_stmt(*block.statements[i]));
            stmtList->nodes.emplace_back(makePtr<Line>());
        }
        ret->nodes.emplace_back(std::move(stmtList));
    }
    if (needBracket || block.statements.size() == 0 || block.statements.size() > 1) {
        ret->nodes.emplace_back(makePtr<Text>("}"));
    }
    return ret;
}
}  // namespace fmt
}  // namespace DMZ