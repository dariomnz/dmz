#include "parser/Parser.hpp"

namespace DMZ {

std::unique_ptr<GenericTypeDecl> Parser::parse_generic_type_decl() {
    debug_func("");
    matchOrReturn(TokenType::id, "expected identifier");
    auto location = m_nextToken.loc;
    auto identifier = m_nextToken.str;
    eat_next_token();  // eat id
    return std::make_unique<GenericTypeDecl>(location, identifier);
}

std::vector<std::unique_ptr<GenericTypeDecl>> Parser::parse_generic_types_decl() {
    debug_func("");
    if (m_nextToken.type != TokenType::op_less) {
        return {};
    }
    auto typesDeclList = (parse_list_with_trailing_comma<GenericTypeDecl>(
        {TokenType::op_less, "expected '<'"}, &Parser::parse_generic_type_decl, {TokenType::op_more, "expected '>'"}));
    if (!typesDeclList) return {};

    return std::move(*typesDeclList);
}

// <functionDecl>
//  ::= 'extern'? 'fn' <identifier> '(' ')' '->' <type> <block>
std::unique_ptr<FuncDecl> Parser::parse_function_decl() {
    debug_func("");
    SourceLocation loc = m_nextToken.loc;
    SourceLocation structLocation;
    bool isExtern = false;

    if (m_nextToken.type == TokenType::kw_extern) {
        isExtern = true;
        eat_next_token();  // eat extern
    }

    matchOrReturn(TokenType::kw_fn, "expected 'fn'");

    eat_next_token();  // eat fn

    matchOrReturn(TokenType::id, "expected identifier");

    std::string_view functionIdentifier = m_nextToken.str;
    structLocation = m_nextToken.loc;
    eat_next_token();  // eat identifier

    auto genericTypes = parse_generic_types_decl();

    varOrReturn(parameterList,
                parse_list_with_trailing_comma<ParamDecl>({TokenType::par_l, "expected '('"}, &Parser::parse_param_decl,
                                                          {TokenType::par_r, "expected ')'"}));

    matchOrReturn(TokenType::return_arrow, "expected '->'");
    eat_next_token();  // eat '->'

    varOrReturn(type, parse_type());

    if (isExtern) {
        if (m_nextToken.type == TokenType::block_l) return report(m_nextToken.loc, "extern fn cannot have a body");
        matchOrReturn(TokenType::semicolon, "expected ';'");
        eat_next_token();
        return std::make_unique<ExternFunctionDecl>(loc, functionIdentifier, *type, std::move(*parameterList));
    }

    matchOrReturn(TokenType::block_l, "expected function body");
    varOrReturn(block, parse_block());

    if (genericTypes.size() != 0) {
        return std::make_unique<GenericFunctionDecl>(loc, functionIdentifier, *type, std::move(*parameterList),
                                                     std::move(block), std::move(genericTypes));
    } else {
        return std::make_unique<FunctionDecl>(loc, functionIdentifier, *type, std::move(*parameterList),
                                              std::move(block));
    }
}

// <paramDecl>
//  ::= 'const'? <identifier> ':' <type>
std::unique_ptr<ParamDecl> Parser::parse_param_decl() {
    debug_func("");
    SourceLocation location = m_nextToken.loc;

    if (m_nextToken.type == TokenType::dotdotdot) {
        std::string_view identifier = m_nextToken.str;
        eat_next_token();  // eat '...'
        return std::make_unique<ParamDecl>(location, std::move(identifier), Type::builtinVoid(), false, true);
    }

    matchOrReturn(TokenType::id, "expected parameter declaration");

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat :

    varOrReturn(type, parse_type());

    return std::make_unique<ParamDecl>(location, std::move(identifier), std::move(*type), false);
}

std::unique_ptr<VarDecl> Parser::parse_var_decl(bool isConst) {
    debug_func("");
    SourceLocation location = m_nextToken.loc;

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    std::unique_ptr<Type> type;
    if (m_nextToken.type == TokenType::colon) {
        eat_next_token();  // eat ':'

        type = parse_type();
        if (!type) return nullptr;
    }

    if (m_nextToken.type != TokenType::op_assign) {
        return std::make_unique<VarDecl>(location, identifier, std::move(type), !isConst);
    }
    eat_next_token();  // eat '='

    varOrReturn(initializer, parse_expr());

    return std::make_unique<VarDecl>(location, identifier, std::move(type), !isConst, std::move(initializer));
}

// <structDecl>
//  ::= 'struct' <identifier> <fieldList>
//
// <fieldList>
//  ::= '{' (<fieldDecl> (',' <fieldDecl>)* ','?)? '}'
std::unique_ptr<StructDecl> Parser::parse_struct_decl() {
    debug_func("");
    SourceLocation location = m_nextToken.loc;
    bool isPacked = false;
    if (m_nextToken.type == TokenType::kw_packed) {
        eat_next_token();  // eat packet
        isPacked = true;
    }

    matchOrReturn(TokenType::kw_struct, "expected 'struct'");
    eat_next_token();  // eat struct

    matchOrReturn(TokenType::id, "expected identifier");

    std::string_view structIdentifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    auto genericTypes = parse_generic_types_decl();

    matchOrReturn(TokenType::block_l, "expected '{'");
    eat_next_token();  // eat openingToken

    std::vector<std::unique_ptr<FieldDecl>> fieldList;
    std::vector<std::unique_ptr<MemberFunctionDecl>> funcList;
    std::unique_ptr<StructDecl> structDecl;
    if (genericTypes.size() != 0) {
        structDecl = std::make_unique<GenericStructDecl>(location, structIdentifier, isPacked, std::move(fieldList),
                                                         std::move(funcList), std::move(genericTypes));
    } else {
        structDecl = std::make_unique<StructDecl>(location, structIdentifier, isPacked, std::move(fieldList),
                                                  std::move(funcList));
    }

    while (true) {
        if (m_nextToken.type == TokenType::block_r) break;

        if (m_nextToken.type == TokenType::id) {
            varOrReturn(init, parse_field_decl());
            fieldList.emplace_back(std::move(init));
            if (m_nextToken.type != TokenType::comma) break;
            eat_next_token();  // eat ','
        } else if (m_nextToken.type == TokenType::kw_fn || m_nextToken.type == TokenType::kw_static) {
            bool isStatic = m_nextToken.type == TokenType::kw_static;
            if (isStatic) {
                eat_next_token();  // eat static
            }
            varOrReturn(init, parse_function_decl());
            varOrReturn(func, dynamic_cast<FunctionDecl*>(init.get()));
            auto memberFunc = std::make_unique<MemberFunctionDecl>(func->location, func->identifier, func->type,
                                                                   std::move(func->params), std::move(func->body),
                                                                   structDecl.get(), isStatic);
            funcList.emplace_back(std::move(memberFunc));
        } else {
            return report(m_nextToken.loc, "expected identifier or fn in struct");
        }
    }

    matchOrReturn(TokenType::block_r, "expected '}'");
    eat_next_token();  // eat closingToken

    structDecl->fields = std::move(fieldList);
    structDecl->functions = std::move(funcList);

    return structDecl;
}

// <fieldDecl>
//  ::= <identifier> ':' <type>
std::unique_ptr<FieldDecl> Parser::parse_field_decl() {
    debug_func("");

    SourceLocation location = m_nextToken.loc;
    // assert(nextToken.value && "identifier token without value");

    matchOrReturn(TokenType::id, "expected field declaration");
    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat :

    varOrReturn(type, parse_type());

    return std::make_unique<FieldDecl>(location, std::move(identifier), std::move(*type));
};

std::unique_ptr<ErrorGroupExprDecl> Parser::parse_error_group_expr_decl() {
    debug_func("");
    matchOrReturn(TokenType::kw_error, "expected 'error'");
    auto location = m_nextToken.loc;

    eat_next_token();  // eat error

    varOrReturn(errors, parse_list_with_trailing_comma<ErrorDecl>({TokenType::block_l, "expected '{'"},
                                                                  &Parser::parse_error_decl,
                                                                  {TokenType::block_r, "expected '}'"}));
    return std::make_unique<ErrorGroupExprDecl>(location, std::move(*errors));
}

std::unique_ptr<ErrorDecl> Parser::parse_error_decl() {
    debug_func("");
    matchOrReturn(TokenType::id, "expected identifier");
    auto location = m_nextToken.loc;
    auto id = m_nextToken.str;

    eat_next_token();  // eat id
    return std::make_unique<ErrorDecl>(location, id);
}

std::unique_ptr<ModuleDecl> Parser::parse_module_decl() {
    debug_func("");
    auto location = m_nextToken.loc;
    matchOrReturn(TokenType::kw_module, "expected 'module'");
    eat_next_token();  // eat module

    matchOrReturn(TokenType::id, "expected identifier");
    auto identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::block_l, "expected '{'");
    eat_next_token();  // eat {

