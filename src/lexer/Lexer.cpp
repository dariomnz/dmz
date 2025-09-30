#include "lexer/Lexer.hpp"

#include <fcntl.h>

#include <cstring>

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
        CASE_TYPE(return_arrow);
        CASE_TYPE(switch_arrow);
        CASE_TYPE(lit_int);
        CASE_TYPE(lit_float);
        CASE_TYPE(lit_char);
        CASE_TYPE(lit_string);
        CASE_TYPE(op_plus);
        CASE_TYPE(op_plusplus);
        CASE_TYPE(op_minus);
        CASE_TYPE(op_minusminus);
        CASE_TYPE(asterisk);
        CASE_TYPE(op_div);
        CASE_TYPE(op_plus_equal);
        CASE_TYPE(op_minus_equal);
        CASE_TYPE(op_asterisk_equal);
        CASE_TYPE(op_div_equal);
        CASE_TYPE(op_percent);
        CASE_TYPE(ampamp);
        CASE_TYPE(pipepipe);
        CASE_TYPE(op_less);
        CASE_TYPE(op_more);
        CASE_TYPE(op_less_eq);
        CASE_TYPE(op_more_eq);
        CASE_TYPE(op_equal);
        CASE_TYPE(op_not_equal);
        CASE_TYPE(op_assign);
        CASE_TYPE(op_quest_mark);
        CASE_TYPE(op_excla_mark);
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
        CASE_TYPE(dotdot);
        CASE_TYPE(dotdotdot);
        CASE_TYPE(ty_void);
        CASE_TYPE(ty_f16);
        CASE_TYPE(ty_f32);
        CASE_TYPE(ty_f64);
        CASE_TYPE(ty_iN);
        CASE_TYPE(ty_uN);
        CASE_TYPE(ty_bool);
        CASE_TYPE(ty_isize);
        CASE_TYPE(ty_usize);
        CASE_TYPE(ty_slice);
        CASE_TYPE(kw_fn);
        CASE_TYPE(kw_true);
        CASE_TYPE(kw_false);
        CASE_TYPE(kw_null);
        CASE_TYPE(kw_if);
        CASE_TYPE(kw_else);
        CASE_TYPE(kw_let);
        CASE_TYPE(kw_const);
        CASE_TYPE(kw_while);
        CASE_TYPE(kw_for);
        CASE_TYPE(kw_return);
        CASE_TYPE(kw_struct);
        CASE_TYPE(kw_extern);
        CASE_TYPE(kw_defer);
        CASE_TYPE(kw_errdefer);
        CASE_TYPE(kw_error);
        CASE_TYPE(kw_catch);
        CASE_TYPE(kw_try);
        CASE_TYPE(kw_orelse);
        CASE_TYPE(kw_module);
        CASE_TYPE(kw_import);
        CASE_TYPE(kw_switch);
        CASE_TYPE(kw_case);
        CASE_TYPE(kw_test);
        // CASE_TYPE(kw_static);
        CASE_TYPE(kw_sizeof);
        CASE_TYPE(kw_packed);
        CASE_TYPE(kw_pub);
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

Lexer::Lexer(std::filesystem::path file_path) : m_file_path(file_path) {}

static inline bool isSpace(const std::string_view& c) {
    return c.size() > 0 &&
           (c[0] == ' ' || c[0] == '\f' || c[0] == '\n' || c[0] == '\r' || c[0] == '\t' || c[0] == '\v');
}
static inline bool isAlpha(const std::string_view& c) {
    return c.size() > 0 && (('a' <= c[0] && c[0] <= 'z') || ('A' <= c[0] && c[0] <= 'Z'));
}
static inline bool isDigit(const std::string_view& c) { return c.size() > 0 && '0' <= c[0] && c[0] <= '9'; }

static inline bool isAlnum(const std::string_view& c) { return isDigit(c) || isAlpha(c); }

