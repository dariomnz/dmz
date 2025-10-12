#include "fmt/Formatter.hpp"

namespace DMZ {
namespace fmt {

ptr<Node> Formatter::fmt_expr(const Expr& expr) {
    debug_func("");
    ptr<Node> node = nullptr;
    if (auto cast_expr = dynamic_cast<const Decoration*>(&expr)) {
        node = fmt_decoration(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const Type*>(&expr)) {
        node = makePtr<Text>(cast_expr->to_str());
    } else if (auto cast_expr = dynamic_cast<const IntLiteral*>(&expr)) {
        node = makePtr<Text>(cast_expr->value);
    } else if (auto cast_expr = dynamic_cast<const FloatLiteral*>(&expr)) {
        node = makePtr<Text>(cast_expr->value);
    } else if (auto cast_expr = dynamic_cast<const BoolLiteral*>(&expr)) {
        node = makePtr<Text>(cast_expr->value);
    } else if (auto cast_expr = dynamic_cast<const StringLiteral*>(&expr)) {
        node = makePtr<Text>(cast_expr->value);
    } else if (auto cast_expr = dynamic_cast<const CharLiteral*>(&expr)) {
        node = makePtr<Text>(cast_expr->value);
    } else if (dynamic_cast<const NullLiteral*>(&expr)) {
        node = makePtr<Text>("null");
    } else if (auto cast_expr = dynamic_cast<const DeclRefExpr*>(&expr)) {
        node = fmt_decl_ref_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const CallExpr*>(&expr)) {
        node = fmt_call_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const DerefPtrExpr*>(&expr)) {
        node = fmt_deref_ptr_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const MemberExpr*>(&expr)) {
        node = fmt_member_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const ImportExpr*>(&expr)) {
        node = fmt_import_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const UnaryOperator*>(&expr)) {
        node = fmt_unary_operator(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const BinaryOperator*>(&expr)) {
        node = fmt_binary_operator(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const GroupingExpr*>(&expr)) {
        node = fmt_grouping_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const ErrorGroupExprDecl*>(&expr)) {
        node = fmt_error_group_expr_decl(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const CatchErrorExpr*>(&expr)) {
        node = fmt_catch_error_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const StructInstantiationExpr*>(&expr)) {
        node = fmt_struct_instantiation_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const ArrayInstantiationExpr*>(&expr)) {
        node = fmt_array_instantiation_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const SizeofExpr*>(&expr)) {
        node = fmt_sizeof_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const ArrayAtExpr*>(&expr)) {
        node = fmt_array_at_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const RefPtrExpr*>(&expr)) {
        node = fmt_ref_ptr_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const RangeExpr*>(&expr)) {
        node = fmt_range_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const ErrorInPlaceExpr*>(&expr)) {
        node = fmt_error_in_place_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const TryErrorExpr*>(&expr)) {
        node = fmt_try_error_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const OrElseErrorExpr*>(&expr)) {
        node = fmt_orelse_error_expr(*cast_expr);
    } else if (auto cast_expr = dynamic_cast<const GenericExpr*>(&expr)) {
        node = fmt_generic_expr(*cast_expr);
    } else {
        println(expr.location.to_string());
        expr.dump();
        dmz_unreachable("TODO");
    }
    return node;
}

ptr<Node> Formatter::fmt_decl_ref_expr(const DeclRefExpr& declRefExpr) {
    debug_func("");
    return makePtr<Text>(declRefExpr.identifier);
}

ptr<Node> Formatter::fmt_call_expr(const CallExpr& callExpr) {
    debug_func("");
    auto callee = fmt_expr(*callExpr.callee);
    vec<ptr<Node>> args;
    for (auto&& arg : callExpr.arguments) {
        args.emplace_back(fmt_expr(*arg));
    }
    auto ret = makePtr<Group>(build.new_id(), vec<ptr<Node>>{});
    ret->nodes.emplace_back(fmt_expr(*callExpr.callee));
    ret->nodes.emplace_back(build.comma_separated_list("(", ")", std::move(args), callExpr.haveTrailingComma));
    return ret;
}

ptr<Node> Formatter::fmt_deref_ptr_expr(const DerefPtrExpr& expr) {
    debug_func("");
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("*"));
    ret->nodes.emplace_back(fmt_expr(*expr.expr));
    return ret;
}

ptr<Node> Formatter::fmt_member_expr(const MemberExpr& expr) {
    debug_func("");
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(fmt_expr(*expr.base));
    ret->nodes.emplace_back(makePtr<Text>("."));
    ret->nodes.emplace_back(makePtr<Text>(expr.field));
    return ret;
}

ptr<Node> Formatter::fmt_import_expr(const ImportExpr& expr) {
    debug_func("");
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("import"));
    ret->nodes.emplace_back(makePtr<Text>("("));
    ret->nodes.emplace_back(build.string(expr.identifier));
    ret->nodes.emplace_back(makePtr<Text>(")"));
    return ret;
}

ptr<Node> Formatter::fmt_unary_operator(const UnaryOperator& expr) {
    debug_func("");

    std::unordered_set<TokenType> postUnaryOps = {
        TokenType::op_minusminus,
        TokenType::op_plusplus,
    };

    if (postUnaryOps.count(expr.op) == 0) {
        auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
        ret->nodes.emplace_back(makePtr<Text>(get_op_str(expr.op)));
        ret->nodes.emplace_back(fmt_expr(*expr.operand));
        return ret;
    } else {
        auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
        ret->nodes.emplace_back(fmt_expr(*expr.operand));
        ret->nodes.emplace_back(makePtr<Text>(get_op_str(expr.op)));
        return ret;
    }
}

ptr<Node> Formatter::fmt_binary_operator(const BinaryOperator& expr) {
    debug_func("");
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(fmt_expr(*expr.lhs));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>(get_op_str(expr.op)));
    ret->nodes.emplace_back(makePtr<SpaceOrLineIfWrap>(-1));
    vec<ptr<Node>> rhs;
    rhs.emplace_back(fmt_expr(*expr.rhs));
    ret->nodes.emplace_back(makePtr<IndentIfWrap>(-1, std::move(rhs)));
    return ret;
}

ptr<Node> Formatter::fmt_grouping_expr(const GroupingExpr& expr) {
    debug_func("");
    auto ret = makePtr<Group>(build.new_id(), vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("("));
    ret->nodes.emplace_back(fmt_expr(*expr.expr));
    ret->nodes.emplace_back(makePtr<Text>(")"));
    return ret;
}

ptr<Node> Formatter::fmt_error_group_expr_decl(const ErrorGroupExprDecl& expr) {
    vec<ptr<Node>> errs;
    for (auto&& e : expr.errs) {
        errs.emplace_back(fmt_decl(*e));
    }

    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("error"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(build.comma_separated_list("{", "}", std::move(errs), expr.haveTrailingComma));
    return ret;
}

ptr<Node> Formatter::fmt_catch_error_expr(const CatchErrorExpr& expr) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("catch"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_expr(*expr.errorToCatch));
    return ret;
}

ptr<Node> Formatter::fmt_struct_instantiation_expr(const StructInstantiationExpr& expr) {
    vec<ptr<Node>> arguments;
    for (auto&& field : expr.fieldInitializers) {
        arguments.emplace_back(fmt_stmt(*field));
    }

    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(fmt_expr(*expr.base));
    ret->nodes.emplace_back(build.comma_separated_list("{", "}", std::move(arguments), expr.haveTrailingComma));
    return ret;
}

ptr<Node> Formatter::fmt_array_instantiation_expr(const ArrayInstantiationExpr& expr) {
    vec<ptr<Node>> arguments;
    for (auto&& field : expr.initializers) {
        arguments.emplace_back(fmt_expr(*field));
    }

    return build.comma_separated_list("{", "}", std::move(arguments), expr.haveTrailingComma);
}

ptr<Node> Formatter::fmt_sizeof_expr(const SizeofExpr& expr) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("@sizeof"));
    ret->nodes.emplace_back(makePtr<Text>("("));
    ret->nodes.emplace_back(fmt_expr(*expr.sizeofType));
    ret->nodes.emplace_back(makePtr<Text>(")"));
    return ret;
}

ptr<Node> Formatter::fmt_array_at_expr(const ArrayAtExpr& expr) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(fmt_expr(*expr.array));
    ret->nodes.emplace_back(makePtr<Text>("["));
    ret->nodes.emplace_back(fmt_expr(*expr.index));
    ret->nodes.emplace_back(makePtr<Text>("]"));
    return ret;
}

