#include "fmt/Builder.hpp"

namespace DMZ {
namespace fmt {

ref<Node> Builder::string(std::string_view str) {
    return makeRef<Group>(new_id(), vec<ref<Node>>{
                                        makeRef<Text>("\""),
                                        makeRef<Text>(str),
                                        makeRef<Text>("\""),
                                    });
}

ref<Node> Builder::comma_separated_list(std::string_view start_list, std::string_view end_list,
                                        const vec<ref<Node>>& arguments) {
    debug_func("");
    if (arguments.empty()) {
        return makeRef<Text>(std::string(start_list) + std::string(end_list));
    }
    auto id = new_id();
    auto max = arguments.size() - 1;
    vec<ref<Node>> vals;
    for (size_t i = 0; i < arguments.size(); i++) {
        auto& arg = arguments[i];
        if (i < max) {
            vals.emplace_back(makeRef<Nodes>(vec<ref<Node>>{arg, makeRef<Text>(","), makeRef<SpaceOrLineIfWrap>()}));
        } else {
            vals.emplace_back(
                makeRef<Nodes>(vec<ref<Node>>{arg, makeRef<IfWrap>(id, makeRef<Text>(","), makeRef<Text>(""))}));
        }
    }

    return makeRef<Group>(id, vec<ref<Node>>{
                                  makeRef<Text>(start_list),
                                  makeRef<LineIfWrap>(),
                                  makeRef<IndentIfWrap>(std::move(vals)),
                                  makeRef<LineIfWrap>(),
                                  makeRef<Text>(end_list),
                              });
}

ref<Node> Builder::call(ref<Node> callee, vec<ref<Node>> arguments) {
    debug_func("");
    auto id = new_id();
    if (arguments.empty()) {
        return makeRef<Group>(id, vec<ref<Node>>{callee, makeRef<Text>("()")});
    }
    auto max = arguments.size() - 1;
    vec<ref<Node>> vals;
    for (size_t i = 0; i < arguments.size(); i++) {
        auto& arg = arguments[i];
        if (i < max) {
            vals.emplace_back(makeRef<Nodes>(vec<ref<Node>>{arg, makeRef<Text>(","), makeRef<SpaceOrLineIfWrap>()}));
        } else {
            vals.emplace_back(
                makeRef<Nodes>(vec<ref<Node>>{arg, makeRef<IfWrap>(id, makeRef<Text>(","), makeRef<Text>(""))}));
        }
    }

    return makeRef<Group>(id, vec<ref<Node>>{
                                  callee,
                                  makeRef<Group>(new_id(),
                                                 vec<ref<Node>>{
                                                     makeRef<Text>("("),
                                                     makeRef<LineIfWrap>(),
                                                     makeRef<IndentIfWrap>(std::move(vals)),
                                                     makeRef<LineIfWrap>(),
                                                     makeRef<Text>(")"),
                                                 }),
                              });
}
}  // namespace fmt
}  // namespace DMZ