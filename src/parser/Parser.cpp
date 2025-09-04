#include "parser/Parser.hpp"

#include "Stats.hpp"

namespace DMZ {

bool Parser::is_top_level_token(TokenType tok) { return top_level_tokens.count(tok) != 0; }
bool Parser::is_top_top_level_token(TokenType tok) { return top_top_level_tokens.count(tok) != 0; }
bool Parser::is_top_stmt_level_token(TokenType tok) { return top_stmt_level_tokens.count(tok) != 0; }

// <sourceFile>
//   ::= (<structDecl> | <functionDecl>)* EOF
std::pair<ptr<ModuleDecl>, bool> Parser::parse_source_file(bool expectMain) {
    debug_func("");
    ScopedTimer(StatType::Parse);

    auto declarations = parse_in_module_decl();

    bool hasMainFunction = !expectMain;
    for (auto &&fn : declarations) hasMainFunction |= fn->identifier == "main";

    if (!hasMainFunction && !m_incompleteAST) report(m_nextToken.loc, "main function not found in global module");
    SourceLocation location = {.file_name = m_lexer.get_file_name(), .line = 0, .col = 0};
    auto mod = makePtr<ModuleDecl>(location, m_lexer.get_file_name(), std::move(declarations));
    debug_msg("Incomplete AST " << (m_incompleteAST ? "true" : "false"));
    return {std::move(mod), !m_incompleteAST && hasMainFunction};
}

// <type>
//  ::= 'int'
//  |   'char'
//  |   'bool'
//  |   'void'
//  |   <identifier>
ptr<Type> Parser::parse_type() {
    debug_func("");
    int isArray = -1;
    std::optional<int> isPointer = std::nullopt;
    SourceLocation loc = m_nextToken.loc;

    while (m_nextToken.type == TokenType::asterisk) {
        isPointer = isPointer.has_value() ? *isPointer + 1 : 1;
        eat_next_token();  // eat '*'
    }
    TokenType type = m_nextToken.type;
    std::string_view name = m_nextToken.str;

    std::unordered_set<TokenType> types = {
        TokenType::ty_void, TokenType::ty_f16, TokenType::ty_f32, TokenType::ty_f64,
        TokenType::ty_iN,   TokenType::ty_uN,  TokenType::id,
    };

    if (types.count(type) == 0) {
        report(m_nextToken.loc, "expected type specifier");
        return nullptr;
    }
    eat_next_token();  // eat type

    auto genericTypes = parse_generic_types();

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
            return report(m_nextToken.loc, "expected ']' next to a '[' in a type");
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
        t = Type::customType(name);
    }

    if (genericTypes) t.genericTypes = std::move(*genericTypes);
    if (isArray != -1) t.isArray = isArray;
    t.isPointer = isPointer;

    if (m_nextToken.type == TokenType::op_excla_mark) {
        t.isOptional = true;
        eat_next_token();  // eat '!'
    }
    t.location = loc;
    return makePtr<Type>(std::move(t));
}

ptr<GenericTypes> Parser::parse_generic_types() {
    debug_func("");
    if (m_nextToken.type != TokenType::op_less) {
        return nullptr;
    }
    auto typesDeclList = (parse_list_with_trailing_comma<Type>(
        {TokenType::op_less, "expected '<'"}, &Parser::parse_type, {TokenType::op_more, "expected '>'"}));
    if (!typesDeclList) return nullptr;

    return makePtr<GenericTypes>(std::move(*typesDeclList));
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

bool Parser::nextToken_is_generic(TokenType nextToken) {
    bool ret = false;
    debug_func(nextToken << " ret: " << (ret ? "true" : "false"));
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
        ret = peek_token(actual_jump + 1).type == nextToken;
        return ret;
    }
    ret = false;
    return ret;

    // return (m_nextToken.type == TokenType::op_less &&
    //         (peek_token(1).type == TokenType::comma || peek_token(1).type == TokenType::op_more ||
    //          peek_token(1).type == TokenType::op_less));
}
}  // namespace DMZ