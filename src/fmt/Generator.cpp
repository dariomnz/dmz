#include "fmt/Generator.hpp"

namespace DMZ {
namespace fmt {

void Generator::generate(const Node& n) {
    debug_func("");
    Wrap w = Wrap::Detect;
    generate(n, w);
}

void Generator::generate(const Node& node, Wrap& wrap) {
    debug_func("indent " << indent);
    if (auto nodes = dynamic_cast<const Nodes*>(&node)) {
        debug_msg("Nodes");
        for (auto&& n : nodes->nodes) {
            generate(*n, wrap);
        }
    } else if (auto group = dynamic_cast<const Group*>(&node)) {
        debug_msg("Group");
        int width = group->width(wrapped);
        Wrap w;
        if (size + width > max_line) {
            wrapped.insert(group->group_id);
            w = Wrap::Enable;
        } else {
            w = Wrap::Detect;
        }
        for (auto&& n : group->nodes) {
            generate(*n, w);
        }
    } else if (auto ind = dynamic_cast<const Indent*>(&node)) {
        debug_msg("Indent");
        size += INDENT.size();
        indent += 1;
        buffer << INDENT;

        for (auto&& n : ind->nodes) {
            generate(*n, wrap);
        }
        indent -= 1;
    } else if (auto ind = dynamic_cast<const IndentIfWrap*>(&node)) {
        debug_msg("IndentIfWrap");
        bool wasWrapped = false;
        if (wrap == Wrap::Enable) {
            size += INDENT.size();
            indent += 1;
            buffer << INDENT;
            wasWrapped = true;
        }

        for (auto&& n : ind->nodes) {
            generate(*n, wrap);
        }
        if (wasWrapped) {
            indent -= 1;
        }
    } else if (auto ifwrap = dynamic_cast<const IfWrap*>(&node)) {
        debug_msg("IfWrap");
        if (wrapped.contains(ifwrap->group_id)) {
            Wrap w = Wrap::Enable;
            generate(*ifwrap->node1, w);
        } else {
            generate(*ifwrap->node2, wrap);
        }
    } else if (auto t = dynamic_cast<const Text*>(&node)) {
        debug_msg("Text");
        text(t->text);
    } else if (dynamic_cast<const Line*>(&node)) {
        debug_msg("Line");
        new_line();
    } else if (dynamic_cast<const LineIfWrap*>(&node)) {
        debug_msg("LineIfWrap");
        if (wrap == Wrap::Enable) {
            new_line();
        }
    } else if (dynamic_cast<const SpaceOrLineIfWrap*>(&node)) {
        debug_msg("SpaceOrLineIfWrap");
        if (wrap == Wrap::Enable) {
            new_line();
        } else {
            text(" ");
        }
    } else if (dynamic_cast<const Space*>(&node)) {
        debug_msg("Space");
        text(" ");
    } else {
        node.dump();
        dmz_unreachable("TODO");
    }
}

void Generator::text(std::string_view value) {
    debug_func(value);
    size += value.size();
    buffer << value;
}

void Generator::new_line() {
    debug_func("indent " << indent);
    size = INDENT.size() * indent;
    buffer << '\n';
    for (int i = 0; i < indent; i++) {
        buffer << INDENT;
    }
}
void Generator::print() {
    debug_func("buffer size " << buffer.str().size());

    std::cout << buffer.str() << std::endl;
}
}  // namespace fmt
}  // namespace DMZ