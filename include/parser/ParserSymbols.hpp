#pragma once

#include <memory>
#include <optional>
#include <variant>

#include "Debug.hpp"
#include "Stats.hpp"
#include "Utils.hpp"
#include "lexer/Lexer.hpp"

namespace DMZ {

[[maybe_unused]] static inline std::string_view get_op_str(TokenType op) {
    if (op == TokenType::op_plus) return "+";
    if (op == TokenType::op_minus) return "-";
    if (op == TokenType::op_mult) return "*";
    if (op == TokenType::op_div) return "/";
    if (op == TokenType::amp) return "&";

    if (op == TokenType::op_not_equal) return "!=";
    if (op == TokenType::op_equal) return "==";
    if (op == TokenType::op_and) return "&&";
    if (op == TokenType::op_or) return "||";
    if (op == TokenType::op_less) return "<";
    if (op == TokenType::op_less_eq) return "<=";
    if (op == TokenType::op_more) return ">";
    if (op == TokenType::op_more_eq) return ">=";
    if (op == TokenType::op_not) return "!";

    dmz_unreachable("unexpected operator");
}

struct Type {
    enum class Kind { Void, Int, Char, Struct, Custom };

    Kind kind;
    std::string_view name;
    std::optional<int> isArray = std::nullopt;
    bool isRef = false;

    static Type builtinVoid() { return {Kind::Void, "void"}; }
    static Type builtinInt() { return {Kind::Int, "int"}; }
    static Type builtinChar() { return {Kind::Char, "char"}; }
    static Type builtinString(int size) { return Type{.kind = Kind::Char, .name = "char", .isArray = size}; }
    static Type custom(const std::string_view& name) { return {Kind::Custom, name}; }
    static Type structType(const std::string_view& name) { return {Kind::Struct, name}; }
    static Type structType(Type t) {
        t.kind = Kind::Struct;
        return t;
    }

    static bool compare(const Type& lhs, const Type& rhs) {
        bool equalArray = false;

        equalArray |= (lhs.isArray && *lhs.isArray == 0);
        equalArray |= (rhs.isArray && *rhs.isArray == 0);
        equalArray |= (lhs.isArray == rhs.isArray);
        equalArray |= (lhs.isRef && rhs.isRef);

        return lhs.kind == rhs.kind && lhs.name == rhs.name && equalArray;
    }

    void dump() const;
    std::string to_str() const;
};

template <typename Ty>
class ConstantValueContainer {
   protected:
    std::optional<Ty> value = std::nullopt;

   public:
    void set_constant_value(std::optional<Ty> val) { value = std::move(val); }
    std::optional<Ty> get_constant_value() const { return value; }
};

struct Decl {
    SourceLocation location;
    std::string_view identifier;

    Decl(SourceLocation location, std::string_view identifier)
        : location(location), identifier(std::move(identifier)) {}
    virtual ~Decl() = default;

    virtual void dump(size_t level = 0) const = 0;
};

struct Stmt {
    SourceLocation location;
    Stmt(SourceLocation location) : location(location) {}

    virtual ~Stmt() = default;

    virtual void dump(size_t level = 0) const = 0;
};

struct Expr : public Stmt {
    Expr(SourceLocation location) : Stmt(location) {}
};

struct Block {
    SourceLocation location;
    std::vector<std::unique_ptr<Stmt>> statements;

    Block(SourceLocation location, std::vector<std::unique_ptr<Stmt>> statements)
        : location(location), statements(std::move(statements)) {}

    void dump(size_t level = 0) const;
};

struct IfStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Block> trueBlock;
    std::unique_ptr<Block> falseBlock;

    IfStmt(SourceLocation location, std::unique_ptr<Expr> condition, std::unique_ptr<Block> trueBlock,
           std::unique_ptr<Block> falseBlock = nullptr)
        : Stmt(location),
          condition(std::move(condition)),
          trueBlock(std::move(trueBlock)),
          falseBlock(std::move(falseBlock)) {}

    void dump(size_t level = 0) const override;
};

struct WhileStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Block> body;

    WhileStmt(SourceLocation location, std::unique_ptr<Expr> condition, std::unique_ptr<Block> body)
        : Stmt(location), condition(std::move(condition)), body(std::move(body)) {}

    void dump(size_t level = 0) const override;
};

struct ReturnStmt : public Stmt {
    std::unique_ptr<Expr> expr;

    ReturnStmt(SourceLocation location, std::unique_ptr<Expr> expr = nullptr) : Stmt(location), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
};

struct FieldInitStmt : public Stmt {
    std::string_view identifier;
    std::unique_ptr<Expr> initializer;

    FieldInitStmt(SourceLocation location, std::string_view identifier, std::unique_ptr<Expr> initializer)
        : Stmt(location), identifier(identifier), initializer(std::move(initializer)) {}

    void dump(size_t level = 0) const override;
};

