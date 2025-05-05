#include "Parser.hpp"

namespace C {

[[maybe_unused]] static inline int get_token_precedence(TokenType tok) {
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
            return 3;
        case TokenType::op_and:
            return 2;
        case TokenType::op_or:
            return 1;
        default:
            return -1;
    }
}

void Parser::synchronize_on(std::unordered_set<TokenType> types) {
    m_incompleteAST = true;

    while (types.count(m_nextToken.type) != 0 && m_nextToken.type != TokenType::eof) eat_next_token();
}

// <sourceFile>
//  ::= <functionDecl>* EOF
std::pair<std::vector<std::unique_ptr<FunctionDecl>>, bool> Parser::parse_source_file() {
    std::vector<std::unique_ptr<FunctionDecl>> functions;

    while (m_nextToken.type != TokenType::eof) {
        if (m_nextToken.type != TokenType::kw_fn) {
            report(m_nextToken.loc, "only function declarations are allowed on the top level");
            synchronize_on({TokenType::kw_fn});
            continue;
        }

        auto fn = parse_function_decl();
        if (!fn) {
            synchronize_on({TokenType::kw_fn});
            continue;
        }
        functions.emplace_back(std::move(fn));
    }

    bool hasMainFunction = false;
    for (auto &&fn : functions) hasMainFunction |= fn->identifier == "main";

    if (!hasMainFunction && !m_incompleteAST) report(m_nextToken.loc, "main function not found");

    return {std::move(functions), !m_incompleteAST && hasMainFunction};
}

// <functionDecl>
//  ::= 'fn' <identifier> '(' ')' '->' <type> <block>
std::unique_ptr<FunctionDecl> Parser::parse_function_decl() {
    SourceLocation loc = m_nextToken.loc;
    eat_next_token();  // eat fn

    matchOrReturn(TokenType::id, "expected identifier");

    std::string_view functionIdentifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    varOrReturn(parameterList, parse_parameter_list());

    matchOrReturn(TokenType::return_type, "expected '->'");
    eat_next_token();  // eat '->'

    varOrReturn(type, parse_type());

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

    if (type == TokenType::kw_void) {
        eat_next_token();  // eat 'void'
        return Type::builtinVoid();
    }
    if (type == TokenType::kw_char) {
        eat_next_token();  // eat 'void'
        return Type::builtinChar();
    }
    if (type == TokenType::kw_int) {
        eat_next_token();  // eat 'void'
        return Type::builtinInt();
    }

    if (type == TokenType::id) {
        auto t = Type::custom(std::string(m_nextToken.str));
        eat_next_token();  // eat identifier
        return t;
    }

    report(m_nextToken.loc, "expected type specifier");
    return std::nullopt;
}

// <block>
//   ::= '{' <statement>* '}'
std::unique_ptr<Block> Parser::parse_block() {
    SourceLocation loc = m_nextToken.loc;
    eat_next_token();  // eat '{'

    std::vector<std::unique_ptr<Statement>> statements;
    while (true) {
        if (m_nextToken.type == TokenType::block_r) break;

        if (m_nextToken.type == TokenType::eof || m_nextToken.type == TokenType::kw_fn)
            return report(m_nextToken.loc, "expected '}' at the end of a block");

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

std::unique_ptr<Statement> Parser::parse_statement() {
    if (m_nextToken.type == TokenType::kw_return) return parse_return_stmt();

    varOrReturn(expr, parse_expr());

    matchOrReturn(TokenType::semicolon, "expected ';' at the end of expression");
    eat_next_token();  // eat ';'

    return expr;
}

std::unique_ptr<Expr> Parser::parse_primary() {
    SourceLocation location = m_nextToken.loc;

    if (m_nextToken.type == TokenType::lit_int) {
        auto literal = std::make_unique<NumberLiteral>(location, m_nextToken.str);
        eat_next_token();  // eat number
        return literal;
    }

    if (m_nextToken.type == TokenType::id) {
        auto declRefExpr = std::make_unique<DeclRefExpr>(location, m_nextToken.str);
        eat_next_token();  // eat identifier
        return declRefExpr;
    }

    if (m_nextToken.type == TokenType::par_l) {
        eat_next_token();  // eat '('

        varOrReturn(expr, parse_expr());

        matchOrReturn(TokenType::par_r, "expected ')'");
        eat_next_token();  // eat ')'

        return std::make_unique<GroupingExpr>(location, std::move(expr));
    }

    return report(location, "expected expression");
}

std::unique_ptr<Expr> Parser::parse_postfix_expr() {
    varOrReturn(expr, parse_primary());

    if (m_nextToken.type != TokenType::par_l) return expr;

    SourceLocation location = m_nextToken.loc;
    varOrReturn(argumentList, parse_argument_list());

    return std::make_unique<CallExpr>(location, std::move(expr), std::move(*argumentList));
}

std::unique_ptr<std::vector<std::unique_ptr<Expr>>> Parser::parse_argument_list() {
    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat '('

    std::vector<std::unique_ptr<Expr>> argumentList;
    while (true) {
        if (m_nextToken.type == TokenType::par_r) break;

        varOrReturn(expr, parse_expr());
        argumentList.emplace_back(std::move(expr));

        if (m_nextToken.type != TokenType::comma) break;
        eat_next_token();  // eat ','
    }
    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();      // eat ')'

    return std::make_unique<std::vector<std::unique_ptr<Expr>>>(std::move(argumentList));
}

std::unique_ptr<Expr> Parser::parse_prefix_expr() {
    Token tok = m_nextToken;

    if (tok.type != TokenType::op_minus || tok.type != TokenType::op_not) {
        return parse_postfix_expr();
    }
    eat_next_token();  // eat '-'

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

std::unique_ptr<ParamDecl> Parser::parse_param_decl() {
    SourceLocation location = m_nextToken.loc;

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat :

    varOrReturn(type, parse_type());

    return std::make_unique<ParamDecl>(location, std::move(identifier), std::move(*type));
}

std::unique_ptr<std::vector<std::unique_ptr<ParamDecl>>> Parser::parse_parameter_list() {
    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat '('
    std::vector<std::unique_ptr<ParamDecl>> parameterList;

    while (true) {
        if (m_nextToken.type == TokenType::par_r) break;

        matchOrReturn(TokenType::id, "expected parameter declaration");

        varOrReturn(paramDecl, parse_param_decl());
        parameterList.emplace_back(std::move(paramDecl));

        if (m_nextToken.type != TokenType::comma) break;
        eat_next_token();  // eat ','
    }
    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();      // eat ')'

    return std::make_unique<std::vector<std::unique_ptr<ParamDecl>>>(std::move(parameterList));
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
        } else if (type == TokenType::kw_fn || type == TokenType::eof) {
            break;
        }
    }
}
}  // namespace C