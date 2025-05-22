#include "parser/Parser.hpp"

namespace DMZ {

// <functionDecl>
//  ::= 'extern'? 'fn' <identifier> '(' ')' '->' <type> <block>
std::unique_ptr<FuncDecl> Parser::parse_function_decl() {
    SourceLocation loc = m_nextToken.loc;
    bool isExtern = false;

    if (m_nextToken.type == TokenType::kw_extern) {
        isExtern = true;
        eat_next_token();  // eat extern
    }

    matchOrReturn(TokenType::kw_fn, "expected 'fn'");

    eat_next_token();  // eat fn

    matchOrReturn(TokenType::id, "expected identifier");

    std::string_view functionIdentifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    varOrReturn(parameterList,
                parse_list_with_trailing_comma<ParamDecl>({TokenType::par_l, "expected '('"}, &Parser::parse_param_decl,
                                                          {TokenType::par_r, "expected ')'"}));

    matchOrReturn(TokenType::return_type, "expected '->'");
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

    return std::make_unique<FunctionDecl>(loc, functionIdentifier, *type, std::move(*parameterList), std::move(block));
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

    bool isConst = m_nextToken.type == TokenType::kw_const;
    if (isConst) eat_next_token();  // eat 'const'

    matchOrReturn(TokenType::id, "expected parameter declaration");
    // assert(nextToken.value && "identifier token without value");

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    matchOrReturn(TokenType::colon, "expected ':'");
    eat_next_token();  // eat :

    varOrReturn(type, parse_type());

    return std::make_unique<ParamDecl>(location, std::move(identifier), std::move(*type), !isConst);
}

std::unique_ptr<VarDecl> Parser::parse_var_decl(bool isConst) {
    SourceLocation location = m_nextToken.loc;

    std::string_view identifier = m_nextToken.str;
    eat_next_token();  // eat identifier

    std::optional<Type> type;
    if (m_nextToken.type == TokenType::colon) {
        eat_next_token();  // eat ':'

        type = parse_type();
        if (!type) return nullptr;
    }

    if (m_nextToken.type != TokenType::op_assign) {
        return std::make_unique<VarDecl>(location, identifier, type, !isConst);
    }
    eat_next_token();  // eat '='

    varOrReturn(initializer, parse_expr());

    return std::make_unique<VarDecl>(location, identifier, type, !isConst, std::move(initializer));
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

    varOrReturn(fieldList, parse_list_with_trailing_comma<FieldDecl>({TokenType::block_l, "expected '{'"},
                                                                     &Parser::parse_field_decl,
                                                                     {TokenType::block_r, "expected '}'"}));

    return std::make_unique<StructDecl>(location, structIdentifier, std::move(*fieldList));
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

    return std::make_unique<ModuleDecl>(location, identifier);
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

    matchOrReturn(TokenType::semicolon, "expected ';'");
    eat_next_token();  // eat ;

    return std::make_unique<ImportDecl>(location, identifier);
}
}  // namespace DMZ