#pragma once

#include <memory>
#include <vector>

#include "Lexer.hpp"

namespace C {

struct Type {
    enum class Kind { Void, Int, Char, Custom };

    Kind kind;
    std::string name;

    static Type builtinVoid() { return {Kind::Void, "void"}; }
    static Type builtinInt() { return {Kind::Int, "int"}; }
    static Type builtinChar() { return {Kind::Char, "char"}; }
    static Type custom(const std::string &name) { return {Kind::Custom, name}; }
};

struct Decl {
    SourceLocation location;
    std::string_view identifier;

    Decl(SourceLocation location, std::string_view identifier)
        : location(location), identifier(std::move(identifier)) {}
    virtual ~Decl() = default;

    virtual void dump(size_t level = 0) const = 0;
};

struct ParamDecl : public Decl {
    Type type;

    ParamDecl(SourceLocation location, std::string_view identifier, Type type)
        : Decl(location, std::move(identifier)), type(std::move(type)) {}

    void dump(size_t level = 0) const override;
};

struct Statement {
    SourceLocation location;

    Statement(SourceLocation location) : location(location) {}

    virtual ~Statement() = default;

    virtual void dump(size_t level = 0) const = 0;
};

struct Block {
    SourceLocation location;
    std::vector<std::unique_ptr<Statement>> statements;

    Block(SourceLocation location, std::vector<std::unique_ptr<Statement>> statements)
        : location(location), statements(std::move(statements)) {}

    void dump(size_t level = 0) const;
};

struct FunctionDecl : public Decl {
    Type type;
    std::vector<std::unique_ptr<ParamDecl>> params;
    std::unique_ptr<Block> body;

    FunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                 std::vector<std::unique_ptr<ParamDecl>> params, std::unique_ptr<Block> body)
        : Decl(location, std::move(identifier)),
          type(std::move(type)),
          params(std::move(params)),
          body(std::move(body)) {}

    void dump(size_t level = 0) const override;
};

struct Expr : public Statement {
    Expr(SourceLocation location) : Statement(location) {}
};

struct ReturnStmt : public Statement {
    std::unique_ptr<Expr> expr;

    ReturnStmt(SourceLocation location, std::unique_ptr<Expr> expr = nullptr)
        : Statement(location), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
};

struct NumberLiteral : public Expr {
    std::string_view value;

    NumberLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
};

struct DeclRefExpr : public Expr {
    std::string_view identifier;

    DeclRefExpr(SourceLocation location, std::string_view identifier) : Expr(location), identifier(identifier) {}

    void dump(size_t level = 0) const override;
};

struct CallExpr : public Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;

    CallExpr(SourceLocation location, std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> arguments)
        : Expr(location), callee(std::move(callee)), arguments(std::move(arguments)) {}

    void dump(size_t level = 0) const override;
};
}  // namespace C
