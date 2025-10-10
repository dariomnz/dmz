#include "fmt/Builder.hpp"

namespace DMZ {
namespace fmt {

ptr<Node> Builder::string(std::string_view str) {
    auto ret = makePtr<Group>(new_id(), vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("\""));
    ret->nodes.emplace_back(makePtr<Text>(str));
    ret->nodes.emplace_back(makePtr<Text>("\""));
    return ret;
}

ptr<Node> Builder::comma_separated_list(std::string_view start_list, std::string_view end_list,
                                        vec<ptr<Node>> arguments) {
    debug_func("");
    if (arguments.empty()) {
        return makePtr<Text>(std::string(start_list) + std::string(end_list));
    }
    auto id = new_id();
    auto max = arguments.size() - 1;
    vec<ptr<Node>> vals;
    for (size_t i = 0; i < arguments.size(); i++) {
        auto& arg = arguments[i];
        if (i < max) {
            vec<ptr<Node>> nodes;
            nodes.emplace_back(std::move(arg));
            nodes.emplace_back(makePtr<Text>(","));
            nodes.emplace_back(makePtr<SpaceOrLineIfWrap>(id));
            vals.emplace_back(makePtr<Nodes>(std::move(nodes)));
        } else {
            vec<ptr<Node>> nodes;
            nodes.emplace_back(std::move(arg));
            nodes.emplace_back(makePtr<IfWrap>(id, makePtr<Text>(","), makePtr<Text>("")));
            vals.emplace_back(makePtr<Nodes>(std::move(nodes)));
        }
    }

    auto ret = makePtr<Group>(id, vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>(start_list));
    ret->nodes.emplace_back(makePtr<LineIfWrap>(id));
    ret->nodes.emplace_back(makePtr<IndentIfWrap>(id, std::move(vals)));
    ret->nodes.emplace_back(makePtr<LineIfWrap>(id));
    ret->nodes.emplace_back(makePtr<Text>(end_list));

    return ret;
}

ptr<Node> Builder::call(ptr<Node> callee, vec<ptr<Node>> arguments) {
    debug_func("");
    auto id = new_id();
    if (arguments.empty()) {
        auto ret = makePtr<Group>(id, vec<ptr<Node>>{});
        ret->nodes.emplace_back(std::move(callee));
        ret->nodes.emplace_back(makePtr<Text>("()"));
        return ret;
    }
    auto max = arguments.size() - 1;
    vec<ptr<Node>> vals;
    for (size_t i = 0; i < arguments.size(); i++) {
        auto& arg = arguments[i];
        if (i < max) {
            vec<ptr<Node>> nodes;
            nodes.emplace_back(std::move(arg));
            nodes.emplace_back(makePtr<Text>(","));
            nodes.emplace_back(makePtr<SpaceOrLineIfWrap>(id));
            vals.emplace_back(makePtr<Nodes>(std::move(nodes)));
        } else {
            vec<ptr<Node>> nodes;
            nodes.emplace_back(std::move(arg));
            nodes.emplace_back(makePtr<IfWrap>(id, makePtr<Text>(","), makePtr<Text>("")));
            vals.emplace_back(makePtr<Nodes>(std::move(nodes)));
        }
    }

    auto ret = makePtr<Group>(id, vec<ptr<Node>>{});
    ret->nodes.emplace_back(std::move(callee));
    auto innerGroup_id = new_id();
    auto innerGroup = makePtr<Group>(innerGroup_id, vec<ptr<Node>>{});
    innerGroup->nodes.emplace_back(makePtr<Text>("("));
    innerGroup->nodes.emplace_back(makePtr<LineIfWrap>(innerGroup_id));
    innerGroup->nodes.emplace_back(makePtr<IndentIfWrap>(innerGroup_id, std::move(vals)));
    innerGroup->nodes.emplace_back(makePtr<LineIfWrap>(innerGroup_id));
    innerGroup->nodes.emplace_back(makePtr<Text>(")"));

    ret->nodes.emplace_back(std::move(innerGroup));

    return ret;
}
}  // namespace fmt
}  // namespace DMZ