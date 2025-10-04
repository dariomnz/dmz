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
    SpaceOrLineIfWrap() : Node() {}

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
    LineIfWrap() : Node() {}

    void dump(size_t level = 0) const override;
};

struct IndentIfWrap : public Node {
    vec<ref<Node>> nodes;
    IndentIfWrap(vec<ref<Node>> nodes) : Node(), nodes(std::move(nodes)) {}

    void dump(size_t level = 0) const override;
};

struct Indent : public Node {
    vec<ref<Node>> nodes;
    Indent(vec<ref<Node>> nodes) : Node(), nodes(std::move(nodes)) {}

    void dump(size_t level = 0) const override;
};

struct Nodes : public Node {
    vec<ref<Node>> nodes;
    Nodes(vec<ref<Node>> nodes) : Node(), nodes(std::move(nodes)) {}

    void dump(size_t level = 0) const override;
};

struct Group : public Node {
    int group_id;
    vec<ref<Node>> nodes;
    Group(int group_id, vec<ref<Node>> nodes) : Node(), group_id(group_id), nodes(std::move(nodes)) {}

    void dump(size_t level = 0) const override;
};

struct IfWrap : public Node {
    int group_id;
    ref<Node> node1;
    ref<Node> node2;
    IfWrap(int group_id, ref<Node> node1, ref<Node> node2)
        : Node(), group_id(group_id), node1(std::move(node1)), node2(std::move(node2)) {}

    void dump(size_t level = 0) const override;
};
}  // namespace fmt
}  // namespace DMZ