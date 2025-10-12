#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ptr<Node> Formatter::fmt_decl(const Decl& decl) {
    debug_func("");

    if (auto cast_decl = dynamic_cast<const ModuleDecl*>(&decl)) {
        return fmt_module_decl(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const Decoration*>(&decl)) {
        return fmt_decoration(*cast_decl);
    } else if (auto cast_decl = dynamic_cast<const FuncDecl*>(&decl)) {
        return fmt_func_decl(*cast_decl);
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
    } else if (auto cast_decl = dynamic_cast<const GenericTypeDecl*>(&decl)) {
        return makePtr<Text>(cast_decl->identifier);
    } else if (auto cast_decl = dynamic_cast<const CaptureDecl*>(&decl)) {
        return makePtr<Text>(cast_decl->identifier);
    } else {
        decl.dump();
        dmz_unreachable("TODO");
    }
}

ptr<Node> Formatter::fmt_module_decl(const ModuleDecl& modDecl) {
    debug_func("");
    auto nodes = makePtr<Nodes>(vec<ptr<Node>>{});

    for (size_t i = 0; i < modDecl.declarations.size(); i++) {
        // Ignore new_lines
        // if (dynamic_cast<const EmptyLine*>(modDecl.declarations[i].get())) continue;

        nodes->nodes.emplace_back(fmt_decl(*modDecl.declarations[i]));
        // Extra line for functions
        if (i < modDecl.declarations.size() - 1) {
            nodes->nodes.emplace_back(makePtr<Line>());
        }
    }

    return nodes;
}

ptr<Node> Formatter::fmt_func_decl(const FuncDecl& fnDecl) {
    debug_func("");

    auto ret = makePtr<Group>(build.new_id(), vec<ptr<Node>>{});
    if (auto test = dynamic_cast<const TestDecl*>(&fnDecl)) {
        ret->nodes.emplace_back(makePtr<Text>("test"));
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(build.string(test->identifier));
    } else {
        vec<ptr<Node>> paramList;

        for (auto&& param : fnDecl.params) {
            paramList.emplace_back(fmt_decl(*param));
        }

        if (fnDecl.isPublic) {
            ret->nodes.emplace_back(makePtr<Text>("pub"));
            ret->nodes.emplace_back(makePtr<Space>());
        }
        if (dynamic_cast<const ExternFunctionDecl*>(&fnDecl)) {
            ret->nodes.emplace_back(makePtr<Text>("extern"));
            ret->nodes.emplace_back(makePtr<Space>());
        }
        ret->nodes.emplace_back(makePtr<Text>("fn"));
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(makePtr<Text>(fnDecl.identifier));
        if (auto generic = dynamic_cast<const GenericFunctionDecl*>(&fnDecl)) {
            vec<ptr<Node>> gens;
            for (auto&& gen : generic->genericTypes) {
                gens.emplace_back(fmt_decl(*gen));
            }
            ret->nodes.emplace_back(build.comma_separated_list("<", ">", std::move(gens)));
        }
        ret->nodes.emplace_back(build.comma_separated_list("(", ")", std::move(paramList)));
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(makePtr<Text>("->"));
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(fmt_expr(*fnDecl.type));
    }
    if (auto functionDecl = dynamic_cast<const FunctionDecl*>(&fnDecl)) {
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(fmt_block(*functionDecl->body, false));
    } else if (dynamic_cast<const ExternFunctionDecl*>(&fnDecl)) {
        ret->nodes.emplace_back(makePtr<Text>(";"));
    }

    return ret;
}

ptr<Node> Formatter::fmt_struct_decl(const StructDecl& decl) {
    debug_func("");

    vec<ptr<Node>> nodes;
    for (size_t i = 0; i < decl.decls.size(); i++) {
        nodes.emplace_back(fmt_decl(*decl.decls[i]));
        nodes.emplace_back(makePtr<Line>());
    }

    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    if (decl.isPublic) {
        ret->nodes.emplace_back(makePtr<Text>("pub"));
        ret->nodes.emplace_back(makePtr<Space>());
    }
    if (decl.isPacked) {
        ret->nodes.emplace_back(makePtr<Text>("packed"));
        ret->nodes.emplace_back(makePtr<Space>());
    }
    ret->nodes.emplace_back(makePtr<Text>("struct"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>(decl.identifier));
    if (auto generic = dynamic_cast<const GenericStructDecl*>(&decl)) {
        vec<ptr<Node>> gens;
        for (auto&& gen : generic->genericTypes) {
            gens.emplace_back(fmt_decl(*gen));
        }
        ret->nodes.emplace_back(build.comma_separated_list("<", ">", std::move(gens)));
    }
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>("{"));
    ret->nodes.emplace_back(makePtr<Line>());
    ret->nodes.emplace_back(makePtr<Indent>(std::move(nodes)));
    ret->nodes.emplace_back(makePtr<Text>("}"));
    return ret;
}

ptr<Node> Formatter::fmt_field_decl(const FieldDecl& decl) {
    debug_func("");

    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>(decl.identifier));
    ret->nodes.emplace_back(makePtr<Text>(":"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_expr(*decl.type));
    ret->nodes.emplace_back(makePtr<Text>(","));
    return ret;
}

ptr<Node> Formatter::fmt_param_decl(const ParamDecl& decl) {
    debug_func("");

    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>(decl.identifier));
    if (!decl.isVararg) {
        ret->nodes.emplace_back(makePtr<Text>(":"));
        ret->nodes.emplace_back(makePtr<Space>());
        ret->nodes.emplace_back(fmt_expr(*decl.type));
    }
    return ret;
}

ptr<Node> Formatter::fmt_error_decl(const ErrorDecl& decl) {
    debug_func("");
    return makePtr<Text>(decl.identifier);
}
}  // namespace fmt
}  // namespace DMZ