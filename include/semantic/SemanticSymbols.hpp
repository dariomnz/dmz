#pragma once

#include <memory>
#include <optional>
#include <variant>

#include "Debug.hpp"
#include "Stats.hpp"
#include "Utils.hpp"
#include "lexer/Lexer.hpp"
#include "parser/ParserSymbols.hpp"

namespace DMZ {

struct ResolvedStmt {
    SourceLocation location;

    ResolvedStmt(SourceLocation location) : location(location) {}

    virtual ~ResolvedStmt() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
};

using ConstValue = std::variant<int, char, bool>;

struct ResolvedExpr : public ConstantValueContainer<ConstValue>, public ResolvedStmt {
    Type type;

    ResolvedExpr(SourceLocation location, Type type) : ResolvedStmt(location), type(type) {}

    virtual ~ResolvedExpr() = default;

    void dump_constant_value(size_t level) const;
};

struct ResolvedDecl {
    SourceLocation location;
    std::string_view identifier;
    Type type;
    bool isMutable;

    ResolvedDecl(SourceLocation location, std::string_view identifier, Type type, bool isMutable)
        : location(location), identifier(std::move(identifier)), type(type), isMutable(isMutable) {}
    virtual ~ResolvedDecl() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
};

struct ResolvedBlock : public ResolvedStmt {
    std::vector<std::unique_ptr<ResolvedStmt>> statements;

    ResolvedBlock(SourceLocation location, std::vector<std::unique_ptr<ResolvedStmt>> statements)
        : ResolvedStmt(location), statements(std::move(statements)) {}

    void dump(size_t level = 0, bool onlySelf = false) const;
};

struct ResolvedIfStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedExpr> condition;
    std::unique_ptr<ResolvedBlock> trueBlock;
    std::unique_ptr<ResolvedBlock> falseBlock;

