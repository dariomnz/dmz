#include "parser/Parser.hpp"

namespace DMZ {

// <returnStmt>
//   ::= 'return' <expr>? ';'
std::unique_ptr<ReturnStmt> Parser::parse_return_stmt() {
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat 'return'
    if (restrictions & ReturnNotAllowed) {
        return report(location, "unexpected return statement");
    }

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
    if (m_nextToken.type == TokenType::kw_defer) return parse_defer_stmt();
    if (m_nextToken.type == TokenType::block_l) return parse_block();
    if (m_nextToken.type == TokenType::kw_switch) return parse_switch_stmt();
    return parse_assignment_or_expr();
}

// <block>
//   ::= '{' <statement>* '}'
std::unique_ptr<Block> Parser::parse_block(bool oneStmt) {
    SourceLocation loc = m_nextToken.loc;
    if (!oneStmt) eat_next_token();  // eat '{'

    std::vector<std::unique_ptr<Stmt>> statements;
    do {
        if (!oneStmt)
            if (m_nextToken.type == TokenType::block_r) break;

        if (!oneStmt)
            if (is_top_level_token(m_nextToken.type))
                return report(m_nextToken.loc, "expected '}' at the end of a block");

        auto stmt = parse_statement();
        if (!stmt) {
            synchronize();
            continue;
        }

        statements.emplace_back(std::move(stmt));
    } while (!oneStmt);
    if (!oneStmt) {
        matchOrReturn(TokenType::block_r, "expected '}' at the end of a block");
        eat_next_token();  // eat '}'
    }

    return std::make_unique<Block>(loc, std::move(statements));
}

std::unique_ptr<IfStmt> Parser::parse_if_stmt() {
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat 'if'

    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat '('

    varOrReturn(condition, with_restrictions<std::unique_ptr<Expr>>(StructNotAllowed, [&]() { return parse_expr(); }));

    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();  // eat ')'

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

    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat '('

    varOrReturn(cond, with_restrictions<std::unique_ptr<Expr>>(StructNotAllowed, [&]() { return parse_expr(); }));

    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();  // eat ')'

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

std::unique_ptr<Stmt> Parser::parse_assignment_or_expr(bool expectSemicolon) {
    varOrReturn(lhs, parse_prefix_expr());

    if (m_nextToken.type != TokenType::op_assign) {
        varOrReturn(expr, parse_expr_rhs(std::move(lhs), 0));

        if (expectSemicolon) {
            matchOrReturn(TokenType::semicolon, "expected ';' at the end of expression");
            eat_next_token();  // eat ';'
        }

        return expr;
    }
    auto *dre = dynamic_cast<AssignableExpr *>(lhs.get());
    if (!dre) return report(lhs->location, "expected variable on the LHS of an assignment");
    std::ignore = lhs.release();

    varOrReturn(assignment, parse_assignment_rhs(std::unique_ptr<AssignableExpr>(dre)));

    if (expectSemicolon) {
        matchOrReturn(TokenType::semicolon, "expected ';' at the end of assignment");
        eat_next_token();  // eat ';'
    }

    return assignment;
}

std::unique_ptr<Assignment> Parser::parse_assignment_rhs(std::unique_ptr<AssignableExpr> lhs) {
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat '='

    varOrReturn(rhs, parse_expr());

    return std::make_unique<Assignment>(location, std::move(lhs), std::move(rhs));
}

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

std::unique_ptr<DeferStmt> Parser::parse_defer_stmt() {
    matchOrReturn(TokenType::kw_defer, "expected defer");
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat defer

    varOrReturn(block, with_restrictions<std::unique_ptr<Block>>(
                           ReturnNotAllowed, [&]() { return parse_block(m_nextToken.type != TokenType::block_l); }));

    return std::make_unique<DeferStmt>(location, std::move(block));
}

std::unique_ptr<SwitchStmt> Parser::parse_switch_stmt() {
    matchOrReturn(TokenType::kw_switch, "expected switch");
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat switch

    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat '('

    varOrReturn(condition, with_restrictions<std::unique_ptr<Expr>>(StructNotAllowed, [&]() { return parse_expr(); }));

    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();  // eat ')'

    matchOrReturn(TokenType::block_l, "expected '{'");
    eat_next_token();  // eat '{'

    std::vector<std::unique_ptr<CaseStmt>> cases;
    std::unique_ptr<Block> elseBlock;
    while (m_nextToken.type == TokenType::kw_case || m_nextToken.type == TokenType::kw_else) {
        bool isElse = m_nextToken.type == TokenType::kw_else;
        varOrReturn(cas, parse_case_stmt());

        if (!isElse) {
            cases.emplace_back(std::move(cas));
        } else {
            if (elseBlock) {
                synchronize_on({TokenType::block_r});
                eat_next_token();  // eat '}'
                return report(cas->location, "only one else is permited");
            }
            elseBlock = std::move(cas->block);
        }
    }

    if (!elseBlock) {
        synchronize_on({TokenType::block_r});
        eat_next_token();  // eat '}'
        return report(location, "expected a else case");
    }

    matchOrReturn(TokenType::block_r, "expected '}'");
    eat_next_token();  // eat '}'

    return std::make_unique<SwitchStmt>(location, std::move(condition), std::move(cases), std::move(elseBlock));
}

std::unique_ptr<CaseStmt> Parser::parse_case_stmt() {
    auto location = m_nextToken.loc;
    if (m_nextToken.type != TokenType::kw_case && m_nextToken.type != TokenType::kw_else) {
        return report(location, "expected case or else");
    }
    bool isElse = m_nextToken.type == TokenType::kw_else;
    eat_next_token();  // eat case or else

    std::unique_ptr<Expr> condition;
    if (!isElse) {
        auto conditionLocation = m_nextToken.loc;
        condition = parse_expr();
        if (!condition) return report(conditionLocation, "expected expresion");
    }

    matchOrReturn(TokenType::switch_arrow, "expected '=>'");
    eat_next_token();  // eat =>

    varOrReturn(block, parse_block(m_nextToken.type != TokenType::block_l));

    return std::make_unique<CaseStmt>(location, std::move(condition), std::move(block));
}
}  // namespace DMZ