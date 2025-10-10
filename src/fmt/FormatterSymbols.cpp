#include "fmt/FormatterSymbols.hpp"

namespace DMZ {
namespace fmt {
int Node::width(std::unordered_set<int>& wrapped) const {
    debug_func("");
    if (auto nodes = dynamic_cast<const Nodes*>(this)) {
        int sum = 0;
        for (auto&& node : nodes->nodes) {
            sum += node->width(wrapped);
        }
        return sum;
    } else if (auto group = dynamic_cast<const Group*>(this)) {
        int sum = 0;
        for (auto&& node : group->nodes) {
            sum += node->width(wrapped);
        }
        return sum;
    } else if (auto ind = dynamic_cast<const IndentIfWrap*>(this)) {
        int sum = 0;
        for (auto&& node : ind->nodes) {
            sum += node->width(wrapped);
        }
        return sum;
    } else if (auto ind = dynamic_cast<const Indent*>(this)) {
        int sum = 0;
        for (auto&& node : ind->nodes) {
            sum += node->width(wrapped);
        }
        return sum;
    } else if (auto ifwrap = dynamic_cast<const IfWrap*>(this)) {
        if (wrapped.contains(ifwrap->group_id)) {
            return ifwrap->node1->width(wrapped);
        } else {
            return ifwrap->node2->width(wrapped);
        }
    } else if (auto text = dynamic_cast<const Text*>(this)) {
        return text->text.size();
    } else if (auto sline = dynamic_cast<const SpaceOrLineIfWrap*>(this)) {
        if (wrapped.contains(sline->group_id)) {
            return 0;
        } else {
            return 1;
        }
    } else if (dynamic_cast<const Space*>(this)) {
        return 1;
    } else if (dynamic_cast<const Line*>(this)) {
        return 0;
    } else if (dynamic_cast<const LineIfWrap*>(this)) {
        return 0;
    } else {
        dump();
        dmz_unreachable("TODO");
    }
}

void Text::dump(size_t level) const { std::cerr << indent(level) << "Text '" << text << "'\n"; }

void SpaceOrLineIfWrap::dump(size_t level) const { std::cerr << indent(level) << "SpaceOrLineIfWrap\n"; }

void Space::dump(size_t level) const { std::cerr << indent(level) << "Space\n"; }

void Line::dump(size_t level) const { std::cerr << indent(level) << "Line\n"; }

void LineIfWrap::dump(size_t level) const { std::cerr << indent(level) << "LineIfWrap\n"; }

void IndentIfWrap::dump(size_t level) const {
    std::cerr << indent(level) << "IndentIfWrap\n";
    for (auto&& node : nodes) {
        node->dump(level + 1);
    }
}

void Indent::dump(size_t level) const {
    std::cerr << indent(level) << "Indent\n";
    for (auto&& node : nodes) {
        node->dump(level + 1);
    }
}

void Nodes::dump(size_t level) const {
    std::cerr << indent(level) << "Nodes\n";
    for (auto&& node : nodes) {
        node->dump(level + 1);
    }
}

void Group::dump(size_t level) const {
    std::unordered_set<int> wrapped;
    std::cerr << indent(level) << "Group " << group_id << " width " << width(wrapped) << "\n";
    for (auto&& node : nodes) {
        node->dump(level + 1);
    }
}

void IfWrap::dump(size_t level) const {
    std::cerr << indent(level) << "IfWrap " << group_id << "\n";
    if (node1) node1->dump(level + 1);
    if (node2) node2->dump(level + 1);
}
}  // namespace fmt
}  // namespace DMZ