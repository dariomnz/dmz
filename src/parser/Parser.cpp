#include "parser/Parser.hpp"

#include <charconv>
namespace DMZ {

bool Parser::is_top_level_token(TokenType tok) { return top_level_tokens.count(tok) != 0; }
bool Parser::is_top_stmt_level_token(TokenType tok) { return top_stmt_level_tokens.count(tok) != 0; }

// <sourceFile>
//   ::= (<structDecl> | <functionDecl>)* EOF
std::pair<std::vector<std::unique_ptr<Decl>>, bool> Parser::parse_source_file(bool expectMain) {
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
        } else if (m_nextToken.type == TokenType::kw_fn || m_nextToken.type == TokenType::kw_extern ||
                   m_nextToken.type == TokenType::kw_err || m_nextToken.type == TokenType::kw_struct) {
            auto decls = parse_in_module_decl();
            if (decls.size() > 0) {
                for (auto &&decl : decls) {
                    declarations.emplace_back(std::move(decl));
                }
                continue;
            }
        } else {
            report(m_nextToken.loc, "expected module, import, function, struct or err declaration on the top level");
        }

        synchronize_on(top_level_tokens);
        continue;
    }

    bool hasMainFunction = !expectMain;
    for (auto &&fn : declarations) hasMainFunction |= fn->identifier == "main";

    if (!hasMainFunction && !m_incompleteAST) report(m_nextToken.loc, "main function not found in global module");

    return {std::move(declarations), !m_incompleteAST && hasMainFunction};
}

// <type>
//  ::= 'int'
//  |   'char'
//  |   'bool'
//  |   'void'
//  |   <identifier>
std::optional<Type> Parser::parse_type() {
    int isArray = -1;
    bool isRef = false;

    if (m_nextToken.type == TokenType::amp) {
        isRef = true;
        eat_next_token();  // eat '&'
    }
    TokenType type = m_nextToken.type;
    std::string_view name = m_nextToken.str;

    std::unordered_set<TokenType> types = {
        TokenType::ty_void, TokenType::ty_f16, TokenType::ty_f32, TokenType::ty_f64,
        TokenType::ty_iN,   TokenType::ty_uN,  TokenType::id,
    };

    if (types.count(type) == 0) {
        report(m_nextToken.loc, "expected type specifier");
        return std::nullopt;
    }
    eat_next_token();      // eat type

    if (m_nextToken.type == TokenType::bracket_l) {
        eat_next_token();  // eat '['
        isArray = 0;
        if (m_nextToken.type == TokenType::lit_int) {
            int result = 0;
            auto res = std::from_chars(m_nextToken.str.data(), m_nextToken.str.data() + m_nextToken.str.size(), result);
            if (result == 0 || res.ec != std::errc()) {
                dmz_unreachable("unexpected size of 0 array type");
            }
            isArray = result;
            eat_next_token();  // eat lit_int
        } else if (m_nextToken.type != TokenType::bracket_r) {
            report(m_nextToken.loc, "expected ']' next to a '[' in a type");
            return std::nullopt;
        }
        eat_next_token();  // eat ']'
    }
    Type t;
    if (type == TokenType::ty_void) {
        t = Type::builtinVoid();
    }
    if (type == TokenType::ty_f16) {
        t = Type::builtinF16();
    }
    if (type == TokenType::ty_f32) {
        t = Type::builtinF32();
    }
    if (type == TokenType::ty_f64) {
        t = Type::builtinF64();
    }
    if (type == TokenType::ty_iN) {
        t = Type::builtinIN(name);
    }
    if (type == TokenType::ty_uN) {
        t = Type::builtinUN(name);
    }
    if (type == TokenType::id) {
        t = Type::custom(name);
    }

    if (isArray != -1) t.isArray = isArray;
    t.isRef = isRef;

    if (m_nextToken.type == TokenType::op_quest_mark) {
        t.isOptional = true;
        eat_next_token();  // eat '?'
    }
    return t;
}

void Parser::synchronize_on(std::unordered_set<TokenType> types) {
    m_incompleteAST = true;

    while (types.count(m_nextToken.type) == 0 && m_nextToken.type != TokenType::eof) {
        eat_next_token();
    }
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
        } else if (is_top_stmt_level_token(type) && blocks == 0) {
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