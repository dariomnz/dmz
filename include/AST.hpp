#pragma once

#include "Lexer.hpp"
#include "PH.hpp"
#include "Utils.hpp"

namespace C {

struct Type {
    enum class Kind { Void, Int, Char, Custom };

    Kind kind;
    std::string_view name;

    static Type builtinVoid() { return {Kind::Void, "void"}; }
    static Type builtinInt() { return {Kind::Int, "int"}; }
    static Type builtinChar() { return {Kind::Char, "char"}; }
    static Type custom(const std::string_view &name) { return {Kind::Custom, name}; }
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

struct BinaryOperator : public Expr {
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    TokenType op;

    BinaryOperator(SourceLocation location, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs, TokenType op)
        : Expr(location), lhs(std::move(lhs)), rhs(std::move(rhs)), op(op) {}

    void dump(size_t level = 0) const override;
};

struct UnaryOperator : public Expr {
    std::unique_ptr<Expr> operand;
    TokenType op;

    UnaryOperator(SourceLocation location, std::unique_ptr<Expr> operand, TokenType op)
        : Expr(location), operand(std::move(operand)), op(op) {}

    void dump(size_t level = 0) const override;
};

struct GroupingExpr : public Expr {
    std::unique_ptr<Expr> expr;

    GroupingExpr(SourceLocation location, std::unique_ptr<Expr> expr) : Expr(location), expr(std::move(expr)) {}

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

template <typename Ty>
class ConstantValueContainer {
    std::optional<Ty> value = std::nullopt;

   public:
    void set_constant_value(std::optional<Ty> val) { value = std::move(val); }
    std::optional<Ty> get_constant_value() const { return value; }
};

struct IfStmt : public Statement {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Block> trueBlock;
    std::unique_ptr<Block> falseBlock;

    IfStmt(SourceLocation location, std::unique_ptr<Expr> condition, std::unique_ptr<Block> trueBlock,
           std::unique_ptr<Block> falseBlock = nullptr)
        : Statement(location),
          condition(std::move(condition)),
          trueBlock(std::move(trueBlock)),
          falseBlock(std::move(falseBlock)) {}

    void dump(size_t level = 0) const override;
};

struct WhileStmt : public Statement {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Block> body;

    WhileStmt(SourceLocation location, std::unique_ptr<Expr> condition, std::unique_ptr<Block> body)
        : Statement(location), condition(std::move(condition)), body(std::move(body)) {}

    void dump(size_t level = 0) const override;
};

struct ResolvedStmt {
    SourceLocation location;

    ResolvedStmt(SourceLocation location) : location(location) {}

    virtual ~ResolvedStmt() = default;

    virtual void dump(size_t level = 0) const = 0;
};

struct ResolvedExpr : public ConstantValueContainer<int>, public ResolvedStmt {
    Type type;

    ResolvedExpr(SourceLocation location, Type type) : ResolvedStmt(location), type(type) {}
};

struct ResolvedDecl {
    SourceLocation location;
    std::string_view identifier;
    Type type;

    ResolvedDecl(SourceLocation location, std::string_view identifier, Type type)
        : location(location), identifier(std::move(identifier)), type(type) {}
    virtual ~ResolvedDecl() = default;

    virtual void dump(size_t level = 0) const = 0;
};

struct ResolvedNumberLiteral : public ResolvedExpr {
    int value;

    ResolvedNumberLiteral(SourceLocation location, int value)
        : ResolvedExpr(location, Type::builtinInt()), value(value) {}

    void dump(size_t level = 0) const override;
};

struct ResolvedDeclRefExpr : public ResolvedExpr {
    const ResolvedDecl &decl;

    ResolvedDeclRefExpr(SourceLocation location, ResolvedDecl &decl) : ResolvedExpr(location, decl.type), decl(decl) {}

    void dump(size_t level = 0) const override;
};

struct ResolvedBlock {
    SourceLocation location;
    std::vector<std::unique_ptr<ResolvedStmt>> statements;

    ResolvedBlock(SourceLocation location, std::vector<std::unique_ptr<ResolvedStmt>> statements)
        : location(location), statements(std::move(statements)) {}

    void dump(size_t level = 0) const;
};

struct ResolvedParamDecl : public ResolvedDecl {
    ResolvedParamDecl(SourceLocation location, std::string_view identifier, Type type)
        : ResolvedDecl{location, std::move(identifier), type} {}

    void dump(size_t level = 0) const override;
};

struct ResolvedFunctionDecl : public ResolvedDecl {
    std::vector<std::unique_ptr<ResolvedParamDecl>> params;
    std::unique_ptr<ResolvedBlock> body;

    ResolvedFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                         std::vector<std::unique_ptr<ResolvedParamDecl>> params, std::unique_ptr<ResolvedBlock> body)
        : ResolvedDecl(location, std::move(identifier), type), params(std::move(params)), body(std::move(body)) {}

    void dump(size_t level = 0) const override;
};

struct ResolvedCallExpr : public ResolvedExpr {
    const ResolvedFunctionDecl &callee;
    std::vector<std::unique_ptr<ResolvedExpr>> arguments;

    ResolvedCallExpr(SourceLocation location, const ResolvedFunctionDecl &callee,
                     std::vector<std::unique_ptr<ResolvedExpr>> arguments)
        : ResolvedExpr(location, callee.type), callee(callee), arguments(std::move(arguments)) {}

    void dump(size_t level = 0) const override;
};

struct ResolvedReturnStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedExpr> expr;

    ResolvedReturnStmt(SourceLocation location, std::unique_ptr<ResolvedExpr> expr = nullptr)
        : ResolvedStmt(location), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
};

struct ResolvedBinaryOperator : public ResolvedExpr {
    TokenType op;
    std::unique_ptr<ResolvedExpr> lhs;
    std::unique_ptr<ResolvedExpr> rhs;

    ResolvedBinaryOperator(SourceLocation location, TokenType op, std::unique_ptr<ResolvedExpr> lhs,
                           std::unique_ptr<ResolvedExpr> rhs)
        : ResolvedExpr(location, lhs->type), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    void dump(size_t level = 0) const override;
};

struct ResolvedUnaryOperator : public ResolvedExpr {
    TokenType op;
    std::unique_ptr<ResolvedExpr> operand;

    ResolvedUnaryOperator(SourceLocation location, TokenType op, std::unique_ptr<ResolvedExpr> operand)
        : ResolvedExpr(location, operand->type), op(op), operand(std::move(operand)) {}

    void dump(size_t level = 0) const override;
};

struct ResolvedGroupingExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> expr;

    ResolvedGroupingExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> expr)
        : ResolvedExpr(location, expr->type), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
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

    void dump(size_t level = 0) const override;
};

struct ResolvedWhileStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedExpr> condition;
    std::unique_ptr<ResolvedBlock> body;

    ResolvedWhileStmt(SourceLocation location, std::unique_ptr<ResolvedExpr> condition,
                      std::unique_ptr<ResolvedBlock> body)
        : ResolvedStmt(location), condition(std::move(condition)), body(std::move(body)) {}

    void dump(size_t level = 0) const override;
};
}  // namespace C
