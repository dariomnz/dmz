#pragma once

#include <unordered_set>

#include "Utils.hpp"
#include "lexer/Lexer.hpp"
#include "parser/ParserSymbols.hpp"

namespace DMZ {

class Parser {
   private:
    Lexer &m_lexer;
    Token m_nextToken;
    bool m_incompleteAST = false;

    using RestrictionType = unsigned char;
    RestrictionType restrictions = 0;

    enum RestrictionKind : RestrictionType { StructNotAllowed = 0b1, ReturnNotAllowed = 0b10 };

    template <typename T>
    T with_restrictions(RestrictionType rests, std::function<T()> func) {
        restrictions |= rests;
        auto res = (func)();
        restrictions &= ~rests;
        return res;
    }

    template <typename T>
    T with_no_restrictions(std::function<T()> func) {
        RestrictionType prevRestrictions = restrictions;
        restrictions = 0;
        auto res = (func)();
        restrictions = prevRestrictions;
        return res;
    }

    void eat_next_token() {
        do {
            m_nextToken = m_lexer.next_token();
        } while (m_nextToken.type == TokenType::comment);
    }
    void synchronize_on(std::unordered_set<TokenType> types);
    void synchronize();

   public:
    std::pair<std::vector<std::unique_ptr<Decl>>, bool> parse_source_file();

   public:
    explicit Parser(Lexer &lexer) : m_lexer(lexer) { eat_next_token(); }

   private:
    std::unique_ptr<FuncDecl> parse_function_decl();
    std::optional<Type> parse_type();
    std::unique_ptr<Block> parse_block(bool oneStmt = false);
    std::unique_ptr<ReturnStmt> parse_return_stmt();
    std::unique_ptr<Stmt> parse_statement();
    std::unique_ptr<Expr> parse_primary();
    std::unique_ptr<Expr> parse_postfix_expr();
    template <typename T, typename F>
    std::unique_ptr<std::vector<std::unique_ptr<T>>> parse_list_with_trailing_comma(
        std::pair<TokenType, const char *> openingToken, F parser, std::pair<TokenType, const char *> closingToken);
    std::unique_ptr<Expr> parse_prefix_expr();
    std::unique_ptr<Expr> parse_expr();
    std::unique_ptr<Expr> parse_expr_rhs(std::unique_ptr<Expr> lhs, int precedence);
    std::unique_ptr<ParamDecl> parse_param_decl();
    std::unique_ptr<std::vector<std::unique_ptr<ParamDecl>>> parse_parameter_list();
    std::unique_ptr<IfStmt> parse_if_stmt();
    std::unique_ptr<WhileStmt> parse_while_stmt();
    std::unique_ptr<DeclStmt> parse_decl_stmt();
    std::unique_ptr<VarDecl> parse_var_decl(bool isLet);
    std::unique_ptr<Stmt> parse_assignment_or_expr(bool expectSemicolon = true);
    std::unique_ptr<Assignment> parse_assignment_rhs(std::unique_ptr<AssignableExpr> lhs);
    std::unique_ptr<StructDecl> parse_struct_decl();
    std::unique_ptr<FieldDecl> parse_field_decl();
    std::unique_ptr<FieldInitStmt> parse_field_init_stmt();
    std::unique_ptr<DeferStmt> parse_defer_stmt();
    std::unique_ptr<ErrGroupDecl> parse_err_group_decl();
    std::unique_ptr<ErrDecl> parse_err_decl();
    std::unique_ptr<CatchErrExpr> parse_catch_err_expr();
    std::unique_ptr<TryErrExpr> parse_try_err_expr();
};
}  // namespace DMZ
