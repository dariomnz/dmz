#include "DMZPCH.hpp"
#include "parser/Parser.hpp"
#include "parser/ParserSymbols.hpp"

namespace DMZ {

ptr<GenericTypeDecl> Parser::parse_generic_type_decl() {
    debug_func("");
    matchOrReturn(TokenType::id, "expected identifier");
    auto location = m_nextToken.loc;
    auto identifier = m_nextToken.str;
    eat_next_token();  // eat id
    return makePtr<GenericTypeDecl>(location, identifier);
}

std::vector<ptr<GenericTypeDecl>> Parser::parse_generic_types_decl() {
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
ptr<FuncDecl> Parser::parse_function_decl() {
    debug_func("");
    SourceLocation loc = m_nextToken.loc;
    SourceLocation structLocation;
    bool isPublic = false;
    bool isExtern = false;

    if (m_nextToken.type == TokenType::kw_pub) {
        eat_next_token();  // eat pub
        isPublic = true;
    }
    if (m_nextToken.type == TokenType::kw_extern) {
        isExtern = true;
        eat_next_token();  // eat extern
    }

    matchOrReturn(TokenType::kw_fn, "expected 'fn'");

    eat_next_token();  // eat fn

    matchOrReturn(TokenType::id, "expected identifier");

    auto functionIdentifier = m_nextToken.str;
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
        return makePtr<ExternFunctionDecl>(loc, isPublic, functionIdentifier, std::move(type),
                                           std::move(*parameterList));
    }

    matchOrReturn(TokenType::block_l, "expected function body");
    varOrReturn(block, parse_block());

    if (genericTypes.size() != 0) {
        return makePtr<GenericFunctionDecl>(loc, isPublic, functionIdentifier, std::move(type),
                                            std::move(*parameterList), std::move(block), std::move(genericTypes));
    } else {
        return makePtr<FunctionDecl>(loc, isPublic, functionIdentifier, std::move(type), std::move(*parameterList),
                                     std::move(block));
    }
}

// <paramDecl>
//  ::= 'const'? <identifier> ':' <type>
ptr<ParamDecl> Parser::parse_param_decl() {
    debug_func("");
    SourceLocation location = m_nextToken.loc;

    if (m_nextToken.type == TokenType::dotdotdot) {
        auto identifier = m_nextToken.str;
        eat_next_token();  // eat '...'
        return makePtr<ParamDecl>(location, std::move(identifier), makePtr<TypeVoid>(location), false, true);
    }

    matchOrReturn(TokenType::id, "expected parameter declaration");

    auto identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat :

    varOrReturn(type, parse_type());

    return makePtr<ParamDecl>(location, std::move(identifier), std::move(type), false);
}

ptr<VarDecl> Parser::parse_var_decl(bool isPublic, bool isConst) {
    debug_func("");
    SourceLocation location = m_nextToken.loc;

    auto identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    ptr<Expr> type = nullptr;
    if (m_nextToken.type == TokenType::colon) {
        eat_next_token();  // eat ':'

        type = parse_type();
        if (!type) return nullptr;
    }

    if (m_nextToken.type != TokenType::op_assign) {
        return makePtr<VarDecl>(location, isPublic, identifier, std::move(type), !isConst);
    }
    eat_next_token();  // eat '='

    varOrReturn(initializer, parse_expr());

    return makePtr<VarDecl>(location, isPublic, identifier, std::move(type), !isConst, std::move(initializer));
}

// <structDecl>
//  ::= 'struct' <identifier> <fieldList>
//
// <fieldList>
//  ::= '{' (<fieldDecl> (',' <fieldDecl>)* ','?)? '}'
ptr<StructDecl> Parser::parse_struct_decl() {
    debug_func("");
    SourceLocation location = m_nextToken.loc;
    bool isPacked = false;
    bool isPublic = false;
    if (m_nextToken.type == TokenType::kw_pub) {
        eat_next_token();  // eat pub
        isPublic = true;
    }

    if (m_nextToken.type == TokenType::kw_packed) {
        eat_next_token();  // eat packet
        isPacked = true;
    }

    matchOrReturn(TokenType::kw_struct, "expected 'struct'");
    eat_next_token();  // eat struct

    matchOrReturn(TokenType::id, "expected identifier");

    auto structIdentifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    auto genericTypes = parse_generic_types_decl();

    matchOrReturn(TokenType::block_l, "expected '{'");
    eat_next_token();  // eat openingToken

    std::vector<ptr<FieldDecl>> fieldList;
    std::vector<ptr<MemberFunctionDecl>> funcList;
    ptr<StructDecl> structDecl;
    if (genericTypes.size() != 0) {
        structDecl = makePtr<GenericStructDecl>(location, isPublic, structIdentifier, isPacked, std::move(fieldList),
                                                std::move(funcList), std::move(genericTypes));
    } else {
        structDecl = makePtr<StructDecl>(location, isPublic, structIdentifier, isPacked, std::move(fieldList),
                                         std::move(funcList));
    }

    while (true) {
        if (m_nextToken.type == TokenType::block_r) break;

        if (m_nextToken.type == TokenType::id) {
            varOrReturn(init, parse_field_decl());
            fieldList.emplace_back(std::move(init));
            if (m_nextToken.type != TokenType::comma) break;
            eat_next_token();  // eat ','
        } else if (m_nextToken.type == TokenType::kw_fn || m_nextToken.type == TokenType::kw_pub) {
            bool isPublic = m_nextToken.type == TokenType::kw_pub;
            if (isPublic) {
                eat_next_token();  // eat pub
            }
            varOrReturn(init, parse_function_decl());
            varOrReturn(func, dynamic_cast<FunctionDecl*>(init.get()));
            auto memberFunc =
                makePtr<MemberFunctionDecl>(func->location, isPublic, func->identifier, std::move(func->type),
                                            std::move(func->params), std::move(func->body), structDecl.get());
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
ptr<FieldDecl> Parser::parse_field_decl() {
    debug_func("");

    SourceLocation location = m_nextToken.loc;
    // assert(nextToken.value && "identifier token without value");

    matchOrReturn(TokenType::id, "expected field declaration");
    auto identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat :

    varOrReturn(type, parse_type());

    return makePtr<FieldDecl>(location, std::move(identifier), std::move(type));
};

ptr<ErrorGroupExprDecl> Parser::parse_error_group_expr_decl() {
    debug_func("");
    matchOrReturn(TokenType::kw_error, "expected 'error'");
    auto location = m_nextToken.loc;

    eat_next_token();  // eat error

    varOrReturn(errors, parse_list_with_trailing_comma<ErrorDecl>({TokenType::block_l, "expected '{'"},
                                                                  &Parser::parse_error_decl,
                                                                  {TokenType::block_r, "expected '}'"}));
    return makePtr<ErrorGroupExprDecl>(location, std::move(*errors));
}

ptr<ErrorInPlaceExpr> Parser::parse_error_in_place_expr() {
    debug_func("");
    auto location = m_nextToken.loc;
    matchOrReturn(TokenType::kw_error, "expected error");
    eat_next_token();  // eat error

    matchOrReturn(TokenType::dot, "expected '.'");
    eat_next_token();  // eat error

    matchOrReturn(TokenType::id, "expected identifier");
    auto id = m_nextToken.str;
    eat_next_token();  // eat id

    return makePtr<ErrorInPlaceExpr>(location, id);
}

ptr<ErrorDecl> Parser::parse_error_decl() {
    debug_func("");
    matchOrReturn(TokenType::id, "expected identifier");
    auto location = m_nextToken.loc;
    auto id = m_nextToken.str;

    eat_next_token();  // eat id
    return makePtr<ErrorDecl>(location, id);
}

ptr<ModuleDecl> Parser::parse_module_decl() {
    debug_func("");
    dmz_unreachable("TODO: think if permit declaration of modules");
    // auto location = m_nextToken.loc;
    // matchOrReturn(TokenType::kw_module, "expected 'module'");
    // eat_next_token();  // eat module

    // matchOrReturn(TokenType::id, "expected identifier");
    // auto identifier = m_nextToken.str;
    // eat_next_token();  // eat identifier

    // matchOrReturn(TokenType::block_l, "expected '{'");
    // eat_next_token();  // eat {

    // auto declarations = parse_in_module_decl();

    // matchOrReturn(TokenType::block_r, "expected '}'");
    // eat_next_token();  // eat {

    // return makePtr<ModuleDecl>(location, identifier, std::move(declarations));
}

std::vector<ptr<Decl>> Parser::parse_in_module_decl() {
    debug_func("");
    std::vector<ptr<Decl>> declarations;

    while (m_nextToken.type != TokenType::eof && m_nextToken.type != TokenType::block_r) {
        TokenType ttype;
        if (m_nextToken.type == TokenType::kw_pub) {
            ttype = peek_token(0).type;
        } else {
            ttype = m_nextToken.type;
        }
        if (ttype == TokenType::kw_extern || ttype == TokenType::kw_fn) {
            if (auto fn = parse_function_decl()) {
                declarations.emplace_back(std::move(fn));
                continue;
            }
        } else if (ttype == TokenType::kw_const || ttype == TokenType::kw_let) {
            if (auto st = parse_decl_stmt()) {
                declarations.emplace_back(std::move(st));
                continue;
            }
        } else if (ttype == TokenType::kw_struct || ttype == TokenType::kw_packed) {
            if (auto st = parse_struct_decl()) {
                declarations.emplace_back(std::move(st));
                continue;
            }
        } else if (ttype == TokenType::kw_module) {
            if (auto st = parse_module_decl()) {
                declarations.emplace_back(std::move(st));
                continue;
            }
        } else if (ttype == TokenType::kw_test) {
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

ptr<TestDecl> Parser::parse_test_decl() {
    auto location = m_nextToken.loc;
    matchOrReturn(TokenType::kw_test, "expected 'test'");
    eat_next_token();  // eat test

    matchOrReturn(TokenType::lit_string, "expected string literal");
    auto name = m_nextToken.str;
    name = name.substr(1, name.size() - 2);
    eat_next_token();  // eat name

    matchOrReturn(TokenType::block_l, "expected function body");
    varOrReturn(block, parse_block());

    return makePtr<TestDecl>(location, name, std::move(block));
}

ptr<CaptureDecl> Parser::parse_capture_decl() {
    auto location = m_nextToken.loc;
    auto identifier = m_nextToken.str;
    matchOrReturn(TokenType::id, "expected identifier");
    eat_next_token();  // eat id

    return makePtr<CaptureDecl>(location, identifier);
}
}  // namespace DMZ