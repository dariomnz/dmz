#pragma once

#include "PH.hpp"
#include "Utils.hpp"

namespace C {

enum class TokenType {
    invalid,
    comment,
    id,
    return_type,
    lit_int,
    lit_float,
    lit_string,
    op_plus,
    op_minus,
    op_mult,
    op_div,
    op_and,
    op_or,
    op_less,
    op_more,
    op_less_eq,
    op_more_eq,
    op_equal,
    op_not,
    block_l,
    block_r,
    par_l,
    par_r,
    colon,
    semicolon,
    comma,
    kw_void,
    kw_fn,
    kw_int,
    kw_char,
    kw_ptr,
    kw_if,
    kw_else,
    kw_let,
    kw_while,
    kw_return,
    kw_struct,
    eof,
};
std::ostream& operator<<(std::ostream& os, const TokenType& t);

const static std::unordered_map<std::string_view, TokenType> keywords = {
    {"void", TokenType::kw_void},     {"fn", TokenType::kw_fn},        {"int", TokenType::kw_int},
    {"char", TokenType::kw_char},     {"ptr", TokenType::kw_ptr},      {"if", TokenType::kw_if},
    {"else", TokenType::kw_else},     {"let", TokenType::kw_let},      {"while", TokenType::kw_while},
    {"return", TokenType::kw_return}, {"struct", TokenType::kw_struct}};

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

   public:
    const std::string& get_file_name() { return m_file_name; }
    const std::string& get_file_content() { return m_file_content; }

   private:
    std::string m_file_name;
    std::string m_file_content;
    size_t m_position = 0;
    size_t m_line = 0;
    size_t m_col = 0;
};
}  // namespace C
