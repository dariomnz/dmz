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
    ptr<Node> string(std::string_view str);
    ptr<Node> comma_separated_list(std::string_view start_list, std::string_view end_list, vec<ptr<Node>> arguments);
    ptr<Node> call(ptr<Node> callee, vec<ptr<Node>> arguments);
};
}  // namespace fmt
}  // namespace DMZ