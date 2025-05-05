#pragma once

#include "AST.hpp"
#include "Lexer.hpp"
#include "PH.hpp"
#include "Utils.hpp"

namespace C {

class Parser {
   private:
    Lexer &m_lexer;
    Token m_nextToken;
    bool m_incompleteAST = false;

    void eat_next_token() {
        do {
            m_nextToken = m_lexer.next_token();
        } while (m_nextToken.type == TokenType::comment);
    }
    void synchronize_on(std::unordered_set<TokenType> types);
    void synchronize();

   public:
    std::pair<std::vector<std::unique_ptr<FunctionDecl>>, bool> parse_source_file();

   public:
    explicit Parser(Lexer &lexer) : m_lexer(lexer) { eat_next_token(); }

   private:
    std::unique_ptr<FunctionDecl> parse_function_decl();
    std::optional<Type> parse_type();
    std::unique_ptr<Block> parse_block();
    std::unique_ptr<ReturnStmt> parse_return_stmt();
    std::unique_ptr<Statement> parse_statement();
    std::unique_ptr<Expr> parse_primary();
    std::unique_ptr<Expr> parse_postfix_expr();
    std::unique_ptr<std::vector<std::unique_ptr<Expr>>> parse_argument_list();
    std::unique_ptr<Expr> parse_prefix_expr();
    std::unique_ptr<Expr> parse_expr();
    std::unique_ptr<Expr> parse_expr_rhs(std::unique_ptr<Expr> lhs, int precedence);
    std::unique_ptr<ParamDecl> parse_param_decl();
    std::unique_ptr<std::vector<std::unique_ptr<ParamDecl>>> parse_parameter_list();
    std::unique_ptr<IfStmt> parse_if_stmt();
    std::unique_ptr<WhileStmt> parse_while_stmt();
    std::unique_ptr<DeclStmt> parse_decl_stmt();
    std::unique_ptr<VarDecl> parse_var_decl(bool isLet);
    std::unique_ptr<Statement> parse_assignment_or_expr();
    std::unique_ptr<Assignment> parse_assignment_rhs(std::unique_ptr<DeclRefExpr> lhs);
};
}  // namespace C