struct StructInstantiationExpr : public Expr {
    std::string_view identifier;
    std::vector<std::unique_ptr<FieldInitStmt>> fieldInitializers;

    StructInstantiationExpr(SourceLocation location, std::string_view identifier,
                            std::vector<std::unique_ptr<FieldInitStmt>> fieldInitializers)
        : Expr(location), identifier(identifier), fieldInitializers(std::move(fieldInitializers)) {}

    void dump(size_t level = 0) const override;
};

struct IntLiteral : public Expr {
    std::string_view value;

    IntLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
};

struct CharLiteral : public Expr {
    std::string_view value;

    CharLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
};

struct StringLiteral : public Expr {
    std::string_view value;

    StringLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
};

struct CallExpr : public Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;

    CallExpr(SourceLocation location, std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> arguments)
        : Expr(location), callee(std::move(callee)), arguments(std::move(arguments)) {}

    void dump(size_t level = 0) const override;
};

struct AssignableExpr : public Expr {
    AssignableExpr(SourceLocation location) : Expr(location) {}
};

struct DeclRefExpr : public AssignableExpr {
    std::string_view identifier;

    DeclRefExpr(SourceLocation location, std::string_view identifier)
        : AssignableExpr(location), identifier(identifier) {}

    void dump(size_t level = 0) const override;
};

struct MemberExpr : public AssignableExpr {
    std::unique_ptr<Expr> base;
    std::string_view field;

    MemberExpr(SourceLocation location, std::unique_ptr<Expr> base, std::string_view field)
        : AssignableExpr(location), base(std::move(base)), field(std::move(field)) {}

    void dump(size_t level = 0) const override;
};

struct GroupingExpr : public Expr {
    std::unique_ptr<Expr> expr;

    GroupingExpr(SourceLocation location, std::unique_ptr<Expr> expr) : Expr(location), expr(std::move(expr)) {}

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

struct FieldDecl : public Decl {
    Type type;

    FieldDecl(SourceLocation location, std::string_view identifier, Type type)
        : Decl(location, std::move(identifier)), type(std::move(type)) {}

    void dump(size_t level = 0) const override;
};

struct StructDecl : public Decl {
    std::vector<std::unique_ptr<FieldDecl>> fields;

    StructDecl(SourceLocation location, std::string_view identifier, std::vector<std::unique_ptr<FieldDecl>> fields)
        : Decl(location, std::move(identifier)), fields(std::move(fields)) {}

    void dump(size_t level = 0) const override;
};

struct ParamDecl : public Decl {
    Type type;
    bool isMutable;
    bool isVararg = false;

    ParamDecl(SourceLocation location, std::string_view identifier, Type type, bool isMutable, bool isVararg = false)
        : Decl(location, std::move(identifier)), type(std::move(type)), isMutable(isMutable), isVararg(isVararg) {}

    void dump(size_t level = 0) const override;
};

struct VarDecl : public Decl {
    std::optional<Type> type;
    std::unique_ptr<Expr> initializer;
    bool isMutable;

    VarDecl(SourceLocation location, std::string_view identifier, std::optional<Type> type, bool isMutable,
            std::unique_ptr<Expr> initializer = nullptr)
        : Decl(location, std::move(identifier)),
          type(std::move(type)),
          initializer(std::move(initializer)),
          isMutable(isMutable) {}

    void dump(size_t level = 0) const override;
};

struct FuncDecl : public Decl {
    Type type;
    std::vector<std::unique_ptr<ParamDecl>> params;

    FuncDecl(SourceLocation location, std::string_view identifier, Type type,
             std::vector<std::unique_ptr<ParamDecl>> params)
        : Decl(location, std::move(identifier)), type(std::move(type)), params(std::move(params)) {}
};

struct ExternFunctionDecl : public FuncDecl {
    ExternFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                       std::vector<std::unique_ptr<ParamDecl>> params)
        : FuncDecl(location, std::move(identifier), std::move(type), std::move(params)) {}

    void dump(size_t level = 0) const override;
};

struct FunctionDecl : public FuncDecl {
    std::unique_ptr<Block> body;

    FunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                 std::vector<std::unique_ptr<ParamDecl>> params, std::unique_ptr<Block> body)
        : FuncDecl(location, std::move(identifier), std::move(type), std::move(params)), body(std::move(body)) {}

    void dump(size_t level = 0) const override;
};

struct DeclStmt : public Stmt {
    std::unique_ptr<VarDecl> varDecl;

    DeclStmt(SourceLocation location, std::unique_ptr<VarDecl> varDecl) : Stmt(location), varDecl(std::move(varDecl)) {}

    void dump(size_t level = 0) const override;
};

struct Assignment : public Stmt {
    std::unique_ptr<AssignableExpr> assignee;
    std::unique_ptr<Expr> expr;

    Assignment(SourceLocation location, std::unique_ptr<AssignableExpr> assignee, std::unique_ptr<Expr> expr)
        : Stmt(location), assignee(std::move(assignee)), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
};
}  // namespace DMZ