    ResolvedIfStmt(SourceLocation location, std::unique_ptr<ResolvedExpr> condition,
                   std::unique_ptr<ResolvedBlock> trueBlock, std::unique_ptr<ResolvedBlock> falseBlock = nullptr)
        : ResolvedStmt(location),
          condition(std::move(condition)),
          trueBlock(std::move(trueBlock)),
          falseBlock(std::move(falseBlock)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedWhileStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedExpr> condition;
    std::unique_ptr<ResolvedBlock> body;

    ResolvedWhileStmt(SourceLocation location, std::unique_ptr<ResolvedExpr> condition,
                      std::unique_ptr<ResolvedBlock> body)
        : ResolvedStmt(location), condition(std::move(condition)), body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedParamDecl : public ResolvedDecl {
    bool isVararg = false;

    ResolvedParamDecl(SourceLocation location, std::string_view identifier, Type type, bool isMutable,
                      bool isVararg = false)
        : ResolvedDecl(location, std::move(identifier), type, isMutable), isVararg(isVararg) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFieldDecl : public ResolvedDecl {
    unsigned index;

    ResolvedFieldDecl(SourceLocation location, std::string_view identifier, Type type, unsigned index)
        : ResolvedDecl(location, std::move(identifier), type, false), index(index) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedVarDecl : public ResolvedDecl {
    std::unique_ptr<ResolvedExpr> initializer;

    ResolvedVarDecl(SourceLocation location, std::string_view identifier, Type type, bool isMutable,
                    std::unique_ptr<ResolvedExpr> initializer = nullptr)
        : ResolvedDecl(location, std::move(identifier), type, isMutable), initializer(std::move(initializer)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFuncDecl : public ResolvedDecl {
    std::vector<std::unique_ptr<ResolvedParamDecl>> params;

    ResolvedFuncDecl(SourceLocation location, std::string_view identifier, Type type,
                     std::vector<std::unique_ptr<ResolvedParamDecl>> params)
        : ResolvedDecl(location, std::move(identifier), type, false), params(std::move(params)) {}
};

struct ResolvedExternFunctionDecl : public ResolvedFuncDecl {
    ResolvedExternFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                               std::vector<std::unique_ptr<ResolvedParamDecl>> params)
        : ResolvedFuncDecl(location, std::move(identifier), type, std::move(params)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFunctionDecl : public ResolvedFuncDecl {
    std::unique_ptr<ResolvedBlock> body;

    ResolvedFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                         std::vector<std::unique_ptr<ResolvedParamDecl>> params, std::unique_ptr<ResolvedBlock> body)
        : ResolvedFuncDecl(location, std::move(identifier), type, std::move(params)), body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedStructDecl : public ResolvedDecl {
    std::vector<std::unique_ptr<ResolvedFieldDecl>> fields;

    ResolvedStructDecl(SourceLocation location, std::string_view identifier, Type type,
                       std::vector<std::unique_ptr<ResolvedFieldDecl>> fields)
        : ResolvedDecl(location, std::move(identifier), type, false), fields(std::move(fields)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedIntLiteral : public ResolvedExpr {
    int value;

    ResolvedIntLiteral(SourceLocation location, int value) : ResolvedExpr(location, Type::builtinInt()), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCharLiteral : public ResolvedExpr {
    char value;

    ResolvedCharLiteral(SourceLocation location, char value)
        : ResolvedExpr(location, Type::builtinChar()), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedBoolLiteral : public ResolvedExpr {
    bool value;

    ResolvedBoolLiteral(SourceLocation location, bool value)
        : ResolvedExpr(location, Type::builtinBool()), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedStringLiteral : public ResolvedExpr {
    std::string value;

    ResolvedStringLiteral(SourceLocation location, std::string_view value)
        : ResolvedExpr(location, Type::builtinString(value.size() + 1)), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCallExpr : public ResolvedExpr {
    const ResolvedFuncDecl &callee;
    std::vector<std::unique_ptr<ResolvedExpr>> arguments;

    ResolvedCallExpr(SourceLocation location, const ResolvedFuncDecl &callee,
                     std::vector<std::unique_ptr<ResolvedExpr>> arguments)
        : ResolvedExpr(location, callee.type), callee(callee), arguments(std::move(arguments)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedAssignableExpr : public ResolvedExpr {
    ResolvedAssignableExpr(SourceLocation location, Type type) : ResolvedExpr(location, type) {}
};

struct ResolvedDeclRefExpr : public ResolvedAssignableExpr {
    const ResolvedDecl &decl;

    ResolvedDeclRefExpr(SourceLocation location, ResolvedDecl &decl)
        : ResolvedAssignableExpr(location, decl.type), decl(decl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedMemberExpr : public ResolvedAssignableExpr {
    std::unique_ptr<ResolvedExpr> base;
    const ResolvedFieldDecl &field;

    ResolvedMemberExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> base, const ResolvedFieldDecl &field)
        : ResolvedAssignableExpr(location, field.type), base(std::move(base)), field(field) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedGroupingExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> expr;

    ResolvedGroupingExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> expr)
        : ResolvedExpr(location, expr->type), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedBinaryOperator : public ResolvedExpr {
    TokenType op;
    std::unique_ptr<ResolvedExpr> lhs;
    std::unique_ptr<ResolvedExpr> rhs;

    ResolvedBinaryOperator(SourceLocation location, TokenType op, std::unique_ptr<ResolvedExpr> lhs,
                           std::unique_ptr<ResolvedExpr> rhs)
        : ResolvedExpr(location, lhs->type), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedUnaryOperator : public ResolvedExpr {
    TokenType op;
    std::unique_ptr<ResolvedExpr> operand;

    ResolvedUnaryOperator(SourceLocation location, TokenType op, std::unique_ptr<ResolvedExpr> operand)
        : ResolvedExpr(location, operand->type), op(op), operand(std::move(operand)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedDeclStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedVarDecl> varDecl;

    ResolvedDeclStmt(SourceLocation location, std::unique_ptr<ResolvedVarDecl> varDecl)
        : ResolvedStmt(location), varDecl(std::move(varDecl)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedAssignment : public ResolvedStmt {
    std::unique_ptr<ResolvedAssignableExpr> assignee;
    std::unique_ptr<ResolvedExpr> expr;

    ResolvedAssignment(SourceLocation location, std::unique_ptr<ResolvedAssignableExpr> assignee,
                       std::unique_ptr<ResolvedExpr> expr)
        : ResolvedStmt(location), assignee(std::move(assignee)), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedReturnStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedExpr> expr;

    ResolvedReturnStmt(SourceLocation location, std::unique_ptr<ResolvedExpr> expr = nullptr)
        : ResolvedStmt(location), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFieldInitStmt : public ResolvedStmt {
    const ResolvedFieldDecl &field;
    std::unique_ptr<ResolvedExpr> initializer;

    ResolvedFieldInitStmt(SourceLocation location, const ResolvedFieldDecl &field,
                          std::unique_ptr<ResolvedExpr> initializer)
        : ResolvedStmt(location), field(field), initializer(std::move(initializer)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedStructInstantiationExpr : public ResolvedExpr {
    const ResolvedStructDecl &structDecl;
    std::vector<std::unique_ptr<ResolvedFieldInitStmt>> fieldInitializers;

    ResolvedStructInstantiationExpr(SourceLocation location, const ResolvedStructDecl &structDecl,
                                    std::vector<std::unique_ptr<ResolvedFieldInitStmt>> fieldInitializers)
        : ResolvedExpr(location, structDecl.type),
          structDecl(structDecl),
          fieldInitializers(std::move(fieldInitializers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedDeferStmt : public ResolvedStmt {
    std::shared_ptr<ResolvedBlock> block;

    ResolvedDeferStmt(SourceLocation location, std::shared_ptr<ResolvedBlock> block)
        : ResolvedStmt(location), block(block) {}

    ResolvedDeferStmt(const ResolvedDeferStmt &deferStmt) : ResolvedStmt(deferStmt.location), block(deferStmt.block) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrDecl : public ResolvedDecl {
    ResolvedErrDecl(SourceLocation location, std::string_view identifier)
        : ResolvedDecl(location, std::move(identifier), Type::builtinErr(identifier), false) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrDeclRefExpr : public ResolvedExpr {
    const ResolvedErrDecl &decl;
    ResolvedErrDeclRefExpr(SourceLocation location, const ResolvedErrDecl &decl)
        : ResolvedExpr(location, decl.type), decl(decl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrGroupDecl : public ResolvedDecl {
    std::vector<std::unique_ptr<ResolvedErrDecl>> errs;

    ResolvedErrGroupDecl(SourceLocation location, std::string_view identifier,
                         std::vector<std::unique_ptr<ResolvedErrDecl>> errs)
        : ResolvedDecl(location, std::move(identifier), Type::builtinErr("errGroup"), false), errs(std::move(errs)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrUnwrapExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> errToUnwrap;

    ResolvedErrUnwrapExpr(SourceLocation location, Type unwrapType, std::unique_ptr<ResolvedExpr> errToUnwrap)
        : ResolvedExpr(location, unwrapType), errToUnwrap(std::move(errToUnwrap)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCatchErrExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> errToCatch;
    std::unique_ptr<ResolvedDeclStmt> declaration;

    ResolvedCatchErrExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> errToCatch,
                         std::unique_ptr<ResolvedDeclStmt> declaration)
        : ResolvedExpr(location, Type::builtinInt()),
          errToCatch(std::move(errToCatch)),
          declaration(std::move(declaration)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTryErrExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> errToTry;
    std::unique_ptr<ResolvedDeclStmt> declaration;

    ResolvedTryErrExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> errToTry,
                         std::unique_ptr<ResolvedDeclStmt> declaration)
        : ResolvedExpr(location, Type::builtinInt()),
          errToTry(std::move(errToTry)),
          declaration(std::move(declaration)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};
}  // namespace DMZ
