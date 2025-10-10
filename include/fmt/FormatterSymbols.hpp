#pragma once

#include "DMZPCH.hpp"
#include "Debug.hpp"

namespace DMZ {
namespace fmt {

struct Node {
    Node() {}
    virtual ~Node() = default;

    virtual void dump(size_t level = 0) const = 0;

    int width(std::unordered_set<int>& wrapped) const;
};

struct Text : public Node {
    std::string text;
    Text(std::string_view text) : Node(), text(text) {}

    void dump(size_t level = 0) const override;
};

struct SpaceOrLineIfWrap : public Node {
    int group_id;
    SpaceOrLineIfWrap(int group_id) : Node(), group_id(group_id) {}

    void dump(size_t level = 0) const override;
};

struct Space : public Node {
    Space() : Node() {}

    void dump(size_t level = 0) const override;
};

struct Line : public Node {
    Line() : Node() {}

    void dump(size_t level = 0) const override;
};

struct LineIfWrap : public Node {
    int group_id;
    LineIfWrap(int group_id) : Node(), group_id(group_id) {}

    void dump(size_t level = 0) const override;
};

struct IndentIfWrap : public Node {
    int group_id;
    vec<ptr<Node>> nodes;
    IndentIfWrap(int group_id, vec<ptr<Node>> nodes) : Node(), group_id(group_id), nodes(std::move(nodes)) {}

    void dump(size_t level = 0) const override;
};

struct Indent : public Node {
    vec<ptr<Node>> nodes;
    Indent(vec<ptr<Node>> nodes) : Node(), nodes(std::move(nodes)) {}

    void dump(size_t level = 0) const override;
};

struct Nodes : public Node {
    vec<ptr<Node>> nodes;
    Nodes(vec<ptr<Node>> nodes) : Node(), nodes(std::move(nodes)) {}

    void dump(size_t level = 0) const override;
};

struct Group : public Node {
    int group_id;
    vec<ptr<Node>> nodes;
    Group(int group_id, vec<ptr<Node>> nodes) : Node(), group_id(group_id), nodes(std::move(nodes)) {}

    void dump(size_t level = 0) const override;
};

struct IfWrap : public Node {
    int group_id;
    ptr<Node> node1;
    ptr<Node> node2;
    IfWrap(int group_id, ptr<Node> node1, ptr<Node> node2)
        : Node(), group_id(group_id), node1(std::move(node1)), node2(std::move(node2)) {}

    void dump(size_t level = 0) const override;
};
}  // namespace fmt
}  // namespace DMZ