#include "lexer/Lexer.hpp"

#include <filesystem>
#include <fstream>

#include "Debug.hpp"

namespace DMZ {
std::ostream& operator<<(std::ostream& os, const TokenType& t) {
#define CASE_TYPE(name)     \
    case TokenType::name:   \
        return os << #name; \
        break
    switch (t) {
        CASE_TYPE(invalid);
        CASE_TYPE(comment);
        CASE_TYPE(id);
        CASE_TYPE(return_type);
        CASE_TYPE(lit_int);
        CASE_TYPE(lit_float);
        CASE_TYPE(lit_char);
        CASE_TYPE(lit_string);
        CASE_TYPE(op_plus);
        CASE_TYPE(op_minus);
        CASE_TYPE(op_mult);
        CASE_TYPE(op_div);
        CASE_TYPE(op_and);
        CASE_TYPE(op_or);
        CASE_TYPE(op_less);
        CASE_TYPE(op_more);
        CASE_TYPE(op_less_eq);
        CASE_TYPE(op_more_eq);
        CASE_TYPE(op_equal);
        CASE_TYPE(op_not_equal);
        CASE_TYPE(op_not);
        CASE_TYPE(op_assign);
        CASE_TYPE(amp);
        CASE_TYPE(pipe);
        CASE_TYPE(block_l);
        CASE_TYPE(block_r);
        CASE_TYPE(par_l);
        CASE_TYPE(par_r);
        CASE_TYPE(bracket_l);
        CASE_TYPE(bracket_r);
        CASE_TYPE(colon);
        CASE_TYPE(semicolon);
        CASE_TYPE(comma);
        CASE_TYPE(dot);
        CASE_TYPE(dotdotdot);
        CASE_TYPE(kw_void);
        CASE_TYPE(kw_fn);
        CASE_TYPE(kw_int);
        CASE_TYPE(kw_char);
        CASE_TYPE(kw_ptr);
        CASE_TYPE(kw_if);
        CASE_TYPE(kw_else);
        CASE_TYPE(kw_let);
        CASE_TYPE(kw_const);
        CASE_TYPE(kw_while);
        CASE_TYPE(kw_return);
        CASE_TYPE(kw_struct);
        CASE_TYPE(kw_extern);
        CASE_TYPE(kw_defer);
        CASE_TYPE(unknown);
        CASE_TYPE(eof);
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const Token& t) {
    os << t.loc;
    os << " '" << t.str << "' ";
    os << "Type: " << t.type;
    return os;
}

std::ostream& operator<<(std::ostream& os, const std::vector<Token>& v_t) {
    for (auto& t : v_t) {
        os << t << std::endl;
    }
    return os;
}

Lexer::Lexer(const char* file_name) : m_file_name(file_name), m_file_content(read_file()) {}

std::string Lexer::read_file() {
    debug_msg("Begin");
    auto size = std::filesystem::file_size(m_file_name);
    std::string content(size, '\0');
    std::ifstream in(m_file_name);
    in.read(&content[0], size);
    debug_msg("End");
    return content;
}

void Lexer::advance(int num) {
    if (num <= 0) return;
    if (m_file_content[m_position] == '\n') {
        m_col = 0;
    }
    for (int i = 0; i < num; i++) {
        m_col++;
        if (m_file_content[m_position + i] == '\n') {
            m_line++;
            m_col = 0;
        }
    }
    m_position += num;
}
Token Lexer::next_token() {
    debug_msg("Begin pos " << m_position);
    if (m_position >= m_file_content.size())
        return Token{.type = TokenType::eof, .loc = {.file_name = m_file_name, .line = m_line, .col = m_col}};

    std::string_view file_content(m_file_content);
    file_content = file_content.substr(m_position);
    size_t space_count = 0;
    // Consume spaces
    while (std::isspace(file_content[space_count])) {
        space_count++;
    }
    advance(space_count);
    file_content = file_content.substr(space_count);

    Token t{.type = TokenType::invalid, .loc = {.file_name = m_file_name, .line = m_line, .col = m_col}};
    if (file_content[0] == '\0') {
        debug_msg(TokenType::eof);
        t.type = TokenType::eof;
    } else if (std::isdigit(file_content[0])) {
        debug_msg(TokenType::lit_int);
        size_t digit_count = 1;
        while (std::isdigit(file_content[digit_count])) {
            digit_count++;
        }
        if (file_content[digit_count] != '.') {
            t.type = TokenType::lit_int;
            t.str = file_content.substr(0, digit_count);
            advance(digit_count);
            return t;
        }
        digit_count++;  // the '.'
        if (!std::isdigit(file_content[digit_count])) {
            t.type = TokenType::unknown;
            t.str = file_content.substr(0, digit_count);
            advance(digit_count);
            return t;
        }
        while (std::isdigit(file_content[digit_count])) {
            digit_count++;
        }
        t.type = TokenType::lit_float;
        t.str = file_content.substr(0, digit_count);
        advance(digit_count);

    } else if (std::isalpha(file_content[0])) {
        debug_msg(TokenType::id);
        size_t alpha_count = 1;
        while (std::isalnum(file_content[alpha_count]) || file_content[alpha_count] == '_') {
            alpha_count++;
        }
        t.type = TokenType::id;
        t.str = file_content.substr(0, alpha_count);
        advance(alpha_count);
        auto it = keywords.find(t.str);
        if (it != keywords.end()) {
            t.type = it->second;
        }
    } else if (file_content.substr(0, 2) == "//") {
        debug_msg(TokenType::comment);
        size_t comment_count = 2;
        // For the //
        advance(2);
        while (comment_count < file_content.size() && file_content[comment_count] != '\n') {
            comment_count++;
            advance();
        }
        t.type = TokenType::comment;
        t.str = file_content.substr(0, comment_count);
    } else if (file_content[0] == '{') {
        debug_msg(TokenType::block_l);
        t.type = TokenType::block_l;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '}') {
        debug_msg(TokenType::block_r);
        t.type = TokenType::block_r;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '(') {
        debug_msg(TokenType::par_l);
        t.type = TokenType::par_l;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == ')') {
        debug_msg(TokenType::par_r);
        t.type = TokenType::par_r;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '[') {
        debug_msg(TokenType::bracket_l);
        t.type = TokenType::bracket_l;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == ']') {
        debug_msg(TokenType::bracket_r);
        t.type = TokenType::bracket_r;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == ':') {
        debug_msg(TokenType::colon);
        t.type = TokenType::colon;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == ';') {
        debug_msg(TokenType::semicolon);
        t.type = TokenType::semicolon;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == ',') {
        debug_msg(TokenType::comma);
        t.type = TokenType::comma;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '\"') {
        debug_msg(TokenType::lit_string);
        size_t str_count = 1;
        advance();
        while (file_content[str_count] != '\"' && file_content[str_count] != '\n') {
            str_count++;
            advance();
        }
        // For last "
        str_count++;
        advance();
        t.type = TokenType::lit_string;
        t.str = file_content.substr(0, str_count);
    } else if (file_content[0] == '\'') {
        debug_msg(TokenType::lit_char);
        int char_size = 1;
        if (file_content[char_size] == '\\') {
            char_size++;
        }
        char_size++;
        advance(char_size);
        if (file_content[char_size] != '\'') {
            t.type = TokenType::unknown;
            t.str = file_content.substr(1, char_size);
            return t;
        }
        advance();
        char_size++;
        t.type = TokenType::lit_char;
        t.str = file_content.substr(0, char_size);
    } else if (file_content.substr(0, 2) == "->") {
        debug_msg(TokenType::return_type);
        t.type = TokenType::return_type;
        t.str = file_content.substr(0, 2);
        advance(2);
    } else if (file_content[0] == '+') {
        debug_msg(TokenType::op_plus);
        t.type = TokenType::op_plus;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '-') {
        debug_msg(TokenType::op_minus);
        t.type = TokenType::op_minus;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '*') {
        debug_msg(TokenType::op_mult);
        t.type = TokenType::op_mult;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '/') {
        debug_msg(TokenType::op_div);
        t.type = TokenType::op_div;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content.substr(0, 2) == "&&") {
        debug_msg(TokenType::op_and);
        t.type = TokenType::op_and;
        t.str = file_content.substr(0, 2);
        advance(2);
    } else if (file_content.substr(0, 2) == "||") {
        debug_msg(TokenType::op_or);
        t.type = TokenType::op_or;
        t.str = file_content.substr(0, 2);
        advance(2);
    } else if (file_content.substr(0, 2) == "==") {
        debug_msg(TokenType::op_equal);
        t.type = TokenType::op_equal;
        t.str = file_content.substr(0, 2);
        advance(2);
    } else if (file_content[0] == '=') {
        debug_msg(TokenType::op_assign);
        t.type = TokenType::op_assign;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content.substr(0, 2) == "!=") {
        debug_msg(TokenType::op_not_equal);
        t.type = TokenType::op_not_equal;
        t.str = file_content.substr(0, 2);
        advance(2);
    } else if (file_content.substr(0, 2) == "<=") {
        debug_msg(TokenType::op_less_eq);
        t.type = TokenType::op_less_eq;
        t.str = file_content.substr(0, 2);
        advance(2);
    } else if (file_content.substr(0, 2) == ">=") {
        debug_msg(TokenType::op_more_eq);
        t.type = TokenType::op_more_eq;
        t.str = file_content.substr(0, 2);
        advance(2);
    } else if (file_content[0] == '<') {
        debug_msg(TokenType::op_less);
        t.type = TokenType::op_less;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '>') {
        debug_msg(TokenType::op_more);
        t.type = TokenType::op_more;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '!') {
        debug_msg(TokenType::op_not);
        t.type = TokenType::op_not;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content.substr(0, 3) == "...") {
        debug_msg(TokenType::dotdotdot);
        t.type = TokenType::dotdotdot;
        t.str = file_content.substr(0, 3);
        advance(3);
    } else if (file_content[0] == '.') {
        debug_msg(TokenType::dot);
        t.type = TokenType::dot;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '|') {
        debug_msg(TokenType::pipe);
        t.type = TokenType::pipe;
        t.str = file_content.substr(0, 1);
        advance();
    } else if (file_content[0] == '&') {
        debug_msg(TokenType::amp);
        t.type = TokenType::amp;
        t.str = file_content.substr(0, 1);
        advance();
    } else {
        debug_msg(TokenType::unknown);
        t.type = TokenType::unknown;
        t.str = file_content.substr(0, 1);
        advance();
    }
    debug_msg("End " << t);
    return t;
}

std::vector<Token> Lexer::tokenize_file() {
    debug_msg("Begin");
    std::vector<Token> v_tokens;

    m_position = 0;
    m_line = 0;
    m_col = 0;

    Token result;
    do {
        result = v_tokens.emplace_back(next_token());
    } while (result.type != TokenType::eof);

    debug_msg("End");
    return v_tokens;
}

}  // namespace DMZ