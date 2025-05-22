#include "parser/Parser.hpp"

namespace DMZ {

static const std::unordered_set<TokenType> top_level_tokens = {TokenType::eof,       TokenType::kw_fn,
                                                               TokenType::kw_struct, TokenType::kw_extern,
                                                               TokenType::kw_module, TokenType::kw_import};
bool Parser::is_top_level_token(TokenType tok) { return top_level_tokens.count(tok) != 0; }

// <sourceFile>
//   ::= (<structDecl> | <functionDecl>)* EOF
std::pair<std::vector<std::unique_ptr<Decl>>, bool> Parser::parse_source_file() {
    ScopedTimer st(Stats::type::parseTime);
    std::vector<std::unique_ptr<Decl>> declarations;

    while (m_nextToken.type != TokenType::eof) {
        if (m_nextToken.type == TokenType::kw_module) {
            if (auto mod = parse_module_decl()) {
                declarations.emplace_back(std::move(mod));
                continue;
            }
        } else if (m_nextToken.type == TokenType::kw_import) {
            if (auto mod = parse_import_decl()) {
                declarations.emplace_back(std::move(mod));
                continue;
            }
        } else if (m_nextToken.type == TokenType::kw_extern || m_nextToken.type == TokenType::kw_fn) {
            if (auto fn = parse_function_decl()) {
                declarations.emplace_back(std::move(fn));
                continue;
            }
        } else if (m_nextToken.type == TokenType::kw_struct) {
            if (auto st = parse_struct_decl()) {
                declarations.emplace_back(std::move(st));
                continue;
            }
        } else if (m_nextToken.type == TokenType::kw_err) {
            if (auto st = parse_err_group_decl()) {
                declarations.emplace_back(std::move(st));
                continue;
            }
        } else {
            report(m_nextToken.loc, "expected function, struct, err, module or import declaration on the top level");
        }

        synchronize_on(top_level_tokens);
        continue;
    }

    bool hasMainFunction = false;
    for (auto &&fn : declarations) hasMainFunction |= fn->identifier == "main";

    if (!hasMainFunction && !m_incompleteAST) report(m_nextToken.loc, "main function not found");

    return {std::move(declarations), !m_incompleteAST && hasMainFunction};
}

// <type>
//  ::= 'int'
//  |   'char'
//  |   'bool'
//  |   'void'
//  |   <identifier>
std::optional<Type> Parser::parse_type() {
    bool isArray = false;
    bool isRef = false;

    if (m_nextToken.type == TokenType::amp) {
        isRef = true;
        eat_next_token();  // eat '&'
    }
    TokenType type = m_nextToken.type;
    std::string_view name = m_nextToken.str;

    std::unordered_set<TokenType> types = {
        TokenType::kw_void, TokenType::kw_int, TokenType::kw_char, TokenType::kw_bool, TokenType::id,
    };

    if (types.count(type) == 0) {
        report(m_nextToken.loc, "expected type specifier");
        return std::nullopt;
    }
    eat_next_token();      // eat type

    if (m_nextToken.type == TokenType::bracket_l) {
        eat_next_token();  // eat '['
        if (m_nextToken.type != TokenType::bracket_r) {
            report(m_nextToken.loc, "expected ']' next to a '[' in a type");
            return std::nullopt;
        }
        eat_next_token();  // eat ']'
        isArray = true;
    }
    Type t;
    if (type == TokenType::kw_void) {
        t = Type::builtinVoid();
    }
    if (type == TokenType::kw_bool) {
        t = Type::builtinBool();
    }
    if (type == TokenType::kw_char) {
        t = Type::builtinChar();
    }
    if (type == TokenType::kw_int) {
        t = Type::builtinInt();
    }

    if (type == TokenType::id) {
        t = Type::custom(name);
    }

    if (isArray) t.isArray = 0;
    t.isRef = isRef;

    if (m_nextToken.type == TokenType::op_quest_mark) {
        t.isOptional = true;
        eat_next_token();  // eat '?'
    }
    return t;
}

void Parser::synchronize_on(std::unordered_set<TokenType> types) {
    m_incompleteAST = true;

    while (types.count(m_nextToken.type) == 0 && m_nextToken.type != TokenType::eof) eat_next_token();
}

void Parser::synchronize() {
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
        } else if (is_top_level_token(type)) {
            break;
        }

        eat_next_token();
    }
}

template <typename T, typename F>
std::unique_ptr<std::vector<std::unique_ptr<T>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *> openingToken, F parser, std::pair<TokenType, const char *> closingToken) {
    matchOrReturn(openingToken.first, openingToken.second);
    eat_next_token();  // eat openingToken

    std::vector<std::unique_ptr<T>> list;
    while (true) {
        if (m_nextToken.type == closingToken.first) break;

        varOrReturn(init, (this->*parser)());
        list.emplace_back(std::move(init));

        if (m_nextToken.type != TokenType::comma) break;
        eat_next_token();  // eat ','
    }

    matchOrReturn(closingToken.first, closingToken.second);
    eat_next_token();  // eat closingToken

    return std::make_unique<decltype(list)>(std::move(list));
}
// Instanciate the template for the needed type
template std::unique_ptr<std::vector<std::unique_ptr<ParamDecl>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *>, std::unique_ptr<ParamDecl> (Parser::*)(), std::pair<TokenType, const char *>);
template std::unique_ptr<std::vector<std::unique_ptr<FieldDecl>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *>, std::unique_ptr<FieldDecl> (Parser::*)(), std::pair<TokenType, const char *>);
template std::unique_ptr<std::vector<std::unique_ptr<ErrDecl>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *>, std::unique_ptr<ErrDecl> (Parser::*)(), std::pair<TokenType, const char *>);
template std::unique_ptr<std::vector<std::unique_ptr<FieldInitStmt>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *>, std::unique_ptr<FieldInitStmt> (Parser::*)(),
    std::pair<TokenType, const char *>);
template std::unique_ptr<std::vector<std::unique_ptr<Expr>>> Parser::parse_list_with_trailing_comma(
    std::pair<TokenType, const char *>, std::unique_ptr<Expr> (Parser::*)(), std::pair<TokenType, const char *>);

}  // namespace DMZ