ptr<Node> Formatter::fmt_ref_ptr_expr(const RefPtrExpr& expr) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("&"));
    ret->nodes.emplace_back(fmt_expr(*expr.expr));
    return ret;
}

ptr<Node> Formatter::fmt_range_expr(const RangeExpr& expr) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(fmt_expr(*expr.startExpr));
    ret->nodes.emplace_back(makePtr<Text>(".."));
    ret->nodes.emplace_back(fmt_expr(*expr.endExpr));
    return ret;
}

ptr<Node> Formatter::fmt_error_in_place_expr(const ErrorInPlaceExpr& expr) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("error."));
    ret->nodes.emplace_back(makePtr<Text>(expr.identifier));
    return ret;
}

ptr<Node> Formatter::fmt_try_error_expr(const TryErrorExpr& expr) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(makePtr<Text>("try"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_expr(*expr.errorToTry));
    return ret;
}

ptr<Node> Formatter::fmt_orelse_error_expr(const OrElseErrorExpr& expr) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(fmt_expr(*expr.errorToOrElse));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(makePtr<Text>("orelse"));
    ret->nodes.emplace_back(makePtr<Space>());
    ret->nodes.emplace_back(fmt_expr(*expr.orElseExpr));
    return ret;
}

ptr<Node> Formatter::fmt_generic_expr(const GenericExpr& expr) {
    auto ret = makePtr<Nodes>(vec<ptr<Node>>{});
    ret->nodes.emplace_back(fmt_expr(*expr.base));
    vec<ptr<Node>> gens;
    for (auto&& gen : expr.types) {
        gens.emplace_back(fmt_expr(*gen));
    }
    ret->nodes.emplace_back(build.comma_separated_list("<", ">", std::move(gens)));
    return ret;
}
}  // namespace fmt
}  // namespace DMZ