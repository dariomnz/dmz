#include "driver/Driver.hpp"
#include "parser/Parser.hpp"

namespace DMZ {

ptr<Expr> Parser::parse_primary() {
    debug_func(m_nextToken.loc << " '" << m_nextToken.str << "' " << restiction_to_str(restrictions));
    SourceLocation location = m_nextToken.loc;

    if (m_nextToken.type == TokenType::ty_void) {
        auto literal = makePtr<TypeVoid>(location);
        eat_next_token();  // eat void
        return literal;
    }
    if (m_nextToken.type == TokenType::ty_bool) {
        auto literal = makePtr<TypeBool>(location);
        eat_next_token();  // eat void
        return literal;
    }
    if (m_nextToken.type == TokenType::ty_iN || m_nextToken.type == TokenType::ty_uN ||
        m_nextToken.type == TokenType::ty_f16 || m_nextToken.type == TokenType::ty_f32 ||
        m_nextToken.type == TokenType::ty_f64) {
        auto literal = makePtr<TypeNumber>(location, m_nextToken.str);
        eat_next_token();  // eat number
        return literal;
    }
    if (m_nextToken.type == TokenType::kw_fn && peek_token().type == TokenType::par_l) {
        eat_next_token();  // eat fn
        varOrReturn(paramsList,
                    (parse_expr_list_with_trailing_comma(
                        {TokenType::par_l, "expected '('"},
                        [&]() { return with_restrictions<ptr<Expr>>(OnlyTypeExpr, [&]() { return parse_expr(); }); },
                        {TokenType::par_r, "expected ')'"})));

        matchOrReturn(TokenType::return_arrow, "expected '->'");
        eat_next_token();  // eat '->'

        varOrReturn(returnType, with_restrictions<ptr<Expr>>(OnlyTypeExpr, [&]() { return parse_expr(); }));

        return makePtr<TypeFunction>(location, std::move(*paramsList), std::move(returnType));
    }
    if (m_nextToken.type == TokenType::id) {
        auto identifier = m_nextToken.str;
        eat_next_token();  // eat identifier

        return makePtr<DeclRefExpr>(location, std::move(identifier));
    }
    if (!(restrictions & OnlyTypeExpr)) {
        if (m_nextToken.type == TokenType::par_l) {
            eat_next_token();  // eat '('

            varOrReturn(expr, with_no_restrictions<ptr<Expr>>([&]() { return parse_expr(); }));

            matchOrReturn(TokenType::par_r, "expected ')'");
            eat_next_token();  // eat ')'

            return makePtr<GroupingExpr>(location, std::move(expr));
        }
        if (m_nextToken.type == TokenType::lit_int) {
            auto literal = makePtr<IntLiteral>(location, m_nextToken.str);
            eat_next_token();  // eat int
            return literal;
        }
        if (m_nextToken.type == TokenType::lit_float) {
            auto literal = makePtr<FloatLiteral>(location, m_nextToken.str);
            eat_next_token();  // eat float
            return literal;
        }
        if (m_nextToken.type == TokenType::lit_char) {
            auto literal = makePtr<CharLiteral>(location, m_nextToken.str);
            eat_next_token();  // eat char
            return literal;
        }
        if (m_nextToken.type == TokenType::kw_true || m_nextToken.type == TokenType::kw_false) {
            auto literal = makePtr<BoolLiteral>(location, m_nextToken.str);
            eat_next_token();  // eat bool
            return literal;
        }
        if (m_nextToken.type == TokenType::kw_null) {
            auto literal = makePtr<NullLiteral>(location);
            eat_next_token();  // eat null
            return literal;
        }
        if (m_nextToken.type == TokenType::lit_string) {
            auto literal = makePtr<StringLiteral>(location, m_nextToken.str);
            eat_next_token();  // eat string
            return literal;
        }
        if (m_nextToken.type == TokenType::block_l) {
            auto initList = parse_list_with_trailing_comma<Expr>(
                {TokenType::block_l, "expected '{'"}, &Parser::parse_expr, {TokenType::block_r, "expected '}'"});
            if (!initList) {
                synchronize_on({TokenType::block_r});
                eat_next_token();  // eat '}'
                return nullptr;
            }

            return makePtr<ArrayInstantiationExpr>(location, std::move(*initList));
        }
        if (m_nextToken.type == TokenType::kw_catch) {
            return parse_catch_error_expr();
        }
        if (m_nextToken.type == TokenType::kw_try) {
            return parse_try_error_expr();
        }
        if (m_nextToken.type == TokenType::kw_import) {
            return parse_import_expr();
        }
        if (m_nextToken.type == TokenType::kw_error) {
            if (peek_token().type == TokenType::dot) {
                return parse_error_in_place_expr();
            } else {
                return parse_error_group_expr_decl();
            }
        }
        if (m_nextToken.type == TokenType::dot) {
            return parse_self_member_expr();
        }
        if (m_nextToken.type == TokenType::kw_sizeof) {
            return parse_sizeof_expr();
        }
    }
    if (restrictions & OnlyTypeExpr) {
        return report(location, "expected type expression");
    } else {
        return report(location, "expected expression");
    }
}

// <postfixExpression>
//  ::= <primaryExpression> <argumentList> <memberExpr>*
//
// <argumentList>
//  ::= '(' (<expr> (',' <expr>)* ',') ')'
//
// <memberExpr>
//  ::= '.' <identifier>
ptr<Expr> Parser::parse_postfix_expr(ptr<Expr> expr) {
    debug_func(m_nextToken.loc << " '" << m_nextToken.str << "'" << restiction_to_str(restrictions));

    if (m_nextToken.type == TokenType::bracket_l) {
        SourceLocation location = m_nextToken.loc;
        matchOrReturn(TokenType::bracket_l, "expected '['");
        eat_next_token();  // eat '['

        varOrReturn(index, with_no_restrictions<ptr<Expr>>([&]() { return parse_expr(); }));

        matchOrReturn(TokenType::bracket_r, "expected ']'");
        eat_next_token();  // eat ']'
        expr = makePtr<ArrayAtExpr>(location, std::move(expr), std::move(index));
        return parse_postfix_expr(std::move(expr));
    }

    if (m_nextToken.type == TokenType::op_less && nextToken_is_generic()) {
        varOrReturn(genericExpr, parse_generic_expr(expr));
        expr = std::move(genericExpr);
        return parse_postfix_expr(std::move(expr));
    }

    // while (m_nextToken.type == TokenType::dot || m_nextToken.type == TokenType::par_l ||
    //        m_nextToken.type == TokenType::block_l || nextToken_is_generic()) {
    //     debug_msg(m_nextToken);
    // if ((nextToken_is_generic())) {
    //     genericTypes = parse_generic_expr();
    // }
    if (!(restrictions & OnlyTypeExpr) && m_nextToken.type == TokenType::par_l) {
        SourceLocation location = m_nextToken.loc;
        varOrReturn(argumentList,
                    parse_list_with_trailing_comma<Expr>({TokenType::par_l, "expected '('"}, &Parser::parse_expr,
                                                         {TokenType::par_r, "expected ')'"}));

        expr = makePtr<CallExpr>(location, std::move(expr), std::move(*argumentList));
        return parse_postfix_expr(std::move(expr));
    }
    if (!(restrictions & (StructNotAllowed | OnlyTypeExpr)) && m_nextToken.type == TokenType::block_l) {
        // if ((restrictions & (StructNotAllowed | OnlyTypeExpr))) break;
        SourceLocation location = m_nextToken.loc;
        auto fieldInitList = parse_list_with_trailing_comma<FieldInitStmt>(
            {TokenType::block_l, "expected '{'"}, &Parser::parse_field_init_stmt, {TokenType::block_r, "expected '}'"});

        if (!fieldInitList) {
            synchronize_on({TokenType::block_r});
            eat_next_token();  // eat '}'
            return nullptr;
        }
        // expr->dump();
        // if (auto declRefExpr = dynamic_cast<DeclRefExpr*>(expr.get())) {
        //     identifier = declRefExpr->identifier;
        // } else if (auto memberExpr = dynamic_cast<MemberExpr*>(expr.get())) {
        //     identifier = memberExpr->field;
        // } else {
        //     expr->dump();
        //     return report(expr->location, "internal error struct instatiation");
        // }
        // Type t = Type::customType(identifier);
        // GenericTypes genTypes = genericTypes ? *genericTypes : GenericTypes{{}};

        expr = makePtr<StructInstantiationExpr>(expr->location, std::move(expr), std::move(*fieldInitList));
        return parse_postfix_expr(std::move(expr));
    }
    if (m_nextToken.type == TokenType::dot) {
        SourceLocation location = m_nextToken.loc;
        eat_next_token();  // eat '.'

        matchOrReturn(TokenType::id, "expected field identifier");
        // assert(m_nextToken.value && "identifier without value");

        expr = makePtr<MemberExpr>(location, std::move(expr), m_nextToken.str);
        eat_next_token();  // eat identifier
        return parse_postfix_expr(std::move(expr));
    }

    if (!(restrictions & OnlyTypeExpr) && m_nextToken.type == TokenType::kw_orelse) {
        eat_next_token();  // eat orelse
        varOrReturn(orelse, parse_expr());

        expr = makePtr<OrElseErrorExpr>(expr->location, std::move(expr), std::move(orelse));
        return parse_postfix_expr(std::move(expr));
    }

    if ((restrictions & OnlyTypeExpr)) {
        if (m_nextToken.type == TokenType::op_excla_mark) {
            eat_next_token();  // eat !
            expr = makePtr<UnaryOperator>(expr->location, std::move(expr), TokenType::op_excla_mark);
            return parse_postfix_expr(std::move(expr));
        }
    }

    return expr;
}

ptr<Expr> Parser::parse_prefix_expr() {
    debug_func(m_nextToken.loc << " '" << m_nextToken.str << "'");
    Token tok = m_nextToken;
    std::unordered_set<TokenType> unaryOps = {
        TokenType::op_minus,
        TokenType::op_excla_mark,
        TokenType::amp,
        TokenType::asterisk,
    };

    if (unaryOps.count(tok.type) == 0) {
        varOrReturn(expr, parse_primary());
        return parse_postfix_expr(std::move(expr));
    }
    eat_next_token();  // eat unaryOps

    varOrReturn(rhs, parse_prefix_expr());

    switch (tok.type) {
        case TokenType::asterisk:
            return makePtr<DerefPtrExpr>(tok.loc, std::move(rhs));
        case TokenType::amp:
            return makePtr<RefPtrExpr>(tok.loc, std::move(rhs));
        default:
            return makePtr<UnaryOperator>(tok.loc, std::move(rhs), tok.type);
    }
}

ptr<Expr> Parser::parse_expr() {
    debug_func("");
    varOrReturn(lhs, parse_prefix_expr());
    return parse_expr_rhs(std::move(lhs), 0);
}

[[maybe_unused]] static inline int get_token_precedence(TokenType tok) {
    debug_func("");
    switch (tok) {
        case TokenType::asterisk:
        case TokenType::op_div:
        case TokenType::op_percent:
            return 6;
        case TokenType::op_plus:
        case TokenType::op_minus:
            return 5;
        case TokenType::op_less:
        case TokenType::op_more:
        case TokenType::op_less_eq:
        case TokenType::op_more_eq:
            return 4;
        case TokenType::op_equal:
        case TokenType::op_not_equal:
            return 3;
        case TokenType::ampamp:
            return 2;
        case TokenType::pipepipe:
            return 1;
        default:
            return -1;
    }
}

ptr<Expr> Parser::parse_expr_rhs(ptr<Expr> lhs, int precedence) {
    debug_func("prec " << precedence << " " << m_nextToken.loc << " '" << m_nextToken.str << "'");
    if ((restrictions & OnlyTypeExpr)) return lhs;
    while (true) {
        Token op = m_nextToken;
        int curOpPrec = get_token_precedence(op.type);

        if (curOpPrec < precedence) return lhs;
        eat_next_token();  // eat operator

        varOrReturn(rhs, parse_prefix_expr());

        if (curOpPrec < get_token_precedence(m_nextToken.type)) {
            rhs = parse_expr_rhs(std::move(rhs), curOpPrec + 1);
            if (!rhs) return nullptr;
        }

        lhs = makePtr<BinaryOperator>(op.loc, std::move(lhs), std::move(rhs), op.type);
    }
}

ptr<CatchErrorExpr> Parser::parse_catch_error_expr() {
    debug_func("");
    matchOrReturn(TokenType::kw_catch, "expected 'catch'");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat catch

    varOrReturn(errorToCatch, parse_expr());

    return makePtr<CatchErrorExpr>(location, std::move(errorToCatch));
}

ptr<TryErrorExpr> Parser::parse_try_error_expr() {
    debug_func("");
    matchOrReturn(TokenType::kw_try, "expected 'try'");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat try

    varOrReturn(errorToTry, parse_expr());

    return makePtr<TryErrorExpr>(location, std::move(errorToTry));
}

ptr<ImportExpr> Parser::parse_import_expr() {
    debug_func("");
    auto location = m_nextToken.loc;
    matchOrReturn(TokenType::kw_import, "expected 'import'");
    eat_next_token();  // eat import

    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat (

    matchOrReturn(TokenType::lit_string, "expected string literal");
    auto identifier = m_nextToken.str.substr(1, m_nextToken.str.size() - 2);
    eat_next_token();  // eat (

    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();  // eat )

    auto ids = Driver::register_import(m_lexer.get_file_path(), identifier);
    return makePtr<ImportExpr>(location, identifier, ids.first, ids.second);
}

ptr<SelfMemberExpr> Parser::parse_self_member_expr() {
    debug_func("");
    auto loc = m_nextToken.loc;
    matchOrReturn(TokenType::dot, "expected '.'");
    eat_next_token();  // eat .

    auto identifier = m_nextToken.str;
    eat_next_token();  // eat id

    return makePtr<SelfMemberExpr>(loc, identifier);
}

ptr<SizeofExpr> Parser::parse_sizeof_expr() {
    debug_func("");
    matchOrReturn(TokenType::kw_sizeof, "expected @sizeof");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat @sizeof

    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat (

    varOrReturn(type, with_restrictions<ptr<Expr>>(OnlyTypeExpr, [&]() { return parse_expr(); }));

    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();  // eat )

    return makePtr<SizeofExpr>(location, std::move(type));
}
}  // namespace DMZ