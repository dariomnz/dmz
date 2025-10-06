#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ref<Node> Formatter::fmt_decl(const Decl& decl) {
    debug_func("");

    if (auto cast_decl = dynamic_cast<const ModuleDecl*>(&decl)) {
        return fmt_module_decl(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const Decoration*>(&decl)) {
        return fmt_decoration(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const FunctionDecl*>(&decl)) {
        return fmt_function_decl(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const ExternFunctionDecl*>(&decl)) {
        return fmt_extern_function_decl(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const StructDecl*>(&decl)) {
        return fmt_struct_decl(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const FieldDecl*>(&decl)) {
        return fmt_field_decl(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const ParamDecl*>(&decl)) {
        return fmt_param_decl(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const DeclStmt*>(&decl)) {
        return fmt_stmt(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const ErrorDecl*>(&decl)) {
        return fmt_error_decl(*cast_decl);
    }

    decl.dump();
    dmz_unreachable("TODO");
}

ref<Node> Formatter::fmt_module_decl(const ModuleDecl& modDecl) {
    debug_func("");
    ref<Nodes> nodes = makeRef<Nodes>(vec<ref<Node>>{});

    for (auto&& decl : modDecl.declarations) {
        // Ignore new_lines
        if (dynamic_cast<const EmptyLine*>(decl.get())) continue;

        auto node = fmt_decl(*decl);
        nodes->nodes.emplace_back(node);
    }

    return nodes;
}

ref<Node> Formatter::fmt_function_decl(const FunctionDecl& fnDecl) {
    debug_func("");

    vec<ref<Node>> paramList;

    for (auto&& param : fnDecl.params) {
        paramList.emplace_back(fmt_decl(*param));
    }

    auto returnType = fmt_expr(*fnDecl.type);

    return makeRef<Group>(build.new_id(), vec<ref<Node>>{
                                              makeRef<Line>(),
                                              makeRef<Text>("fn"),
                                              makeRef<Space>(),
                                              makeRef<Text>(fnDecl.identifier),
                                              build.comma_separated_list("(", ")", paramList),
                                              makeRef<Space>(),
                                              makeRef<Text>("->"),
                                              makeRef<Space>(),
                                              std::move(returnType),
                                              makeRef<Space>(),
                                              fmt_block(*fnDecl.body),
                                              makeRef<Line>(),
                                          });
}

ref<Node> Formatter::fmt_extern_function_decl(const ExternFunctionDecl& fnDecl) {
    debug_func("");

    vec<ref<Node>> paramList;

    for (auto&& param : fnDecl.params) {
        paramList.emplace_back(fmt_decl(*param));
    }

    auto returnType = fmt_expr(*fnDecl.type);

    return makeRef<Group>(build.new_id(), vec<ref<Node>>{
                                              makeRef<Line>(),
                                              makeRef<Text>("extern"),
                                              makeRef<Space>(),
                                              makeRef<Text>("fn"),
                                              makeRef<Space>(),
                                              makeRef<Text>(fnDecl.identifier),
                                              build.comma_separated_list("(", ")", paramList),
                                              makeRef<Space>(),
                                              makeRef<Text>("->"),
                                              makeRef<Space>(),
                                              std::move(returnType),
                                              makeRef<Text>(";"),
                                              makeRef<Line>(),
                                          });
}

ref<Node> Formatter::fmt_struct_decl(const StructDecl& decl) {
    debug_func("");

    vec<ref<Node>> nodes;
    for (auto&& d : decl.decls) {
        nodes.emplace_back(fmt_decl(*d));
    }

    return makeRef<Nodes>(vec<ref<Node>>{
        makeRef<Line>(),
        makeRef<Text>("struct"),
        makeRef<Space>(),
        makeRef<Text>(decl.identifier),
        makeRef<Space>(),
        makeRef<Text>("{"),
        makeRef<Line>(),
        makeRef<Indent>(std::move(nodes)),
        makeRef<Text>("}"),
        makeRef<Line>(),
    });
}

ref<Node> Formatter::fmt_field_decl(const FieldDecl& decl) {
    debug_func("");

    auto type = fmt_expr(*decl.type);

    return makeRef<Nodes>(vec<ref<Node>>{
        makeRef<Text>(decl.identifier),
        makeRef<Text>(":"),
        makeRef<Space>(),
        std::move(type),
        makeRef<Text>(","),
        makeRef<Line>(),
    });
}

ref<Node> Formatter::fmt_param_decl(const ParamDecl& decl) {
    debug_func("");

    auto type = fmt_expr(*decl.type);

    return makeRef<Nodes>(vec<ref<Node>>{
        makeRef<Text>(decl.identifier),
        makeRef<Text>(":"),
        makeRef<Space>(),
        std::move(type),
    });
}

ref<Node> Formatter::fmt_error_decl(const ErrorDecl& decl) {
    debug_func("");
    return makeRef<Text>(decl.identifier);
}
}  // namespace fmt
}  // namespace DMZ