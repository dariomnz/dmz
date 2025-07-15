#include "parser/Parser.hpp"

namespace DMZ {

std::unique_ptr<GenericTypeDecl> Parser::parse_generic_type_decl() {
    matchOrReturn(TokenType::id, "expected identifier");
    auto location = m_nextToken.loc;
    auto identifier = m_nextToken.str;
    eat_next_token();  // eat id
    return std::make_unique<GenericTypeDecl>(location, identifier);
}

std::unique_ptr<GenericTypesDecl> Parser::parse_generic_types_decl() {
    if (m_nextToken.type != TokenType::op_less) {
        return nullptr;
    }
    auto typesDeclList = (parse_list_with_trailing_comma<GenericTypeDecl>(
        {TokenType::op_less, "expected '<'"}, &Parser::parse_generic_type_decl, {TokenType::op_more, "expected '>'"}));
    if (!typesDeclList) return nullptr;

    return std::make_unique<GenericTypesDecl>(std::move(*typesDeclList));
}

// <functionDecl>
//  ::= 'extern'? 'fn' <identifier> '(' ')' '->' <type> <block>
std::unique_ptr<FuncDecl> Parser::parse_function_decl() {
    SourceLocation loc = m_nextToken.loc;
    SourceLocation structLocation;
    std::string_view structIdentifier;
    bool isExtern = false;
    bool isMember = false;

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

    if (m_nextToken.type == TokenType::dot) {
        isMember = true;
        eat_next_token();  // eat '.'
        matchOrReturn(TokenType::id, "expected identifier");
        structIdentifier = functionIdentifier;
        functionIdentifier = m_nextToken.str;
        eat_next_token();  // eat identifier
    }

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

    auto funcDecl = std::make_unique<FunctionDecl>(loc, functionIdentifier, *type, std::move(*parameterList),
                                                   std::move(block), std::move(genericTypes));

    if (isMember) {
        auto declRefExpr = Type::customType(structIdentifier);
        return std::make_unique<MemberFunctionDecl>(loc, functionIdentifier, std::move(declRefExpr),
                                                    std::move(funcDecl));
    }
    return funcDecl;
}

// <paramDecl>
//  ::= 'const'? <identifier> ':' <type>
std::unique_ptr<ParamDecl> Parser::parse_param_decl() {
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
    SourceLocation location = m_nextToken.loc;
    eat_next_token();  // eat struct

    matchOrReturn(TokenType::id, "expected identifier");

    std::string_view structIdentifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    auto genericTypes = parse_generic_types_decl();

    varOrReturn(fieldList, parse_list_with_trailing_comma<FieldDecl>({TokenType::block_l, "expected '{'"},
                                                                     &Parser::parse_field_decl,
                                                                     {TokenType::block_r, "expected '}'"}));

    return std::make_unique<StructDecl>(location, structIdentifier, std::move(*fieldList), std::move(genericTypes));
}

// <fieldDecl>
//  ::= <identifier> ':' <type>
std::unique_ptr<FieldDecl> Parser::parse_field_decl() {
    matchOrReturn(TokenType::id, "expected field declaration");

    SourceLocation location = m_nextToken.loc;
    // assert(nextToken.value && "identifier token without value");

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat :

    varOrReturn(type, parse_type());

    return std::make_unique<FieldDecl>(location, std::move(identifier), std::move(*type));
};

std::unique_ptr<ErrGroupDecl> Parser::parse_err_group_decl() {
    matchOrReturn(TokenType::kw_err, "expected 'err'");
    auto location = m_nextToken.loc;
    auto id = m_nextToken.str;

    eat_next_token();  // eat err

    varOrReturn(errs,
                parse_list_with_trailing_comma<ErrDecl>({TokenType::block_l, "expected '{'"}, &Parser::parse_err_decl,
                                                        {TokenType::block_r, "expected '}'"}));
    return std::make_unique<ErrGroupDecl>(location, id, std::move(*errs));
}

std::unique_ptr<ErrDecl> Parser::parse_err_decl() {
    matchOrReturn(TokenType::id, "expected identifier");
    auto location = m_nextToken.loc;
    auto id = m_nextToken.str;

    eat_next_token();  // eat id
    return std::make_unique<ErrDecl>(location, id);
}

std::unique_ptr<ModuleDecl> Parser::parse_module_decl(bool haveEatModule) {
    auto location = m_nextToken.loc;
    if (!haveEatModule) {
        matchOrReturn(TokenType::kw_module, "expected 'module'");
        eat_next_token();  // eat module
    }

    matchOrReturn(TokenType::id, "expected identifier");
    auto identifier = m_nextToken.str;
    eat_next_token();      // eat identifier

    if (m_nextToken.type == TokenType::coloncolon) {
        eat_next_token();  // eat ::
        varOrReturn(nestedModule, parse_module_decl(true));
        return std::make_unique<ModuleDecl>(location, identifier, std::move(nestedModule));
    }

    matchOrReturn(TokenType::semicolon, "expected ';'");
    eat_next_token();  // eat ;

    auto declarations = parse_in_module_decl();

    return std::make_unique<ModuleDecl>(location, identifier, nullptr, std::move(declarations));
}

std::vector<std::unique_ptr<Decl>> Parser::parse_in_module_decl() {
    std::vector<std::unique_ptr<Decl>> declarations;

    while (m_nextToken.type != TokenType::eof && m_nextToken.type != TokenType::kw_module) {
        if (m_nextToken.type == TokenType::kw_extern || m_nextToken.type == TokenType::kw_fn) {
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
            report(m_nextToken.loc, "expected function, struct, err declaration inside a module");
        }

        synchronize_on({TokenType::kw_extern, TokenType::kw_fn, TokenType::kw_struct, TokenType::kw_err});
        continue;
    }

    return declarations;
}

std::unique_ptr<ImportDecl> Parser::parse_import_decl(bool haveEatImport) {
    auto location = m_nextToken.loc;
    if (!haveEatImport) {
        matchOrReturn(TokenType::kw_import, "expected 'import'");
        eat_next_token();  // eat import
    }

    matchOrReturn(TokenType::id, "expected identifier");
    auto identifier = m_nextToken.str;
    eat_next_token();      // eat identifier

    if (m_nextToken.type == TokenType::coloncolon) {
        eat_next_token();  // eat ::
        varOrReturn(nestedImport, parse_import_decl(true));
        return std::make_unique<ImportDecl>(location, identifier, std::move(nestedImport));
    }

    std::string_view alias = "";
    if (m_nextToken.type == TokenType::kw_as) {
        eat_next_token();  // eat as
        matchOrReturn(TokenType::id, "expected identifier");
        alias = m_nextToken.str;
        eat_next_token();  // eat id
    }

    matchOrReturn(TokenType::semicolon, "expected ';'");
    eat_next_token();  // eat ;

    return std::make_unique<ImportDecl>(location, identifier, nullptr, alias);
}
}  // namespace DMZ