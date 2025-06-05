#pragma once

#include "Utils.hpp"

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
    coloncolon,
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
    kw_true,
    kw_false,
    kw_fn,
    kw_ptr,
    kw_if,
    kw_else,
    kw_let,
    kw_const,
    kw_while,
    kw_return,
    kw_struct,
    kw_extern,
    kw_defer,
    kw_err,
    kw_catch,
    kw_try,
    kw_module,
    kw_import,
    kw_as,
    kw_switch,
    kw_case,
    unknown,
    eof,
};
std::ostream& operator<<(std::ostream& os, const TokenType& t);

const static std::unordered_map<std::string_view, TokenType> keywords = {
    {"fn", TokenType::kw_fn},
    {"ptr", TokenType::kw_ptr},
    {"if", TokenType::kw_if},
    {"else", TokenType::kw_else},
    {"let", TokenType::kw_let},
    {"const", TokenType::kw_const},
    {"while", TokenType::kw_while},
    {"return", TokenType::kw_return},
    {"struct", TokenType::kw_struct},
    {"extern", TokenType::kw_extern},
    {"defer", TokenType::kw_defer},
    {"err", TokenType::kw_err},
    {"catch", TokenType::kw_catch},
    {"try", TokenType::kw_try},
    {"module", TokenType::kw_module},
    {"import", TokenType::kw_import},
    {"as", TokenType::kw_as},
    {"switch", TokenType::kw_switch},
    {"case", TokenType::kw_case},
    // Types
    {"void", TokenType::ty_void},
    {"f16", TokenType::ty_f16},
    {"f32", TokenType::ty_f32},
    {"f64", TokenType::ty_f64},
    {"true", TokenType::kw_true},
    {"false", TokenType::kw_false},
};

struct Token {
    TokenType type = TokenType::invalid;
    std::string_view str = {};
    SourceLocation loc = {};
};

std::ostream& operator<<(std::ostream& os, const Token& t);
std::ostream& operator<<(std::ostream& os, const std::vector<Token>& v_t);

class Lexer {
   public:
    Lexer(const char* file_name);
    std::vector<Token> tokenize_file();
    Token next_token();

   private:
    std::string read_file();
    void advance(int num = 1);

   private:
    std::string m_file_name;
    std::string m_file_content;
    size_t m_position = 0;
    size_t m_line = 0;
    size_t m_col = 0;
};
}  // namespace DMZ
