#pragma once

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "Debug.hpp"
#include "lexer/Lexer.hpp"

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
    std::deque<Token> m_peekedTokens;
    void eat_next_token() {
        do {
            if (!m_peekedTokens.empty()) {
                m_nextToken = m_peekedTokens.front();
                m_peekedTokens.pop_front();
            } else {
                m_nextToken = m_lexer.next_token();
            }
        } while (m_nextToken.type == TokenType::comment);
    }

    const Token &peek_token(size_t jump = 0) {
        while (m_peekedTokens.size() <= jump) {
            Token nextLexerToken = m_lexer.next_token();
            if (nextLexerToken.type != TokenType::comment) {
                m_peekedTokens.push_back(nextLexerToken);
            }
            if (nextLexerToken.type == TokenType::eof) {
                break;
            }
        }

        if (jump >= m_peekedTokens.size()) {
            static const Token eof_token = {TokenType::eof, ""};
            return eof_token;
        }

        return m_peekedTokens[jump];
    }
    // void eat_next_token() {
    //     do {
    //         m_nextToken = m_lexer.next_token();
    //     } while (m_nextToken.type == TokenType::comment);
    // }

    void synchronize_on(std::unordered_set<TokenType> types);
    void synchronize();

    const std::unordered_set<TokenType> top_level_tokens = {
        TokenType::eof,       TokenType::kw_fn,     TokenType::kw_struct, TokenType::kw_packed,
        TokenType::kw_extern, TokenType::kw_module, TokenType::kw_const,  TokenType::kw_let,
    };
    const std::unordered_set<TokenType> top_top_level_tokens = {
        TokenType::eof,       TokenType::kw_fn,     TokenType::kw_struct,
        TokenType::kw_packed, TokenType::kw_extern, TokenType::kw_module,
    };
    const std::unordered_set<TokenType> top_stmt_level_tokens = {
        TokenType::kw_if,    TokenType::kw_while, TokenType::kw_return, TokenType::kw_let,
        TokenType::kw_const, TokenType::kw_defer, TokenType::kw_switch,
    };

    bool is_top_level_token(TokenType tok);
    bool is_top_top_level_token(TokenType tok);
    bool is_top_stmt_level_token(TokenType tok);

   public:
    std::pair<std::unique_ptr<ModuleDecl>, bool> parse_source_file(bool expectMain = false);

   public:
    explicit Parser(Lexer &lexer) : m_lexer(lexer) { eat_next_token(); }

   private:
    bool nextToken_is_generic(TokenType nextToken);
    std::unique_ptr<FuncDecl> parse_function_decl();
    std::unique_ptr<Type> parse_type();
    std::unique_ptr<GenericTypes> parse_generic_types();
    std::unique_ptr<GenericTypeDecl> parse_generic_type_decl();
    std::vector<std::unique_ptr<GenericTypeDecl>> parse_generic_types_decl();
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
    std::unique_ptr<ErrorGroupExprDecl> parse_error_group_expr_decl();
    std::unique_ptr<ErrorDecl> parse_error_decl();
    std::unique_ptr<CatchErrorExpr> parse_catch_error_expr();
    std::unique_ptr<TryErrorExpr> parse_try_error_expr();
    std::unique_ptr<ModuleDecl> parse_module_decl();
    std::vector<std::unique_ptr<Decl>> parse_in_module_decl();
    std::unique_ptr<ImportExpr> parse_import_expr();
    std::unique_ptr<SwitchStmt> parse_switch_stmt();
    std::unique_ptr<CaseStmt> parse_case_stmt();
    std::unique_ptr<TestDecl> parse_test_decl();
    std::unique_ptr<SelfMemberExpr> parse_self_member_expr();
    std::unique_ptr<SizeofExpr> parse_sizeof_expr();
};
}  // namespace DMZ
