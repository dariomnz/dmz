#include "driver/Driver.hpp"
#include "parser/Parser.hpp"
namespace DMZ {

std::unique_ptr<Expr> Parser::parse_primary() {
    debug_func(m_nextToken.loc << " '" << m_nextToken.str << "'");
    SourceLocation location = m_nextToken.loc;

    if (m_nextToken.type == TokenType::par_l) {
        eat_next_token();  // eat '('

        varOrReturn(expr, with_no_restrictions<std::unique_ptr<Expr>>([&]() { return parse_expr(); }));

        matchOrReturn(TokenType::par_r, "expected ')'");
        eat_next_token();  // eat ')'

        return std::make_unique<GroupingExpr>(location, std::move(expr));
    }
    if (m_nextToken.type == TokenType::lit_int) {
        auto literal = std::make_unique<IntLiteral>(location, m_nextToken.str);
        eat_next_token();  // eat int
        return literal;
    }
    if (m_nextToken.type == TokenType::lit_float) {
        auto literal = std::make_unique<FloatLiteral>(location, m_nextToken.str);
        eat_next_token();  // eat float
        return literal;
    }
    if (m_nextToken.type == TokenType::lit_char) {
        auto literal = std::make_unique<CharLiteral>(location, m_nextToken.str);
        eat_next_token();  // eat char
        return literal;
    }
    if (m_nextToken.type == TokenType::kw_true || m_nextToken.type == TokenType::kw_false) {
        auto literal = std::make_unique<BoolLiteral>(location, m_nextToken.str);
        eat_next_token();  // eat bool
        return literal;
    }
    if (m_nextToken.type == TokenType::kw_null) {
        auto literal = std::make_unique<NullLiteral>(location);
        eat_next_token();  // eat null
        return literal;
    }
    if (m_nextToken.type == TokenType::lit_string) {
        auto literal = std::make_unique<StringLiteral>(location, m_nextToken.str);
        eat_next_token();  // eat string
        return literal;
    }
    if (m_nextToken.type == TokenType::id) {
        std::string_view identifier = m_nextToken.str;
        eat_next_token();  // eat identifier

        std::unique_ptr<Expr> expr = std::make_unique<DeclRefExpr>(location, std::move(identifier));

        if (!(restrictions & StructNotAllowed) &&
            (m_nextToken.type == TokenType::block_l || nextToken_is_generic(TokenType::block_l))) {
            auto genericTypes = parse_generic_types();

            auto fieldInitList = parse_list_with_trailing_comma<FieldInitStmt>({TokenType::block_l, "expected '{'"},
                                                                               &Parser::parse_field_init_stmt,
                                                                               {TokenType::block_r, "expected '}'"});

            if (!fieldInitList) {
                synchronize_on({TokenType::block_r});
                eat_next_token();  // eat '}'
                return nullptr;
            }

            GenericTypes genTypes = genericTypes ? *genericTypes : GenericTypes{{}};
            expr = std::make_unique<StructInstantiationExpr>(location, std::move(expr), std::move(genTypes),
                                                             std::move(*fieldInitList));
        }
        return expr;
    }
    if (m_nextToken.type == TokenType::block_l) {
        auto initList = parse_list_with_trailing_comma<Expr>({TokenType::block_l, "expected '{'"}, &Parser::parse_expr,
                                                             {TokenType::block_r, "expected '}'"});
        if (!initList) {
            synchronize_on({TokenType::block_r});
            eat_next_token();  // eat '}'
            return nullptr;
        }

        return std::make_unique<ArrayInstantiationExpr>(location, std::move(*initList));
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
        return parse_error_group_expr_decl();
    }
    if (m_nextToken.type == TokenType::dot) {
        return parse_self_member_expr();
    }
    if (m_nextToken.type == TokenType::kw_sizeof) {
        return parse_sizeof_expr();
    }

    return report(location, "expected expression");
}

// <postfixExpression>
//  ::= <primaryExpression> <argumentList> <memberExpr>*
//
// <argumentList>
//  ::= '(' (<expr> (',' <expr>)* ',') ')'
//
// <memberExpr>
//  ::= '.' <identifier>
std::unique_ptr<Expr> Parser::parse_postfix_expr() {
    debug_func("");
    varOrReturn(expr, parse_primary());

    std::unique_ptr<DMZ::GenericTypes> genericTypes;

    if (m_nextToken.type == TokenType::bracket_l) {
        SourceLocation location = m_nextToken.loc;
        matchOrReturn(TokenType::bracket_l, "expected '['");
        eat_next_token();  // eat '['

        varOrReturn(index, parse_expr());

        matchOrReturn(TokenType::bracket_r, "expected ']'");
        eat_next_token();  // eat ']'
        expr = std::make_unique<ArrayAtExpr>(location, std::move(expr), std::move(index));
    }

    while (m_nextToken.type == TokenType::dot || m_nextToken.type == TokenType::par_l ||
           nextToken_is_generic(TokenType::par_l) || m_nextToken.type == TokenType::block_l ||
           nextToken_is_generic(TokenType::block_l)) {
        debug_msg(m_nextToken);
        if (nextToken_is_generic(TokenType::par_l) || nextToken_is_generic(TokenType::block_l)) {
            genericTypes = parse_generic_types();
        }
        if (m_nextToken.type == TokenType::par_l) {
            SourceLocation location = m_nextToken.loc;
            varOrReturn(argumentList,
                        parse_list_with_trailing_comma<Expr>({TokenType::par_l, "expected '('"}, &Parser::parse_expr,
                                                             {TokenType::par_r, "expected ')'"}));

            expr = std::make_unique<CallExpr>(location, std::move(expr), std::move(*argumentList),
                                              std::move(genericTypes));
        }
        if (m_nextToken.type == TokenType::block_l) {
            SourceLocation location = m_nextToken.loc;
            auto fieldInitList = parse_list_with_trailing_comma<FieldInitStmt>({TokenType::block_l, "expected '{'"},
                                                                               &Parser::parse_field_init_stmt,
                                                                               {TokenType::block_r, "expected '}'"});

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
            GenericTypes genTypes = genericTypes ? *genericTypes : GenericTypes{{}};

            expr = std::make_unique<StructInstantiationExpr>(expr->location, std::move(expr), std::move(genTypes),
                                                             std::move(*fieldInitList));
        }
        if (m_nextToken.type == TokenType::dot) {
            SourceLocation location = m_nextToken.loc;
            eat_next_token();  // eat '.'

            matchOrReturn(TokenType::id, "expected field identifier");
            // assert(m_nextToken.value && "identifier without value");

            expr = std::make_unique<MemberExpr>(location, std::move(expr), m_nextToken.str);
            eat_next_token();  // eat identifier
        }
    }

    if (m_nextToken.type == TokenType::kw_orelse) {
        eat_next_token();  // eat orelse
        varOrReturn(orelse, parse_expr());

        expr = std::make_unique<OrElseErrorExpr>(expr->location, std::move(expr), std::move(orelse));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_prefix_expr() {
    debug_func("");
    Token tok = m_nextToken;
    std::unordered_set<TokenType> unaryOps = {
        TokenType::op_minus,
        TokenType::op_excla_mark,
        TokenType::amp,
        TokenType::asterisk,
    };

    if (unaryOps.count(tok.type) == 0) {
        return parse_postfix_expr();
    }
    eat_next_token();  // eat unaryOps

    varOrReturn(rhs, parse_prefix_expr());

    switch (tok.type) {
        case TokenType::asterisk:
            return std::make_unique<DerefPtrExpr>(tok.loc, std::move(rhs));
        case TokenType::amp:
            return std::make_unique<RefPtrExpr>(tok.loc, std::move(rhs));
        default:
            return std::make_unique<UnaryOperator>(tok.loc, std::move(rhs), tok.type);
    }
}

std::unique_ptr<Expr> Parser::parse_expr() {
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

std::unique_ptr<Expr> Parser::parse_expr_rhs(std::unique_ptr<Expr> lhs, int precedence) {
    debug_func("");
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

        lhs = std::make_unique<BinaryOperator>(op.loc, std::move(lhs), std::move(rhs), op.type);
    }
}

std::unique_ptr<CatchErrorExpr> Parser::parse_catch_error_expr() {
    debug_func("");
    matchOrReturn(TokenType::kw_catch, "expected 'catch'");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat catch

    std::string_view identifier = m_nextToken.str;
    auto idLocation = m_nextToken.loc;
    varOrReturn(first_expr, parse_expr());

    Type type = Type::builtinError("err");
    std::unique_ptr<Expr> initializer;

    if (m_nextToken.type != TokenType::op_assign) {
        return std::make_unique<CatchErrorExpr>(location, std::move(first_expr), nullptr);
    }

    matchOrReturn(TokenType::op_assign, "expected '='");
    eat_next_token();  // eat '='

    varOrReturn(errorToCatch, parse_expr());

    auto varDecl = std::make_unique<VarDecl>(idLocation, identifier, std::make_unique<Type>(std::move(type)), false,
                                             std::move(errorToCatch));
    auto declaration = std::make_unique<DeclStmt>(idLocation, std::move(varDecl));
    return std::make_unique<CatchErrorExpr>(location, nullptr, std::move(declaration));
}

std::unique_ptr<TryErrorExpr> Parser::parse_try_error_expr() {
    debug_func("");
    matchOrReturn(TokenType::kw_try, "expected 'try'");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat try

    varOrReturn(first_expr, parse_expr());

    return std::make_unique<TryErrorExpr>(location, std::move(first_expr));
}

std::unique_ptr<ImportExpr> Parser::parse_import_expr() {
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

    Driver::register_import(identifier);
    return std::make_unique<ImportExpr>(location, identifier);
}

std::unique_ptr<SelfMemberExpr> Parser::parse_self_member_expr() {
    debug_func("");
    auto loc = m_nextToken.loc;
    matchOrReturn(TokenType::dot, "expected '.'");
    eat_next_token();  // eat .

    auto identifier = m_nextToken.str;
    eat_next_token();  // eat id

    return std::make_unique<SelfMemberExpr>(loc, identifier);
}

std::unique_ptr<SizeofExpr> Parser::parse_sizeof_expr() {
    debug_func("");
    matchOrReturn(TokenType::kw_sizeof, "expected @sizeof");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat @sizeof

    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat (

    varOrReturn(type, parse_type());

    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();  // eat )

    return std::make_unique<SizeofExpr>(location, *type);
}
}  // namespace DMZ