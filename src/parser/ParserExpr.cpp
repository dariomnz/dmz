#include "parser/Parser.hpp"

namespace DMZ {

std::unique_ptr<Expr> Parser::parse_primary() {
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
    if (m_nextToken.type == TokenType::lit_string) {
        auto literal = std::make_unique<StringLiteral>(location, m_nextToken.str);
        eat_next_token();  // eat string
        return literal;
    }
    if (m_nextToken.type == TokenType::id) {
        std::string_view identifier = m_nextToken.str;
        eat_next_token();  // eat identifier

        if (!(restrictions & StructNotAllowed) && m_nextToken.type == TokenType::block_l) {
            auto fieldInitList = parse_list_with_trailing_comma<FieldInitStmt>({TokenType::block_l, "expected '{'"},
                                                                               &Parser::parse_field_init_stmt,
                                                                               {TokenType::block_r, "expected '}'"});

            if (!fieldInitList) {
                synchronize_on({TokenType::block_r});
                eat_next_token();  // eat '}'
                return nullptr;
            }

            return std::make_unique<StructInstantiationExpr>(location, std::move(identifier),
                                                             std::move(*fieldInitList));
        }

        auto declRefExpr = std::make_unique<DeclRefExpr>(location, std::move(identifier));
        return declRefExpr;
    }
    if (m_nextToken.type == TokenType::kw_catch) {
        return parse_catch_err_expr();
    }
    if (m_nextToken.type == TokenType::kw_try) {
        return parse_try_err_expr();
    }

    return report(location, "expected expression");
}

// <postfixExpression>
//  ::= <primaryExpression> <argumentList>? <memberExpr>*
//
// <argumentList>
//  ::= '(' (<expr> (',' <expr>)* ','?)? ')'
//
// <memberExpr>
//  ::= '.' <identifier>
std::unique_ptr<Expr> Parser::parse_postfix_expr() {
    varOrReturn(expr, parse_primary());

    if (auto declRef = dynamic_cast<DeclRefExpr *>(expr.get())) {
        if (m_nextToken.type == TokenType::coloncolon) {
            eat_next_token();  // eat '::'

            varOrReturn(modExpr, parse_expr());
            return std::make_unique<ModuleDeclRefExpr>(declRef->location, declRef->identifier, std::move(modExpr));
        }
    }

    if (m_nextToken.type == TokenType::par_l) {
        SourceLocation location = m_nextToken.loc;
        varOrReturn(argumentList,
                    parse_list_with_trailing_comma<Expr>({TokenType::par_l, "expected '('"}, &Parser::parse_expr,
                                                         {TokenType::par_r, "expected ')'"}));

        expr = std::make_unique<CallExpr>(location, std::move(expr), std::move(*argumentList));
    }

    while (m_nextToken.type == TokenType::dot) {
        SourceLocation location = m_nextToken.loc;
        eat_next_token();  // eat '.'

        matchOrReturn(TokenType::id, "expected field identifier");
        // assert(m_nextToken.value && "identifier without value");

        expr = std::make_unique<MemberExpr>(location, std::move(expr), m_nextToken.str);
        eat_next_token();  // eat identifier
    }

    if (m_nextToken.type == TokenType::op_quest_mark) {
        SourceLocation location = m_nextToken.loc;
        if (auto declref = dynamic_cast<DeclRefExpr *>(expr.get())) {
            eat_next_token();  // eat '?'
            expr = std::make_unique<ErrDeclRefExpr>(location, declref->identifier);
        } else {
            return report(location, "expected identifier");
        }
    }

    if (m_nextToken.type == TokenType::op_excla_mark) {
        SourceLocation location = m_nextToken.loc;
        eat_next_token();  // eat '!'
        expr = std::make_unique<ErrUnwrapExpr>(location, std::move(expr));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parse_prefix_expr() {
    Token tok = m_nextToken;
    std::unordered_set<TokenType> unaryOps = {
        TokenType::op_minus,
        TokenType::op_excla_mark,
        TokenType::amp,
    };

    if (unaryOps.count(tok.type) == 0) {
        return parse_postfix_expr();
    }
    eat_next_token();  // eat unaryOps

    varOrReturn(rhs, parse_prefix_expr());

    return std::make_unique<UnaryOperator>(tok.loc, std::move(rhs), tok.type);
}

std::unique_ptr<Expr> Parser::parse_expr() {
    varOrReturn(lhs, parse_prefix_expr());
    return parse_expr_rhs(std::move(lhs), 0);
}

[[maybe_unused]] constexpr static inline int get_token_precedence(TokenType tok) {
    switch (tok) {
        case TokenType::op_mult:
        case TokenType::op_div:
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
        case TokenType::op_and:
            return 2;
        case TokenType::op_or:
            return 1;
        default:
            return -1;
    }
}

std::unique_ptr<Expr> Parser::parse_expr_rhs(std::unique_ptr<Expr> lhs, int precedence) {
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

std::unique_ptr<CatchErrExpr> Parser::parse_catch_err_expr() {
    matchOrReturn(TokenType::kw_catch, "expected 'catch'");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat catch

    std::string_view identifier = m_nextToken.str;
    auto idLocation = m_nextToken.loc;
    varOrReturn(first_expr, parse_expr());

    Type type = Type::builtinErr("err");
    std::unique_ptr<Expr> initializer;

    if (m_nextToken.type != TokenType::op_assign) {
        return std::make_unique<CatchErrExpr>(location, std::move(first_expr), nullptr);
    }

    matchOrReturn(TokenType::op_assign, "expected '='");
    eat_next_token();  // eat '='

    varOrReturn(errToCatch, parse_expr());

    auto varDecl = std::make_unique<VarDecl>(idLocation, identifier, type, false, std::move(errToCatch));
    auto declaration = std::make_unique<DeclStmt>(idLocation, std::move(varDecl));
    return std::make_unique<CatchErrExpr>(location, nullptr, std::move(declaration));
}

std::unique_ptr<TryErrExpr> Parser::parse_try_err_expr() {
    matchOrReturn(TokenType::kw_try, "expected 'try'");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat try

    std::string_view identifier = m_nextToken.str;
    auto idLocation = m_nextToken.loc;
    varOrReturn(first_expr, parse_expr());

    std::unique_ptr<Expr> initializer;

    if (m_nextToken.type != TokenType::op_assign) {
        return std::make_unique<TryErrExpr>(location, std::move(first_expr), nullptr);
    }

    matchOrReturn(TokenType::op_assign, "expected '='");
    eat_next_token();  // eat '='

    varOrReturn(errToCatch, parse_expr());

    auto varDecl = std::make_unique<VarDecl>(idLocation, identifier, std::nullopt, false, std::move(errToCatch));
    auto declaration = std::make_unique<DeclStmt>(idLocation, std::move(varDecl));
    return std::make_unique<TryErrExpr>(location, nullptr, std::move(declaration));
}
}  // namespace DMZ