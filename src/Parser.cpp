#include "Parser.hpp"

namespace C {

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

static const std::unordered_set<TokenType> top_level_tokens = {TokenType::eof, TokenType::kw_fn, TokenType::kw_struct,
                                                               TokenType::kw_extern};
[[maybe_unused]] static inline bool is_top_level_token(TokenType tok) { return top_level_tokens.count(tok) != 0; }

void Parser::synchronize_on(std::unordered_set<TokenType> types) {
    m_incompleteAST = true;

    while (types.count(m_nextToken.type) == 0 && m_nextToken.type != TokenType::eof) eat_next_token();
}

// <sourceFile>
//   ::= (<structDecl> | <functionDecl>)* EOF
std::pair<std::vector<std::unique_ptr<Decl>>, bool> Parser::parse_source_file() {
    ScopedTimer st(Stats::type::parseTime);
    std::vector<std::unique_ptr<Decl>> declarations;

    while (m_nextToken.type != TokenType::eof) {
        if (m_nextToken.type == TokenType::kw_extern || m_nextToken.type == TokenType::kw_fn) {
            if (auto fn = parse_function_decl()) {
                declarations.emplace_back(std::move(fn));
                continue;
            }
        } else if (m_nextToken.type == TokenType::kw_struct) {
            if (auto st = parse_struct_decl()) {
                declarations.emplace_back(std::move(st));
                continue;
            }
        } else {
            report(m_nextToken.loc, "expected function or struct declaration on the top level");
        }

        synchronize_on(top_level_tokens);
        continue;
    }

    bool hasMainFunction = false;
    for (auto &&fn : declarations) hasMainFunction |= fn->identifier == "main";

    if (!hasMainFunction && !m_incompleteAST) report(m_nextToken.loc, "main function not found");

    return {std::move(declarations), !m_incompleteAST && hasMainFunction};
}

// <functionDecl>
//  ::= 'extern'? 'fn' <identifier> '(' ')' '->' <type> <block>
std::unique_ptr<FuncDecl> Parser::parse_function_decl() {
    SourceLocation loc = m_nextToken.loc;
    bool isExtern = false;

    if (m_nextToken.type == TokenType::kw_extern) {
        isExtern = true;
        eat_next_token();  // eat extern
    }

    matchOrReturn(TokenType::kw_fn, "expected 'fn'");

    eat_next_token();  // eat fn

    matchOrReturn(TokenType::id, "expected identifier");

    std::string_view functionIdentifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    varOrReturn(parameterList,
                parse_list_with_trailing_comma<ParamDecl>({TokenType::par_l, "expected '('"}, &Parser::parse_param_decl,
                                                          {TokenType::par_r, "expected ')'"}));

    matchOrReturn(TokenType::return_type, "expected '->'");
    eat_next_token();  // eat '->'

    varOrReturn(type, parse_type());

    if (isExtern) {
        matchOrReturn(TokenType::semicolon, "expected ';'");
        eat_next_token();
        return std::make_unique<ExternFunctionDecl>(loc, functionIdentifier, *type, std::move(*parameterList));
    }

    matchOrReturn(TokenType::block_l, "expected function body");
    varOrReturn(block, parse_block());

    return std::make_unique<FunctionDecl>(loc, functionIdentifier, *type, std::move(*parameterList), std::move(block));
}

// <type>
//  ::= 'int'
//  |   'char'
//  |   'void'
//  |   <identifier>
std::optional<Type> Parser::parse_type() {
    TokenType type = m_nextToken.type;
    std::string_view name = m_nextToken.str;
    bool isSlice = false;
    std::unordered_set<TokenType> types = {
        TokenType::kw_void,
        TokenType::kw_int,
        TokenType::kw_char,
        TokenType::id,
    };

    if (types.count(type) == 0) {
        report(m_nextToken.loc, "expected type specifier");
        return std::nullopt;
    }
    eat_next_token();      // eat type

    if (m_nextToken.type == TokenType::bracket_l) {
        eat_next_token();  // eat '['
        if (m_nextToken.type != TokenType::bracket_r) {
            report(m_nextToken.loc, "expected ']' next to a '[' in a type");
            return std::nullopt;
        }
        eat_next_token();  // eat ']'
        isSlice = true;
    }
    Type t;
    if (type == TokenType::kw_void) {
        t = Type::builtinVoid();
    }
    if (type == TokenType::kw_char) {
        t = Type::builtinChar();
    }
    if (type == TokenType::kw_int) {
        t = Type::builtinInt();
    }

    if (type == TokenType::id) {
        t = Type::custom(name);
    }

    t.isSlice = isSlice;
    return t;
}

// <block>
//   ::= '{' <statement>* '}'
std::unique_ptr<Block> Parser::parse_block() {
    SourceLocation loc = m_nextToken.loc;
    eat_next_token();  // eat '{'

    std::vector<std::unique_ptr<Stmt>> statements;
    while (true) {
        if (m_nextToken.type == TokenType::block_r) break;

        if (is_top_level_token(m_nextToken.type)) return report(m_nextToken.loc, "expected '}' at the end of a block");

        auto stmt = parse_statement();
        if (!stmt) {
            synchronize();
            continue;
        }

        statements.emplace_back(std::move(stmt));
    }
    matchOrReturn(TokenType::block_r, "expected '}' at the end of a block");
    eat_next_token();  // eat '}'

    return std::make_unique<Block>(loc, std::move(statements));
}

// <returnStmt>
//   ::= 'return' <expr>? ';'
std::unique_ptr<ReturnStmt> Parser::parse_return_stmt() {
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat 'return'

    std::unique_ptr<Expr> expr;
    if (m_nextToken.type != TokenType::semicolon) {
        expr = parse_expr();
        if (!expr) return nullptr;
    }

    matchOrReturn(TokenType::semicolon, "expected ';' at the end of a return statement");
    eat_next_token();  // eat ';'

    return std::make_unique<ReturnStmt>(location, std::move(expr));
}

std::unique_ptr<Stmt> Parser::parse_statement() {
    if (m_nextToken.type == TokenType::kw_if) return parse_if_stmt();
    if (m_nextToken.type == TokenType::kw_while) return parse_while_stmt();
    if (m_nextToken.type == TokenType::kw_return) return parse_return_stmt();
    if (m_nextToken.type == TokenType::kw_let) return parse_decl_stmt();
    return parse_assignment_or_expr();
}

std::unique_ptr<Expr> Parser::parse_primary() {
    SourceLocation location = m_nextToken.loc;

    if (m_nextToken.type == TokenType::par_l) {
        eat_next_token();  // eat '('

        varOrReturn(expr, with_no_restrictions(&Parser::parse_expr));

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

    if (m_nextToken.type == TokenType::lit_string) {
        auto literal = std::make_unique<StringLiteral>(location, m_nextToken.str);
        eat_next_token();  // eat char
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

    return expr;
}

template <typename T, typename F>
std::unique_ptr<std::vector<std::unique_ptr<T>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *> openingToken, F parser, std::pair<TokenType, const char *> closingToken) {
    matchOrReturn(openingToken.first, openingToken.second);
    eat_next_token();  // eat openingToken

    std::vector<std::unique_ptr<T>> list;
    while (true) {
        if (m_nextToken.type == closingToken.first) break;

        varOrReturn(init, (this->*parser)());
        list.emplace_back(std::move(init));

        if (m_nextToken.type != TokenType::comma) break;
        eat_next_token();  // eat ','
    }

    matchOrReturn(closingToken.first, closingToken.second);
    eat_next_token();  // eat closingToken

    return std::make_unique<decltype(list)>(std::move(list));
}

std::unique_ptr<Expr> Parser::parse_prefix_expr() {
    Token tok = m_nextToken;

    if (tok.type != TokenType::op_minus && tok.type != TokenType::op_not) {
        return parse_postfix_expr();
    }
    eat_next_token();  // eat '!' or '-'

    varOrReturn(rhs, parse_prefix_expr());

    return std::make_unique<UnaryOperator>(tok.loc, std::move(rhs), tok.type);
}

std::unique_ptr<Expr> Parser::parse_expr() {
    varOrReturn(lhs, parse_prefix_expr());
    return parse_expr_rhs(std::move(lhs), 0);
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

// <paramDecl>
//  ::= 'const'? <identifier> ':' <type>
std::unique_ptr<ParamDecl> Parser::parse_param_decl() {
    SourceLocation location = m_nextToken.loc;

    bool isConst = m_nextToken.type == TokenType::kw_const;
    if (isConst) eat_next_token();  // eat 'const'

    matchOrReturn(TokenType::id, "expected parameter declaration");
    // assert(nextToken.value && "identifier token without value");

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat :

    varOrReturn(type, parse_type());

    return std::make_unique<ParamDecl>(location, std::move(identifier), std::move(*type), !isConst);
}

void Parser::synchronize() {
    m_incompleteAST = true;

    int blocks = 0;
    while (true) {
        TokenType type = m_nextToken.type;

        if (type == TokenType::block_l) {
            ++blocks;
        } else if (type == TokenType::block_r) {
            if (blocks == 0) break;
            if (blocks == 1) {
                eat_next_token();  // eat '}'
                break;
            }
            --blocks;
        } else if (type == TokenType::semicolon && blocks == 0) {
            eat_next_token();  // eat ';'
            break;
        } else if (is_top_level_token(type)) {
            break;
        }

        eat_next_token();
    }
}

std::unique_ptr<IfStmt> Parser::parse_if_stmt() {
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat 'if'

    varOrReturn(condition, with_restrictions(StructNotAllowed, &Parser::parse_expr));

    matchOrReturn(TokenType::block_l, "expected 'if' body");

    varOrReturn(trueBlock, parse_block());

    if (m_nextToken.type != TokenType::kw_else)
        return std::make_unique<IfStmt>(location, std::move(condition), std::move(trueBlock));
    eat_next_token();  // eat 'else'

    std::unique_ptr<Block> falseBlock;
    if (m_nextToken.type == TokenType::kw_if) {
        varOrReturn(elseIf, parse_if_stmt());

        SourceLocation loc = elseIf->location;
        std::vector<std::unique_ptr<Stmt>> stmts;
        stmts.emplace_back(std::move(elseIf));

        falseBlock = std::make_unique<Block>(loc, std::move(stmts));
    } else {
        matchOrReturn(TokenType::block_l, "expected 'else' body");
        falseBlock = parse_block();
    }

    if (!falseBlock) return nullptr;

    return std::make_unique<IfStmt>(location, std::move(condition), std::move(trueBlock), std::move(falseBlock));
}

std::unique_ptr<WhileStmt> Parser::parse_while_stmt() {
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat 'while'

    varOrReturn(cond, with_restrictions(StructNotAllowed, &Parser::parse_expr));

    matchOrReturn(TokenType::block_l, "expected 'while' body");
    varOrReturn(body, parse_block());

    return std::make_unique<WhileStmt>(location, std::move(cond), std::move(body));
}

std::unique_ptr<DeclStmt> Parser::parse_decl_stmt() {
    Token tok = m_nextToken;
    eat_next_token();  // eat 'let'

    bool isConst = false;
    if (m_nextToken.type == TokenType::kw_const) {
        eat_next_token();  // eat 'const'
        isConst = true;
    }

    matchOrReturn(TokenType::id, "expected identifier");
    varOrReturn(varDecl, parse_var_decl(isConst));

    matchOrReturn(TokenType::semicolon, "expected ';' after declaration");
    eat_next_token();  // eat ';'

    return std::make_unique<DeclStmt>(tok.loc, std::move(varDecl));
}

std::unique_ptr<VarDecl> Parser::parse_var_decl(bool isConst) {
    SourceLocation location = m_nextToken.loc;

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    std::optional<Type> type;
    if (m_nextToken.type == TokenType::colon) {
        eat_next_token();  // eat ':'

        type = parse_type();
        if (!type) return nullptr;
    }

    if (m_nextToken.type != TokenType::op_assign) {
        return std::make_unique<VarDecl>(location, identifier, type, !isConst);
    }
    eat_next_token();  // eat '='

    varOrReturn(initializer, parse_expr());

    return std::make_unique<VarDecl>(location, identifier, type, !isConst, std::move(initializer));
}

std::unique_ptr<Stmt> Parser::parse_assignment_or_expr() {
    varOrReturn(lhs, parse_prefix_expr());

    if (m_nextToken.type != TokenType::op_assign) {
        varOrReturn(expr, parse_expr_rhs(std::move(lhs), 0));

        matchOrReturn(TokenType::semicolon, "expected ';' at the end of expression");
        eat_next_token();  // eat ';'

        return expr;
    }
    auto *dre = dynamic_cast<AssignableExpr *>(lhs.get());
    if (!dre) return report(lhs->location, "expected variable on the LHS of an assignment");
    std::ignore = lhs.release();

    varOrReturn(assignment, parse_assignment_rhs(std::unique_ptr<AssignableExpr>(dre)));

    matchOrReturn(TokenType::semicolon, "expected ';' at the end of assignment");
    eat_next_token();  // eat ';'

    return assignment;
}

std::unique_ptr<Assignment> Parser::parse_assignment_rhs(std::unique_ptr<AssignableExpr> lhs) {
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat '='

    varOrReturn(rhs, parse_expr());

    return std::make_unique<Assignment>(location, std::move(lhs), std::move(rhs));
}

// <structDecl>
//  ::= 'struct' <identifier> <fieldList>
//
// <fieldList>
//  ::= '{' (<fieldDecl> (',' <fieldDecl>)* ','?)? '}'
std::unique_ptr<StructDecl> Parser::parse_struct_decl() {
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat struct

    matchOrReturn(TokenType::id, "expected identifier");

    std::string_view structIdentifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    varOrReturn(fieldList, parse_list_with_trailing_comma<FieldDecl>({TokenType::block_l, "expected '{'"},
                                                                     &Parser::parse_field_decl,
                                                                     {TokenType::block_r, "expected '}'"}));

    return std::make_unique<StructDecl>(location, structIdentifier, std::move(*fieldList));
}

// <fieldDecl>
//  ::= <identifier> ':' <type>
std::unique_ptr<FieldDecl> Parser::parse_field_decl() {
    matchOrReturn(TokenType::id, "expected field declaration");

    SourceLocation location = m_nextToken.loc;
    // assert(nextToken.value && "identifier token without value");

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat :

    varOrReturn(type, parse_type());

    return std::make_unique<FieldDecl>(location, std::move(identifier), std::move(*type));
};

// <fieldInit>
//  ::= <identifier> ':' <expr>
std::unique_ptr<FieldInitStmt> Parser::parse_field_init_stmt() {
    matchOrReturn(TokenType::id, "expected field initialization");

    SourceLocation location = m_nextToken.loc;
    // assert(m_nextToken.value && "identifier token without value");

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat ':'

    varOrReturn(init, parse_expr());

    return std::make_unique<FieldInitStmt>(location, std::move(identifier), std::move(init));
}
}  // namespace C