#pragma once

#include "DMZPCH.hpp"

namespace DMZ {

enum class TokenType {
    invalid,
    comment,
    id,
    return_arrow,
    switch_arrow,
    lit_int,
    lit_float,
    lit_char,
    lit_string,
    op_plus,
    op_minus,
    asterisk,
    op_div,
    op_percent,
    ampamp,
    pipepipe,
    op_less,
    op_more,
    op_less_eq,
    op_more_eq,
    op_equal,
    op_not_equal,
    op_assign,
    op_quest_mark,
    op_excla_mark,
    amp,
    pipe,
    block_l,
    block_r,
    par_l,
    par_r,
    bracket_l,
    bracket_r,
    colon,
    semicolon,
    comma,
    dot,
    dotdotdot,
    ty_void,
    ty_f16,
    ty_f32,
    ty_f64,
    ty_iN,
    ty_uN,
    ty_bool,
    kw_true,
    kw_false,
    kw_null,
    kw_fn,
    kw_if,
    kw_else,
    kw_let,
    kw_const,
    kw_while,
    kw_return,
    kw_struct,
    kw_extern,
    kw_defer,
    kw_errdefer,
    kw_error,
    kw_catch,
    kw_try,
    kw_orelse,
    kw_module,
    kw_import,
    kw_switch,
    kw_case,
    kw_test,
    kw_static,
    kw_sizeof,
    kw_packed,
    kw_pub,
    unknown,
    eof,
};
std::ostream& operator<<(std::ostream& os, const TokenType& t);

const static std::unordered_map<std::string, TokenType> keywords = {
    {"fn", TokenType::kw_fn},
    {"if", TokenType::kw_if},
    {"else", TokenType::kw_else},
    {"let", TokenType::kw_let},
    {"const", TokenType::kw_const},
    {"while", TokenType::kw_while},
    {"return", TokenType::kw_return},
    {"struct", TokenType::kw_struct},
    {"extern", TokenType::kw_extern},
    {"defer", TokenType::kw_defer},
    {"errdefer", TokenType::kw_errdefer},
    {"error", TokenType::kw_error},
    {"catch", TokenType::kw_catch},
    {"try", TokenType::kw_try},
    {"orelse", TokenType::kw_orelse},
    {"module", TokenType::kw_module},
    {"import", TokenType::kw_import},
    {"switch", TokenType::kw_switch},
    {"case", TokenType::kw_case},
    {"test", TokenType::kw_test},
    {"static", TokenType::kw_static},
    {"@sizeof", TokenType::kw_sizeof},
    {"packed", TokenType::kw_packed},
    {"pub", TokenType::kw_pub},
    // Types
    {"void", TokenType::ty_void},
    {"f16", TokenType::ty_f16},
    {"f32", TokenType::ty_f32},
    {"f64", TokenType::ty_f64},
    {"bool", TokenType::ty_bool},
    {"true", TokenType::kw_true},
    {"false", TokenType::kw_false},
    {"null", TokenType::kw_null},
};

struct Token {
    TokenType type = TokenType::invalid;
    std::string str = {};
    SourceLocation loc = {};
};

std::ostream& operator<<(std::ostream& os, const Token& t);
std::ostream& operator<<(std::ostream& os, const std::vector<Token>& v_t);

class Lexer {
   public:
    Lexer(std::filesystem::path file_path);
    std::vector<Token> tokenize_file();
    bool next_line();
    Token next_token();
    std::string get_file_name() { return m_file_path.filename().string(); }

    std::filesystem::path get_file_path() { return m_file_path; }

   private:
    bool advance(int num = 1);

   private:
    std::filesystem::path m_file_path = {};
    std::ifstream m_file = {};
    std::string m_line_buffer = "";
    size_t m_line = 0;
    size_t m_col = 0;
};
}  // namespace DMZ
