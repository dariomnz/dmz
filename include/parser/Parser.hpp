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

    enum RestrictionKind : RestrictionType { StructNotAllowed = 0b1, ReturnNotAllowed = 0b10, OnlyTypeExpr = 0b100 };

    std::string restiction_to_str(RestrictionType rests) const {
        return std::string(rests & StructNotAllowed ? "StructNotAllowed " : "") +
               std::string(rests & ReturnNotAllowed ? "ReturnNotAllowed" : "") +
               std::string(rests & OnlyTypeExpr ? "OnlyTypeExpr" : "");
    }

    template <typename T>
    T with_restrictions(RestrictionType rests, std::function<T()> func) {
        debug_func(restiction_to_str(rests));
        RestrictionType prevRestrictions = restrictions;
        restrictions |= rests;
        auto res = (func)();
        restrictions = prevRestrictions;
        return res;
    }

    template <typename T>
    T with_no_restrictions(std::function<T()> func) {
        debug_func("");
        RestrictionType prevRestrictions = restrictions;
        restrictions = 0;
        auto res = (func)();
        restrictions = prevRestrictions;
        return res;
    }
    std::deque<Token> m_peekedTokens;
    void eat_next_token() {
        debug_func("");
        do {
            if (!m_peekedTokens.empty()) {
                m_nextToken = m_peekedTokens.front();
                m_peekedTokens.pop_front();
            } else {
                m_nextToken = m_lexer.next_token();
            }
        } while (m_nextToken.type == TokenType::comment);
        debug_msg(m_nextToken.loc << " '" << m_nextToken.str << "'");
    }

    const Token &peek_token(size_t jump = 0) {
        debug_func("");
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
        debug_msg(m_peekedTokens[jump].loc << " '" << m_peekedTokens[jump].str << "'");
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
    std::pair<ptr<ModuleDecl>, bool> parse_source_file(bool expectMain = false);

   public:
    explicit Parser(Lexer &lexer) : m_lexer(lexer) { eat_next_token(); }

   private:
    bool nextToken_is_generic();
    ptr<FuncDecl> parse_function_decl();
    // ptr<Type> parse_type();
    ptr<GenericExpr> parse_generic_expr(ptr<Expr> &prevExpr);
    ptr<GenericTypeDecl> parse_generic_type_decl();
    std::vector<ptr<GenericTypeDecl>> parse_generic_types_decl();
    ptr<Block> parse_block(bool oneStmt = false);
    ptr<ReturnStmt> parse_return_stmt();
    ptr<Stmt> parse_statement();
    ptr<Expr> parse_primary();
    ptr<Expr> parse_postfix_expr(ptr<Expr> expr);
    template <typename T, typename F>
    ptr<std::vector<ptr<T>>> parse_list_with_trailing_comma(std::pair<TokenType, const char *> openingToken, F parser,
                                                            std::pair<TokenType, const char *> closingToken);
    ptr<std::vector<ptr<Expr>>> parse_expr_list_with_trailing_comma(std::pair<TokenType, const char *> openingToken,
                                                               std::function<ptr<Expr>()> parser,
                                                               std::pair<TokenType, const char *> closingToken);
    ptr<Expr> parse_prefix_expr();
    ptr<Expr> parse_expr();
    ptr<Expr> parse_expr_rhs(ptr<Expr> lhs, int precedence);
    ptr<ParamDecl> parse_param_decl();
    ptr<std::vector<ptr<ParamDecl>>> parse_parameter_list();
    ptr<IfStmt> parse_if_stmt();
    ptr<WhileStmt> parse_while_stmt();
    ptr<DeclStmt> parse_decl_stmt();
    ptr<VarDecl> parse_var_decl(bool isPublic, bool isConst);
    ptr<Stmt> parse_assignment_or_expr(bool expectSemicolon = true);
    ptr<Assignment> parse_assignment_rhs(ptr<AssignableExpr> lhs);
    ptr<StructDecl> parse_struct_decl();
    ptr<FieldDecl> parse_field_decl();
    ptr<FieldInitStmt> parse_field_init_stmt();
    ptr<DeferStmt> parse_defer_stmt();
    ptr<ErrorGroupExprDecl> parse_error_group_expr_decl();
    ptr<ErrorDecl> parse_error_decl();
    ptr<CatchErrorExpr> parse_catch_error_expr();
    ptr<TryErrorExpr> parse_try_error_expr();
    ptr<ModuleDecl> parse_module_decl();
    std::vector<ptr<Decl>> parse_in_module_decl();
    ptr<ImportExpr> parse_import_expr();
    ptr<SwitchStmt> parse_switch_stmt();
    ptr<CaseStmt> parse_case_stmt();
    ptr<TestDecl> parse_test_decl();
    ptr<SelfMemberExpr> parse_self_member_expr();
    ptr<SizeofExpr> parse_sizeof_expr();
};
}  // namespace DMZ
