#pragma once

#include "DMZPCH.hpp"
#include "fmt/FormatterSymbols.hpp"

namespace DMZ {
namespace fmt {
class Builder {
    int id = 0;

   public:
    Builder() {};

    int new_id() {
        id++;
        return id;
    }
    ref<Node> string(std::string_view str);
    ref<Node> comma_separated_list(std::string_view start_list, std::string_view end_list,
                                   const vec<ref<Node>>& arguments);
    ref<Node> call(ref<Node> callee, std::vector<ref<Node>> arguments);
};
}  // namespace fmt
}  // namespace DMZ