#ifdef DEBUG_PARSER
#ifndef DEBUG
#define DEBUG
#endif
#endif
#include "parser/Parser.hpp"

#include "Stats.hpp"

namespace DMZ {

bool Parser::is_top_level_token(TokenType tok) { return top_level_tokens.count(tok) != 0; }
bool Parser::is_top_top_level_token(TokenType tok) { return top_top_level_tokens.count(tok) != 0; }
bool Parser::is_top_stmt_level_token(TokenType tok) { return top_stmt_level_tokens.count(tok) != 0; }

// <sourceFile>
//   ::= (<structDecl> | <functionDecl>)* EOF
std::pair<ptr<ModuleDecl>, bool> Parser::parse_source_file() {
    debug_func(m_lexer.get_file_path());
    ScopedTimer(StatType::Parse);

    auto declarations = parse_in_module_decl();

    if (declarations.size() == 0) {
        return {nullptr, true};
    }

    auto file_path = m_lexer.get_file_path();
    SourceLocation location = {.file_name = file_path, .line = 1, .col = 0};
    auto module_name = file_path.filename().replace_extension("").string();
    auto mod = makePtr<ModuleDecl>(location, std::move(module_name), std::move(file_path), std::move(declarations));
    debug_msg("Incomplete AST " << (m_incompleteAST ? "true" : "false"));
    return {std::move(mod), !m_incompleteAST};
}

void Parser::synchronize_on(std::unordered_set<TokenType> types) {
    debug_func("");
    m_incompleteAST = true;

    while (types.count(m_nextToken.type) == 0 && m_nextToken.type != TokenType::eof) {
        eat_next_token();
    }
}

void Parser::synchronize() {
    debug_func("");
    m_incompleteAST = true;

    int blocks = 0;
    while (true) {
        TokenType type = m_nextToken.type;

        if (type == TokenType::block_l) {
            ++blocks;
        } else if (type == TokenType::block_r) {
            if (blocks == 0) break;
            if (blocks == 1) {
                eat_next_token();  // eat '}'
                break;
            }
            --blocks;
        } else if (type == TokenType::semicolon && blocks == 0) {
            eat_next_token();  // eat ';'
            break;
        } else if (is_top_stmt_level_token(type) && blocks == 0) {
            break;
        } else if (is_top_level_token(type)) {
            break;
        }

        eat_next_token();
    }
}

template <typename T>
ptr<std::vector<ptr<T>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *> openingToken,
                                                                std::function<ptr<T>()> parser,
                                                                std::pair<TokenType, const char *> closingToken,
                                                                bool &haveLastComma) {
    debug_func("");
    matchOrReturn(openingToken.first, openingToken.second);
    eat_next_token();  // eat openingToken

    std::vector<ptr<T>> list;
    haveLastComma = false;
    while (true) {
        if (m_nextToken.type == closingToken.first) break;
        haveLastComma = false;

        varOrReturn(init, parser());
        list.emplace_back(std::move(init));

        if (m_nextToken.type != TokenType::comma) break;
        haveLastComma = true;
        eat_next_token();  // eat ','
    }

    matchOrReturn(closingToken.first, closingToken.second);
    eat_next_token();  // eat closingToken

    return makePtr<std::vector<ptr<T>>>(std::move(list));
}
// Specialize
template ptr<std::vector<ptr<ParamDecl>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *>,
                                                                                 std::function<ptr<ParamDecl>()>,
                                                                                 std::pair<TokenType, const char *>,
                                                                                 bool &);
template ptr<std::vector<ptr<FieldInitStmt>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *>, std::function<ptr<FieldInitStmt>()>, std::pair<TokenType, const char *>,
    bool &);
template ptr<std::vector<ptr<CaptureDecl>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *>,
                                                                                   std::function<ptr<CaptureDecl>()>,
                                                                                   std::pair<TokenType, const char *>,
                                                                                   bool &);
template ptr<std::vector<ptr<ErrorDecl>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *>,
                                                                                 std::function<ptr<ErrorDecl>()>,
                                                                                 std::pair<TokenType, const char *>,
                                                                                 bool &);
template ptr<std::vector<ptr<Expr>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *>,
                                                                            std::function<ptr<Expr>()>,
                                                                            std::pair<TokenType, const char *>, bool &);

ptr<Comment> Parser::parse_comment() {
    debug_func("");
    auto location = m_nextToken.loc;
    auto identifier = m_nextToken.str;
    eat_next_token();  // eat comment

    return makePtr<Comment>(location, std::move(identifier));
}

ptr<EmptyLine> Parser::parse_empty_line() {
    debug_func("");
    auto location = m_nextToken.loc;
    eat_next_token();  // eat empty_line

    return makePtr<EmptyLine>(location);
}
}  // namespace DMZ