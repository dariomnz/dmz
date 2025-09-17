#include "parser/Parser.hpp"

#include "Stats.hpp"

namespace DMZ {

bool Parser::is_top_level_token(TokenType tok) { return top_level_tokens.count(tok) != 0; }
bool Parser::is_top_top_level_token(TokenType tok) { return top_top_level_tokens.count(tok) != 0; }
bool Parser::is_top_stmt_level_token(TokenType tok) { return top_stmt_level_tokens.count(tok) != 0; }

// <sourceFile>
//   ::= (<structDecl> | <functionDecl>)* EOF
std::pair<ptr<ModuleDecl>, bool> Parser::parse_source_file(bool expectMain) {
    debug_func(m_lexer.get_file_path());
    ScopedTimer(StatType::Parse);

    auto declarations = parse_in_module_decl();

    bool hasMainFunction = !expectMain;
    for (auto &&fn : declarations) hasMainFunction |= fn->identifier == "main";

    if (!hasMainFunction && !m_incompleteAST) report(m_nextToken.loc, "main function not found in global module");

    if (declarations.size() == 0) {
        return {nullptr, false};
    }

    auto file_path = m_lexer.get_file_path();
    SourceLocation location = {.file_name = file_path, .line = 0, .col = 0};
    auto module_name = file_path.filename().replace_extension("").string();
    auto mod = makePtr<ModuleDecl>(location, std::move(module_name), std::move(file_path), std::move(declarations));
    debug_msg("Incomplete AST " << (m_incompleteAST ? "true" : "false"));
    return {std::move(mod), !m_incompleteAST && hasMainFunction};
}

ptr<GenericExpr> Parser::parse_generic_expr(ptr<Expr> &prevExpr) {
    debug_func("");
    if (m_nextToken.type != TokenType::op_less) {
        return nullptr;
    }
    auto location = m_nextToken.loc;
    matchOrReturn(TokenType::op_less, "expected '<'");
    eat_next_token();  // eat openingToken

    std::vector<ptr<Expr>> typesDeclList;
    while (true) {
        if (m_nextToken.type == TokenType::op_more) break;

        varOrReturn(init, with_restrictions<ptr<Expr>>(OnlyTypeExpr, [&]() { return parse_expr(); }));
        typesDeclList.emplace_back(std::move(init));

        if (m_nextToken.type != TokenType::comma) break;
        eat_next_token();  // eat ','
    }

    matchOrReturn(TokenType::op_more, "expected '>'");
    eat_next_token();  // eat closingToken

    if (typesDeclList.size() == 0) return nullptr;

    return makePtr<GenericExpr>(location, std::move(prevExpr), std::move(typesDeclList));
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

template <typename T, typename F>
ptr<std::vector<ptr<T>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *> openingToken,
                                                                F parser,
                                                                std::pair<TokenType, const char *> closingToken) {
    debug_func("");
    matchOrReturn(openingToken.first, openingToken.second);
    eat_next_token();  // eat openingToken

    std::vector<ptr<T>> list;
    while (true) {
        if (m_nextToken.type == closingToken.first) break;

        varOrReturn(init, (this->*parser)());
        list.emplace_back(std::move(init));

        if (m_nextToken.type != TokenType::comma) break;
        eat_next_token();  // eat ','
    }

    matchOrReturn(closingToken.first, closingToken.second);
    eat_next_token();  // eat closingToken

    return makePtr<decltype(list)>(std::move(list));
}
// Instanciate the template for the needed type
template ptr<std::vector<ptr<ParamDecl>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *>,
                                                                                 ptr<ParamDecl> (Parser::*)(),
                                                                                 std::pair<TokenType, const char *>);
template ptr<std::vector<ptr<FieldDecl>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *>,
                                                                                 ptr<FieldDecl> (Parser::*)(),
                                                                                 std::pair<TokenType, const char *>);
template ptr<std::vector<ptr<ErrorDecl>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *>,
                                                                                 ptr<ErrorDecl> (Parser::*)(),
                                                                                 std::pair<TokenType, const char *>);
template ptr<std::vector<ptr<FieldInitStmt>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *>, ptr<FieldInitStmt> (Parser::*)(), std::pair<TokenType, const char *>);
template ptr<std::vector<ptr<Expr>>> Parser::parse_list_with_trailing_comma(std::pair<TokenType, const char *>,
                                                                            ptr<Expr> (Parser::*)(),
                                                                            std::pair<TokenType, const char *>);
template ptr<std::vector<ptr<GenericTypeDecl>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *>, ptr<GenericTypeDecl> (Parser::*)(), std::pair<TokenType, const char *>);

bool Parser::nextToken_is_generic() {
    bool ret = false;
    debug_func(" ret: " << (ret ? "true" : "false"));
    std::unordered_set<TokenType> postGenericToken = {
        TokenType::dot,   TokenType::block_l,   TokenType::par_l,
        TokenType::par_r, TokenType::semicolon, TokenType::op_assign,
    };
    if (m_nextToken.type == TokenType::op_less) {
        int blocks = 0;
        int actual_jump = 0;
        while (true) {
            TokenType type = peek_token(actual_jump).type;

            if (type == TokenType::op_less) {
                ++blocks;
            } else if (type == TokenType::op_more) {
                if (blocks == 0) break;
                --blocks;
            } else if (type == TokenType::eof || type == TokenType::block_l || type == TokenType::semicolon) {
                ret = false;
                return ret;
            }
            actual_jump++;
        }
        ret = postGenericToken.count(peek_token(actual_jump + 1).type) == 1;
        return ret;
    }
    ret = false;
    return ret;
}
}  // namespace DMZ