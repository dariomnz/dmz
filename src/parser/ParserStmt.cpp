#include <unordered_set>

#include "Utils.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "parser/ParserSymbols.hpp"

namespace DMZ {

// <returnStmt>
//   ::= 'return' <expr>? ';'
ptr<ReturnStmt> Parser::parse_return_stmt() {
    debug_func("");
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat 'return'
    if (restrictions & ReturnNotAllowed) {
        return report(location, "unexpected return statement");
    }

    ptr<Expr> expr;
    if (m_nextToken.type != TokenType::semicolon) {
        expr = parse_expr();
        if (!expr) return nullptr;
    }

    matchOrReturn(TokenType::semicolon, "expected ';' at the end of a return statement");
    eat_next_token();  // eat ';'

    return makePtr<ReturnStmt>(location, std::move(expr));
}

ptr<Stmt> Parser::parse_statement() {
    debug_func("");
    if (m_nextToken.type == TokenType::kw_if) return parse_if_stmt();
    if (m_nextToken.type == TokenType::kw_while) return parse_while_stmt();
    if (m_nextToken.type == TokenType::kw_for) return parse_for_stmt();
    if (m_nextToken.type == TokenType::kw_return) return parse_return_stmt();
    if (m_nextToken.type == TokenType::kw_let || m_nextToken.type == TokenType::kw_const) return parse_decl_stmt();
    if (m_nextToken.type == TokenType::kw_defer || m_nextToken.type == TokenType::kw_errdefer)
        return parse_defer_stmt();
    if (m_nextToken.type == TokenType::block_l) return parse_block();
    if (m_nextToken.type == TokenType::kw_switch) return parse_switch_stmt();
    return parse_assignment_or_expr();
}

// <block>
//   ::= '{' <statement>* '}'
ptr<Block> Parser::parse_block(bool oneStmt) {
    debug_func("");
    SourceLocation loc = m_nextToken.loc;
    if (!oneStmt) eat_next_token();  // eat '{'

    std::vector<ptr<Stmt>> statements;
    do {
        if (!oneStmt)
            if (m_nextToken.type == TokenType::block_r) break;

        if (!oneStmt)
            if (is_top_top_level_token(m_nextToken.type))
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

    return makePtr<Block>(loc, std::move(statements));
}

ptr<IfStmt> Parser::parse_if_stmt() {
    debug_func("");
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat 'if'

    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat '('

    varOrReturn(condition, with_restrictions<ptr<Expr>>(StructNotAllowed, [&]() { return parse_expr(); }));

    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();  // eat ')'

    matchOrReturn(TokenType::block_l, "expected 'if' body");

    varOrReturn(trueBlock, parse_block());

    if (m_nextToken.type != TokenType::kw_else)
        return makePtr<IfStmt>(location, std::move(condition), std::move(trueBlock));
    eat_next_token();  // eat 'else'

    ptr<Block> falseBlock;
    if (m_nextToken.type == TokenType::kw_if) {
        varOrReturn(elseIf, parse_if_stmt());

        SourceLocation loc = elseIf->location;
        std::vector<ptr<Stmt>> stmts;
        stmts.emplace_back(std::move(elseIf));

        falseBlock = makePtr<Block>(loc, std::move(stmts));
    } else {
        matchOrReturn(TokenType::block_l, "expected 'else' body");
        falseBlock = parse_block();
    }

    if (!falseBlock) return nullptr;

    return makePtr<IfStmt>(location, std::move(condition), std::move(trueBlock), std::move(falseBlock));
}

ptr<WhileStmt> Parser::parse_while_stmt() {
    debug_func("");
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat 'while'

    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat '('

    varOrReturn(cond, with_restrictions<ptr<Expr>>(StructNotAllowed, [&]() { return parse_expr(); }));

    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();  // eat ')'

    matchOrReturn(TokenType::block_l, "expected 'while' body");
    varOrReturn(body, parse_block());

    return makePtr<WhileStmt>(location, std::move(cond), std::move(body));
}

ptr<ForStmt> Parser::parse_for_stmt() {
    debug_func("");
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat 'for'

    varOrReturn(conditions,
                parse_expr_list_with_trailing_comma({TokenType::par_l, "expected '('"}, [&]() { return parse_expr(); },
                                                    {TokenType::par_r, "expected ')'"}));
    varOrReturn(captures, parse_list_with_trailing_comma<CaptureDecl>({TokenType ::pipe, "expected '|'"},
                                                                      &Parser::parse_capture_decl,
                                                                      {TokenType ::pipe, "expected '|'"}));

    matchOrReturn(TokenType::block_l, "expected 'for' body");
    varOrReturn(body, parse_block());

    return makePtr<ForStmt>(location, std::move(*conditions), std::move(*captures), std::move(body));
}

ptr<DeclStmt> Parser::parse_decl_stmt() {
    debug_func("");
    Token tok = m_nextToken;
    bool isPublic = false;
    if (m_nextToken.type == TokenType::kw_pub) {
        eat_next_token();  // eat 'pub'
        isPublic = true;
    }
    bool isConst = m_nextToken.type == TokenType::kw_const;
    eat_next_token();  // eat 'let' or 'const'

    matchOrReturn(TokenType::id, "expected identifier");
    varOrReturn(varDecl, parse_var_decl(isPublic, isConst));

    matchOrReturn(TokenType::semicolon, "expected ';' after declaration");
    eat_next_token();  // eat ';'

    return makePtr<DeclStmt>(tok.loc, std::move(varDecl));
}

ptr<Stmt> Parser::parse_assignment_or_expr(bool expectSemicolon) {
    debug_func("");
    varOrReturn(lhs, parse_prefix_expr());

    std::unordered_set<TokenType> assing_ops = {
        TokenType::op_assign,         TokenType::op_plus_equal, TokenType::op_minus_equal,
        TokenType::op_asterisk_equal, TokenType::op_div_equal,
    };

    if (assing_ops.count(m_nextToken.type) == 0) {
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

    varOrReturn(assignment, parse_assignment_rhs(ptr<AssignableExpr>(dre)));

    if (expectSemicolon) {
        matchOrReturn(TokenType::semicolon, "expected ';' at the end of assignment");
        eat_next_token();  // eat ';'
    }

    return assignment;
}

ptr<Assignment> Parser::parse_assignment_rhs(ptr<AssignableExpr> lhs) {
    debug_func("");
    SourceLocation location = m_nextToken.loc;
    auto type = m_nextToken.type;
    eat_next_token();  // eat type

    varOrReturn(rhs, parse_expr());

    if (type == TokenType::op_assign) {
        return makePtr<Assignment>(location, std::move(lhs), std::move(rhs));
    } else {
        return makePtr<AssignmentOperator>(location, std::move(lhs), std::move(rhs), type);
    }
}

// <fieldInit>
//  ::= <identifier> ':' <expr>
ptr<FieldInitStmt> Parser::parse_field_init_stmt() {
    debug_func("");
    matchOrReturn(TokenType::id, "expected field initialization");

    SourceLocation location = m_nextToken.loc;
    // assert(m_nextToken.value && "identifier token without value");

    auto identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat ':'

    varOrReturn(init, parse_expr());

    return makePtr<FieldInitStmt>(location, std::move(identifier), std::move(init));
}

ptr<DeferStmt> Parser::parse_defer_stmt() {
    debug_func("");
    if (m_nextToken.type != TokenType::kw_errdefer && m_nextToken.type != TokenType::kw_defer) {
        return report(m_nextToken.loc, "expected defer");
    }
    bool isErrDefer = m_nextToken.type == TokenType::kw_errdefer;
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat defer or errdefer

    varOrReturn(block, with_restrictions<ptr<Block>>(
                           ReturnNotAllowed, [&]() { return parse_block(m_nextToken.type != TokenType::block_l); }));

    return makePtr<DeferStmt>(location, std::move(block), isErrDefer);
}

ptr<SwitchStmt> Parser::parse_switch_stmt() {
    debug_func("");
    matchOrReturn(TokenType::kw_switch, "expected switch");
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat switch

    matchOrReturn(TokenType::par_l, "expected '('");
    eat_next_token();  // eat '('

    varOrReturn(condition, with_restrictions<ptr<Expr>>(StructNotAllowed, [&]() { return parse_expr(); }));

    matchOrReturn(TokenType::par_r, "expected ')'");
    eat_next_token();  // eat ')'

    matchOrReturn(TokenType::block_l, "expected '{'");
    eat_next_token();  // eat '{'

    std::vector<ptr<CaseStmt>> cases;
    ptr<Block> elseBlock;
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

    return makePtr<SwitchStmt>(location, std::move(condition), std::move(cases), std::move(elseBlock));
}

ptr<CaseStmt> Parser::parse_case_stmt() {
    debug_func("");
    auto location = m_nextToken.loc;
    if (m_nextToken.type != TokenType::kw_case && m_nextToken.type != TokenType::kw_else) {
        return report(location, "expected case or else");
    }
    bool isElse = m_nextToken.type == TokenType::kw_else;
    eat_next_token();  // eat case or else

    ptr<Expr> condition;
    if (!isElse) {
        auto conditionLocation = m_nextToken.loc;
        condition = parse_expr();
        if (!condition) return report(conditionLocation, "expected expresion");
    }

    matchOrReturn(TokenType::switch_arrow, "expected '=>'");
    eat_next_token();  // eat =>

    varOrReturn(block, parse_block(m_nextToken.type != TokenType::block_l));

    return makePtr<CaseStmt>(location, std::move(condition), std::move(block));
}
}  // namespace DMZ