    auto declarations = parse_in_module_decl();

    matchOrReturn(TokenType::block_r, "expected '}'");
    eat_next_token();  // eat {

    return std::make_unique<ModuleDecl>(location, identifier, std::move(declarations));
}

std::vector<std::unique_ptr<Decl>> Parser::parse_in_module_decl() {
    debug_func("");
    std::vector<std::unique_ptr<Decl>> declarations;

    while (m_nextToken.type != TokenType::eof && m_nextToken.type != TokenType::block_r) {
        if (m_nextToken.type == TokenType::kw_extern || m_nextToken.type == TokenType::kw_fn) {
            if (auto fn = parse_function_decl()) {
                declarations.emplace_back(std::move(fn));
                continue;
            }
        } else if (m_nextToken.type == TokenType::kw_const || m_nextToken.type == TokenType::kw_let) {
            if (auto st = parse_decl_stmt()) {
                declarations.emplace_back(std::move(st));
                continue;
            }
        } else if (m_nextToken.type == TokenType::kw_struct || m_nextToken.type == TokenType::kw_packed) {
            if (auto st = parse_struct_decl()) {
                declarations.emplace_back(std::move(st));
                continue;
            }
        } else if (m_nextToken.type == TokenType::kw_module) {
            if (auto st = parse_module_decl()) {
                declarations.emplace_back(std::move(st));
                continue;
            }
        } else if (m_nextToken.type == TokenType::kw_test) {
            if (auto test = parse_test_decl()) {
                declarations.emplace_back(std::move(test));
                continue;
            }
        } else {
            report(m_nextToken.loc, "expected declaration");
        }

        synchronize_on(top_top_level_tokens);
        continue;
    }

    return declarations;
}

std::unique_ptr<TestDecl> Parser::parse_test_decl() {
    auto location = m_nextToken.loc;
    matchOrReturn(TokenType::kw_test, "expected 'test'");
    eat_next_token();  // eat test

    matchOrReturn(TokenType::lit_string, "expected string literal");
    std::string_view name = m_nextToken.str;
    name = name.substr(1, name.size() - 2);
    eat_next_token();  // eat name

    matchOrReturn(TokenType::block_l, "expected function body");
    varOrReturn(block, parse_block());

    return std::make_unique<TestDecl>(location, name, std::move(block));
}
}  // namespace DMZ