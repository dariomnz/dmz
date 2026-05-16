#ifdef DEBUG_PARSER
#ifndef DEBUG
#define DEBUG
#endif
#endif

#include "Utils.hpp"
#include "parser/Parser.hpp"

namespace DMZ {

ptr<Expr> Parser::parse_primary() {
    debug_func(m_nextToken.loc << " " << m_nextToken.type << " '" << m_nextToken.str << "' "
                               << restiction_to_str(restrictions));
    SourceLocation location = m_nextToken.loc;

    if (m_nextToken.type == TokenType::ty_void) {
        auto literal = makePtr<TypeVoid>(location);
        eat_next_token();  // eat void
        return literal;
    }
    if (m_nextToken.type == TokenType::ty_bool) {
        auto literal = makePtr<TypeBool>(location);
        eat_next_token();  // eat bool
        return literal;
    }
    if (m_nextToken.type == TokenType::ty_type) {
        auto literal = makePtr<TypeType>(location);
        eat_next_token();  // eat type
        return literal;
    }
    if (m_nextToken.type == TokenType::ty_err) {
        auto literal = makePtr<TypeError>(location);
        eat_next_token();  // eat err
        return literal;
    }
    if (m_nextToken.type == TokenType::ty_anytype) {
        auto literal = makePtr<TypeAnyType>(location);
        eat_next_token();  // eat anytype
        return literal;
    }
    if (m_nextToken.type == TokenType::ty_iN || m_nextToken.type == TokenType::ty_uN ||
        m_nextToken.type == TokenType::ty_f16 || m_nextToken.type == TokenType::ty_f32 ||
        m_nextToken.type == TokenType::ty_f64 || m_nextToken.type == TokenType::ty_isize ||
        m_nextToken.type == TokenType::ty_usize) {
        auto literal = makePtr<TypeNumber>(location, m_nextToken.str);
        eat_next_token();  // eat number
        return literal;
    }
    if (m_nextToken.type == TokenType::kw_fn && peek_token().type == TokenType::par_l) {
        eat_next_token();  // eat fn
        bool haveTrailingComma;
        varOrReturn(paramsList, (parse_list_with_trailing_comma<Expr>(
                                    {TokenType::par_l, "expected '('"}, [&]() { return parse_type(); },
                                    {TokenType::par_r, "expected ')'"}, haveTrailingComma)));

        matchOrReturn(TokenType::return_arrow, "expected '->'");
        eat_next_token();  // eat '->'

        varOrReturn(returnType, parse_type());

        return makePtr<TypeFunction>(location, std::move(*paramsList), std::move(returnType));
    }
    if (m_nextToken.type == TokenType::id) {
        auto identifier = m_nextToken.str;
        eat_next_token();  // eat identifier

        return makePtr<DeclRefExpr>(location, std::move(identifier));
    }
    if (m_nextToken.type == TokenType::kw_this) {
        eat_next_token();  // eat @This
        return makePtr<DeclRefExpr>(location, "@This");
    }
    if (m_nextToken.type == TokenType::kw_struct || m_nextToken.type == TokenType::kw_union ||
        m_nextToken.type == TokenType::kw_enum || m_nextToken.type == TokenType::kw_packed) {
        if (m_nextToken.type == TokenType::kw_union) {
            return parse_union_decl();
        } else if (m_nextToken.type == TokenType::kw_enum) {
            return parse_enum_decl();
        } else {
            return parse_struct_decl();
        }
    }
    if (!(restrictions & OnlyTypeExpr)) {
        if (m_nextToken.type == TokenType::dot && peek_token().type == TokenType::block_l) {
            SourceLocation location = m_nextToken.loc;
            eat_next_token();  // eat '.'
            bool haveTrailingComma;
            auto elements = parse_list_with_trailing_comma<Expr>(
                {TokenType::block_l, "expected '{'"}, [this]() { return parse_expr(); },
                {TokenType::block_r, "expected '}'"}, haveTrailingComma);
            if (!elements) {
                synchronize_on({TokenType::block_r});
                eat_next_token();  // eat '}'
                return nullptr;
            }

            return makePtr<TupleInstantiationExpr>(location, std::move(*elements), haveTrailingComma);
        }
        if (m_nextToken.type == TokenType::dot && peek_token().type == TokenType::id) {
            SourceLocation location = m_nextToken.loc;
            eat_next_token();  // eat '.'
            auto identifier = m_nextToken.str;
            eat_next_token();  // eat identifier
            return makePtr<AutoMemberExpr>(location, std::move(identifier));
        }
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
            bool haveTrailingComma;
            auto initList = parse_list_with_trailing_comma<Expr>(
                {TokenType::block_l, "expected '{'"}, [this]() { return parse_expr(); },
                {TokenType::block_r, "expected '}'"}, haveTrailingComma);
            if (!initList) {
                synchronize_on({TokenType::block_r});
                eat_next_token();  // eat '}'
                return nullptr;
            }

            return makePtr<ArrayInstantiationExpr>(location, std::move(*initList), haveTrailingComma);
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
    debug_func(m_nextToken.loc << " '" << m_nextToken.str << "' " << restiction_to_str(restrictions));

    if (m_nextToken.type == TokenType::bracket_l) {
        SourceLocation location = m_nextToken.loc;
        matchOrReturn(TokenType::bracket_l, "expected '['");
        eat_next_token();  // eat '['

        varOrReturn(index, parse_expr());

        matchOrReturn(TokenType::bracket_r, "expected ']'");
        eat_next_token();  // eat ']'
        expr = makePtr<ArrayAtExpr>(location, std::move(expr), std::move(index));
        return parse_postfix_expr(std::move(expr));
    }

    if (m_nextToken.type == TokenType::dotdot) {
        eat_next_token();  // eat '..'
        varOrReturn(endRange, parse_expr());
        return makePtr<RangeExpr>(expr->location, std::move(expr), std::move(endRange));
    }

    if (m_nextToken.type == TokenType::par_l) {
        SourceLocation location = m_nextToken.loc;
        bool haveTrailingComma;
        varOrReturn(argumentList, with_no_restrictions<ptr<vec<ptr<Expr>>>>([&]() {
                        return parse_list_with_trailing_comma<Expr>(
                            {TokenType::par_l, "expected '('"}, [this]() { return parse_expr(); },
                            {TokenType::par_r, "expected ')'"}, haveTrailingComma);
                    }));

        expr = makePtr<CallExpr>(location, std::move(expr), std::move(*argumentList), haveTrailingComma);
        return parse_postfix_expr(std::move(expr));
    }
    if (!(restrictions & (StructNotAllowed | OnlyTypeExpr)) && m_nextToken.type == TokenType::block_l) {
        SourceLocation location = m_nextToken.loc;
        bool haveTrailingComma;
        auto fieldInitList = parse_list_with_trailing_comma<FieldInitStmt>(
            {TokenType::block_l, "expected '{'"}, [this]() { return parse_field_init_stmt(); },
            {TokenType::block_r, "expected '}'"}, haveTrailingComma);

        if (!fieldInitList) {
            synchronize_on({TokenType::block_r});
            eat_next_token();  // eat '}'
            return nullptr;
        }
        expr = makePtr<StructInstantiationExpr>(expr->location, std::move(expr), std::move(*fieldInitList),
                                                haveTrailingComma);
        return parse_postfix_expr(std::move(expr));
    }
    if (m_nextToken.type == TokenType::dot) {
        SourceLocation location = m_nextToken.loc;
        eat_next_token();  // eat '.'

        if (m_nextToken.type == TokenType::id) {
            expr = makePtr<MemberExpr>(location, std::move(expr), m_nextToken.str);
            eat_next_token();  // eat identifier
            return parse_postfix_expr(std::move(expr));
        } else if (m_nextToken.type == TokenType::asterisk) {
            expr = makePtr<DerefPtrExpr>(location, std::move(expr));
            eat_next_token();  // eat '*'
            return parse_postfix_expr(std::move(expr));
        } else {
            m_incompleteAST = true;
            m_expectIncompleteStatement = true;
            expr = makePtr<MemberExpr>(location, std::move(expr), "");
            report(m_nextToken.loc, "expected member identifier or '*'");
            return parse_postfix_expr(std::move(expr));
        }
    }
    if (m_nextToken.type == TokenType::op_plusplus || m_nextToken.type == TokenType::op_minusminus) {
        SourceLocation location = m_nextToken.loc;
        auto type = m_nextToken.type;
        eat_next_token();  // eat '++' '--'

        expr = makePtr<UnaryOperator>(location, std::move(expr), type);
        return parse_postfix_expr(std::move(expr));
    }

    if (!(restrictions & OnlyTypeExpr) && m_nextToken.type == TokenType::kw_orelse) {
        eat_next_token();  // eat orelse
        varOrReturn(orelse, parse_expr());

        expr = makePtr<OrElseErrorExpr>(expr->location, std::move(expr), std::move(orelse));
        return parse_postfix_expr(std::move(expr));
    }
    if (!(restrictions & OnlyTypeExpr) && m_nextToken.type == TokenType::kw_catch) {
        expr = parse_catch_error_expr(std::move(expr));
        return parse_postfix_expr(std::move(expr));
    }

    return expr;
}

ptr<Expr> Parser::parse_prefix_expr() {
    debug_func(m_nextToken.loc << " '" << m_nextToken.str << "'" << m_nextToken.type);
    Token tok = m_nextToken;

    if (tok.type == TokenType::kw_comptime) {
        SourceLocation loc = m_nextToken.loc;
        eat_next_token();  // eat comptime
        varOrReturn(expr, parse_prefix_expr());
        return makePtr<ComptimeExpr>(loc, std::move(expr));
    }

    if (tok.type == TokenType::bracket_l) {
        SourceLocation loc = m_nextToken.loc;
        eat_next_token();      // eat [
        if (m_nextToken.type == TokenType::bracket_r) {
            eat_next_token();  // eat ]
            varOrReturn(rhs, parse_prefix_expr());
            if (restrictions & OnlyTypeExpr) {
                return makePtr<TypeSlice>(loc, std::move(rhs));
            }
            return report(loc, "unexpected '[]' in expression");
        } else {
            varOrReturn(size, with_no_restrictions<ptr<Expr>>([&]() { return parse_expr(); }));
            matchOrReturn(TokenType::bracket_r, "expected ']'");
            eat_next_token();  // eat ]
            varOrReturn(rhs, parse_prefix_expr());
            if (restrictions & OnlyTypeExpr) {
                return makePtr<TypeArray>(loc, std::move(rhs), std::move(size));
            }
            return report(loc, "unexpected '[size]' in expression");
        }
    }

    std::unordered_set<TokenType> unaryOps = {
        TokenType::op_minus, TokenType::amp, TokenType::op_excla_mark, TokenType::asterisk, TokenType::op_tilde,
    };

    if (unaryOps.count(tok.type) == 0) {
        varOrReturn(expr, parse_primary());
        return parse_postfix_expr(std::move(expr));
    }
    eat_next_token();  // eat unaryOps

    varOrReturn(rhs, parse_prefix_expr());

    if (tok.type == TokenType::asterisk) {
        return makePtr<TypePointer>(tok.loc, std::move(rhs));
    }
    switch (tok.type) {
        case TokenType::amp:
            return makePtr<RefPtrExpr>(tok.loc, std::move(rhs));
        case TokenType::op_excla_mark:
        case TokenType::op_minus:
        case TokenType::op_tilde:
            if (restrictions & OnlyTypeExpr) {
                if (tok.type == TokenType::op_excla_mark) {
                    return makePtr<TypeOptional>(tok.loc, std::move(rhs));
                }
            }

            return makePtr<UnaryOperator>(tok.loc, std::move(rhs), tok.type);
        default:
            return report(tok.loc, "unexpected unary operator");
    }
}

ptr<Expr> Parser::parse_type() {
    debug_func("");
    return with_restrictions<ptr<Expr>>(OnlyTypeExpr, [&]() { return parse_expr(); });
}

ptr<Expr> Parser::parse_expr() {
    debug_func("");
    varOrReturn(lhs, parse_prefix_expr());
    return parse_expr_rhs(std::move(lhs), 0);
}

int Parser::get_token_precedence(TokenType tok) {
    debug_func("");
    switch (tok) {
        case TokenType::asterisk:
        case TokenType::op_div:
        case TokenType::op_percent:
            return 9;
        case TokenType::op_plus:
        case TokenType::op_minus:
            return 8;
        case TokenType::op_shl:
        case TokenType::op_shr:
            return 7;
        case TokenType::op_less:
        case TokenType::op_more:
        case TokenType::op_less_eq:
        case TokenType::op_more_eq:
            return 6;
        case TokenType::op_equal:
        case TokenType::op_not_equal:
            return 5;
        case TokenType::amp:
            return 4;
        case TokenType::caret:
            return 3;
        case TokenType::pipe:
            return 2;
        case TokenType::ampamp:
            return 1;
        case TokenType::pipepipe:
            return 0;
        default:
            return -1;
    }
}

int Parser::get_current_binop_precedence(TokenType &outOp) {
    outOp = m_nextToken.type;
    if (outOp == TokenType::op_less && peek_token().type == TokenType::op_less) {
        outOp = TokenType::op_shl;
        return 7;
    }
    if (outOp == TokenType::op_more && peek_token().type == TokenType::op_more) {
        outOp = TokenType::op_shr;
        return 7;
    }
    return get_token_precedence(outOp);
}

ptr<Expr> Parser::parse_expr_rhs(ptr<Expr> lhs, int precedence) {
    debug_func("prec " << precedence << " " << m_nextToken.loc << " '" << m_nextToken.str << "'");
    if ((restrictions & OnlyTypeExpr)) return lhs;
    while (true) {
        TokenType opType;
        int curOpPrec = get_current_binop_precedence(opType);

        if (curOpPrec < precedence) return lhs;
        SourceLocation loc = m_nextToken.loc;
        eat_next_token();      // eat operator (first part if combined)
        if (opType == TokenType::op_shl || opType == TokenType::op_shr) {
            eat_next_token();  // eat second part
        }

        varOrReturn(rhs, parse_prefix_expr());

        TokenType nextOp;
        if (curOpPrec < get_current_binop_precedence(nextOp)) {
            rhs = parse_expr_rhs(std::move(rhs), curOpPrec + 1);
            if (!rhs) return nullptr;
        }

        lhs = makePtr<BinaryOperator>(loc, std::move(lhs), std::move(rhs), opType);
    }
}

ptr<Expr> Parser::parse_catch_error_expr(ptr<Expr> expr) {
    debug_func("");
    matchOrReturn(TokenType::kw_catch, "expected 'catch'");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat catch

    std::string captureIdentifier = "";
    if (m_nextToken.type == TokenType::pipe) {
        eat_next_token();  // eat |
        matchOrReturn(TokenType::id, "expected identifier");
        captureIdentifier = m_nextToken.str;
        eat_next_token();  // eat identifier
        matchOrReturn(TokenType::pipe, "expected '|'");
        eat_next_token();  // eat |
    }

    ptr<Stmt> handler;
    if (m_nextToken.type == TokenType::block_l) {
        handler = parse_block();
    } else {
        handler = parse_expr();
    }
    if (!handler) return nullptr;

    return makePtr<CatchErrorExpr>(location, std::move(expr), captureIdentifier, std::move(handler));
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

    return makePtr<ImportExpr>(location, identifier);
}
}  // namespace DMZ
