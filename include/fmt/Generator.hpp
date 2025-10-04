#pragma once

#include "DMZPCH.hpp"
#include "fmt/FormatterSymbols.hpp"

namespace DMZ {
namespace fmt {

enum class Wrap {
    Enable,
    Detect,
};

constexpr static std::string INDENT = "    ";

class Generator {
    std::stringstream buffer = {};
    int indent = 0;
    int size = 0;
    int max_line = 0;
    bool in_new_line = true;
    std::unordered_set<int> wrapped = {};

   public:
    Generator(int max) : max_line(max) {}

    void generate(const Node& node);
    void generate(const Node& node, Wrap& wrap);

    void add_indent();
    void sub_indent();
    void text(std::string_view value);
    void new_line();

    void print();
};
}  // namespace fmt
}  // namespace DMZ