bool Lexer::next_line() {
    if (!m_file.is_open()) {
        m_file.open(m_file_path);
        if (!m_file.is_open()) {
            dmz_unreachable("unexpected cannot open " + m_file_path.string() + " " + std::strerror(errno));
        }
    }
    m_line_buffer.clear();
    if (!std::getline(m_file, m_line_buffer)) {
        debug_msg("no more lines");
        return false;
    }
    debug_msg("read line " << m_line << " '" + m_line_buffer + "'");
    m_line++;
    m_col = 0;
    return true;
}

bool Lexer::advance(int num) {
    if (num <= 0) return true;
    // debug_msg("m_col " << m_col << " new m_col " << m_col + num);
    m_col += num;
    return m_col < m_line_buffer.size();
}

Token Lexer::next_token() {
    debug_msg("m_col " << m_col << " m_line_buffer.size() " << m_line_buffer.size());
    if (m_line_buffer.size() == 0 || m_col == m_line_buffer.size()) {
        if (!next_line()) {
            return Token{.type = TokenType::eof, .loc = {.file_name = m_file_path, .line = m_line, .col = m_col}};
        }
    }
    std::string_view line_content(m_line_buffer);
    line_content = line_content.substr(m_col);
    debug_msg("current line: " << m_line << " current col: " << m_col << " content '" << line_content << "'");
    size_t space_count = 0;
    // Consume spaces
    while (isSpace(line_content.substr(space_count, 1))) {
        space_count++;
    }
    advance(space_count);

    line_content = line_content.substr(space_count);
    if (line_content.empty()) {
        return next_token();
    }

    Token t{.type = TokenType::invalid, .loc = {.file_name = m_file_path, .line = m_line, .col = m_col}};
    defer([&]() { debug_msg(t); });
    if (line_content.substr(0, 1) == "\0") {
        t.type = TokenType::eof;
    } else if (isDigit(line_content.substr(0, 1))) {
        size_t digit_count = 1;
        while (isDigit(line_content.substr(digit_count, 1))) {
            digit_count++;
        }
        if (line_content.substr(digit_count, 1) != "." || line_content.substr(digit_count, 2) == "..") {
            t.type = TokenType::lit_int;
            t.str = line_content.substr(0, digit_count);
            advance(digit_count);
            return t;
        }
        digit_count++;  // the '.'
        if (!isDigit(line_content.substr(digit_count, 1))) {
            t.type = TokenType::unknown;
            t.str = line_content.substr(0, digit_count);
            advance(digit_count);
            return t;
        }
        while (isDigit(line_content.substr(digit_count, 1))) {
            digit_count++;
        }
        t.type = TokenType::lit_float;
        t.str = line_content.substr(0, digit_count);
        advance(digit_count);

    } else if (isAlpha(line_content.substr(0, 1)) || line_content.substr(0, 1) == "_" ||
               line_content.substr(0, 1) == "@") {
        size_t alpha_count = 1;
        while (isAlnum(line_content.substr(alpha_count, 1)) || line_content.substr(alpha_count, 1) == "_") {
            alpha_count++;
        }
        t.type = TokenType::id;
        t.str = line_content.substr(0, alpha_count);
        advance(alpha_count);
        auto it = keywords.find(t.str);
        if (it != keywords.end()) {
            t.type = it->second;
        }
        if (t.str.size() > 1 && (t.str[0] == 'i' || t.str[0] == 'u')) {
            bool isInteger = true;
            for (size_t i = 1; i < t.str.size(); i++) {
                if (!std::isdigit(t.str[i])) {
                    isInteger = false;
                    break;
                }
            }
            if (isInteger && t.str[0] == 'i') t.type = TokenType::ty_iN;
            if (isInteger && t.str[0] == 'u') t.type = TokenType::ty_uN;
        }

    } else if (line_content.substr(0, 2) == "//") {
        t.type = TokenType::comment;
        t.str = line_content;
        advance(line_content.size());
    } else if (line_content.substr(0, 1) == "{") {
        t.type = TokenType::block_l;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == "}") {
        t.type = TokenType::block_r;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == "(") {
        t.type = TokenType::par_l;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == ")") {
        t.type = TokenType::par_r;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 2) == "[]") {
        t.type = TokenType::ty_slice;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 1) == "[") {
        t.type = TokenType::bracket_l;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == "]") {
        t.type = TokenType::bracket_r;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == ":") {
        t.type = TokenType::colon;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == ";") {
        t.type = TokenType::semicolon;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == ",") {
        t.type = TokenType::comma;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == "\"") {
        size_t str_count = 1;
        if (!advance()) return t;
        while (line_content.substr(str_count, 1) != "\"" && line_content.substr(str_count, 1) != "\n") {
            str_count++;
            if (!advance()) return t;
        }
        // For last "
        str_count++;
        advance();
        t.type = TokenType::lit_string;
        t.str = line_content.substr(0, str_count);
    } else if (line_content.substr(0, 1) == "\'") {
        int char_size = 1;
        if (line_content.substr(char_size, 1) == "\\") {
            char_size++;
        }
        char_size++;
        if (!advance(char_size)) return t;
        if (line_content.substr(char_size, 1) != "\'") {
            t.type = TokenType::unknown;
            t.str = line_content.substr(1, char_size);
            return t;
        }
        advance();
        char_size++;
        t.type = TokenType::lit_char;
        t.str = line_content.substr(0, char_size);
    } else if (line_content.substr(0, 2) == "->") {
        t.type = TokenType::return_arrow;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 2) == "=>") {
        t.type = TokenType::switch_arrow;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 2) == "+=") {
        t.type = TokenType::op_plus_equal;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 2) == "++") {
        t.type = TokenType::op_plusplus;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 1) == "+") {
        t.type = TokenType::op_plus;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 2) == "-=") {
        t.type = TokenType::op_minus_equal;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 2) == "--") {
        t.type = TokenType::op_minusminus;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 1) == "-") {
        t.type = TokenType::op_minus;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 2) == "*=") {
        t.type = TokenType::op_asterisk_equal;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 1) == "*") {
        t.type = TokenType::asterisk;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 2) == "/=") {
        t.type = TokenType::op_div_equal;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 1) == "/") {
        t.type = TokenType::op_div;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == "%") {
        t.type = TokenType::op_percent;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 2) == "&&") {
        t.type = TokenType::ampamp;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 2) == "||") {
        t.type = TokenType::pipepipe;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 2) == "==") {
        t.type = TokenType::op_equal;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 1) == "=") {
        t.type = TokenType::op_assign;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 2) == "!=") {
        t.type = TokenType::op_not_equal;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 2) == "<=") {
        t.type = TokenType::op_less_eq;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 2) == ">=") {
        t.type = TokenType::op_more_eq;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 1) == "<") {
        t.type = TokenType::op_less;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == ">") {
        t.type = TokenType::op_more;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 3) == "...") {
        t.type = TokenType::dotdotdot;
        t.str = line_content.substr(0, 3);
        advance(3);
    } else if (line_content.substr(0, 2) == "..") {
        t.type = TokenType::dotdot;
        t.str = line_content.substr(0, 2);
        advance(2);
    } else if (line_content.substr(0, 1) == ".") {
        t.type = TokenType::dot;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == "|") {
        t.type = TokenType::pipe;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == "&") {
        t.type = TokenType::amp;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == "?") {
        t.type = TokenType::op_quest_mark;
        t.str = line_content.substr(0, 1);
        advance();
    } else if (line_content.substr(0, 1) == "!") {
        t.type = TokenType::op_excla_mark;
        t.str = line_content.substr(0, 1);
        advance();
    } else {
        t.type = TokenType::unknown;
        t.str = line_content.substr(0, 1);
        advance();
    }
    debug_msg(t);
    return t;
}

std::vector<Token> Lexer::tokenize_file() {
    debug_msg("Begin");
    std::vector<Token> v_tokens;

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