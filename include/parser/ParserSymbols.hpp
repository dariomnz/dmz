#pragma once

#include "DMZPCH.hpp"
#include "lexer/Lexer.hpp"

namespace DMZ {

[[maybe_unused]] static inline std::string_view get_op_str(TokenType op) {
    if (op == TokenType::op_plus) return "+";
    if (op == TokenType::op_minus) return "-";
    if (op == TokenType::asterisk) return "*";
    if (op == TokenType::op_div) return "/";
    if (op == TokenType::op_percent) return "%";
    if (op == TokenType::amp) return "&";

    if (op == TokenType::op_not_equal) return "!=";
    if (op == TokenType::op_equal) return "==";
    if (op == TokenType::ampamp) return "&&";
    if (op == TokenType::pipepipe) return "||";
    if (op == TokenType::op_less) return "<";
    if (op == TokenType::op_less_eq) return "<=";
    if (op == TokenType::op_more) return ">";
    if (op == TokenType::op_more_eq) return ">=";
    if (op == TokenType::op_excla_mark) return "!";
    if (op == TokenType::op_quest_mark) return "?";

    dmz_unreachable("unexpected operator");
}
// Forward declaration
struct Type;

struct GenericTypes {
    std::vector<std::unique_ptr<Type>> types;

    GenericTypes(std::vector<std::unique_ptr<Type>> types) noexcept;

    GenericTypes(const GenericTypes& other);
    GenericTypes& operator=(const GenericTypes& other);
    GenericTypes(GenericTypes&& other) noexcept;

    void dump() const;
    std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& os, const GenericTypes& t);

    bool operator==(const GenericTypes& other) const;
};
struct Type {
    enum class Kind { Void, Int, UInt, Float, Struct, Custom, Generic, Err, Module };

    Kind kind = Kind::Void;
    std::string_view name = "";
    int size = 0;
    std::optional<int> isArray = std::nullopt;
    std::optional<int> isPointer = std::nullopt;
    bool isRef = false;
    bool isOptional = false;
    std::optional<GenericTypes> genericTypes = std::nullopt;

    static Type builtinVoid() { return {Kind::Void, "void"}; }
    static Type builtinBool() { return {Kind::Int, "i1", 1}; }
    static Type builtinIN(const std::string_view& name) {
        auto num = name.substr(1);
        int result = 0;
        auto res = std::from_chars(num.data(), num.data() + num.size(), result);
        if (result == 0 || res.ec != std::errc()) {
            dmz_unreachable("unexpected size of 0 in i type");
        }
        return {Kind::Int, name, result};
    }
    static Type builtinUN(const std::string_view& name) {
        auto num = name.substr(1);
        int result = 0;
        auto res = std::from_chars(num.data(), num.data() + num.size(), result);
        if (result == 0 || res.ec != std::errc()) {
            dmz_unreachable("unexpected size of 0 in u type");
        }
        return {Kind::UInt, name, result};
    }
    static Type builtinF16() { return {Kind::Float, "f16", 16}; }
    static Type builtinF32() { return {Kind::Float, "f32", 32}; }
    static Type builtinF64() { return {Kind::Float, "f64", 64}; }
    static Type builtinString(int size) { return Type{.kind = Kind::UInt, .name = "u8", .size = 8, .isArray = size}; }
    static Type moduleType(const std::string_view& name) { return {Kind::Module, name}; }
    static Type customType(const std::string_view& name) { return {Kind::Custom, name}; }
    static Type structType(const std::string_view& name) { return {Kind::Struct, name}; }
    static Type structType(Type t) {
        if (t.kind != Kind::Custom) dmz_unreachable("expected custom type to convert to struct");
        t.kind = Kind::Struct;
        return t;
    }
    static Type genericType(Type t) {
        if (t.kind != Kind::Custom) dmz_unreachable("expected custom type to convert to generic");
        t.kind = Kind::Generic;
        return t;
    }
    static Type builtinErr(const std::string_view& name) { return {Kind::Err, name}; }

    bool operator==(const Type& otro) const {
        return (kind == otro.kind && name == otro.name && isArray == otro.isArray && isRef == otro.isRef &&
                isOptional == otro.isOptional);
    }

    static bool can_convert(const Type& to, const Type& from) {
        bool canConvert = false;
        canConvert |= from.kind == Type::Kind::Int && to.kind == Type::Kind::Int;
        canConvert |= from.kind == Type::Kind::Int && to.kind == Type::Kind::Float;
        canConvert |= from.kind == Type::Kind::UInt && to.kind == Type::Kind::UInt;
        canConvert |= from.kind == Type::Kind::UInt && to.kind == Type::Kind::Float;
        canConvert |= from.kind == Type::Kind::Float && to.kind == Type::Kind::Int;
        canConvert |= from.kind == Type::Kind::Float && to.kind == Type::Kind::UInt;
        return canConvert;
    }

    static bool compare(const Type& lhs, const Type& rhs) {
        // println("Types: '" << lhs.to_str() << "' '" << rhs.to_str() << "'");
        bool equalArray = false;
        bool equalOptional = false;
        if (can_convert(lhs, rhs) || lhs == rhs) {
            return true;
        }

        if (lhs.isOptional && rhs.kind == Kind::Err) return true;
        if (rhs.isOptional && lhs.kind == Kind::Err) return true;

        equalArray |= (lhs.isArray && *lhs.isArray == 0);
        equalArray |= (rhs.isArray && *rhs.isArray == 0);
        equalArray |= (lhs.isArray == rhs.isArray);
        equalArray |= (lhs.isRef && rhs.isRef);

        equalOptional |= lhs.isOptional == rhs.isOptional;
        equalOptional |= lhs.isOptional == true && rhs.isOptional == false;

        bool equal = equalArray && equalOptional;
        equal &= lhs.kind == rhs.kind;
        if ((lhs.kind == Type::Kind::Struct || lhs.kind == Type::Kind::Custom) &&
            (rhs.kind == Type::Kind::Struct || rhs.kind == Type::Kind::Custom)) {
            equal &= lhs.name == rhs.name;
        }

        return equal;
    }

    void dump() const;
    std::string to_str() const;
    Type withoutOptional() const {
        Type t = *this;
        t.isOptional = false;
        return t;
    }
    Type withoutRef() const {
        Type t = *this;
        t.isRef = false;
        return t;
    }
    Type withoutArray() const {
        Type t = *this;
        t.isArray = std::nullopt;
        return t;
    }
    Type pointer() const {
        Type t = *this;
        t.isPointer = t.isPointer.has_value() ? *t.isPointer + 1 : 1;
        return t;
    }
    Type remove_pointer() const {
        Type t = *this;
        if (t.isPointer) {
            if (*t.isPointer == 1) {
                t.isPointer = std::nullopt;
            } else {
                t.isPointer = *t.isPointer - 1;
            }
        }
        return t;
    }

    friend std::ostream& operator<<(std::ostream& os, const Type& t);
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

struct GenericTypeDecl : public Decl {
    GenericTypeDecl(SourceLocation location, std::string_view identifier) : Decl(location, identifier) {}

    void dump(size_t level = 0) const override;
};

struct GenericTypesDecl {
    std::vector<std::unique_ptr<GenericTypeDecl>> types;

    GenericTypesDecl(std::vector<std::unique_ptr<GenericTypeDecl>> types) : types(std::move(types)) {}

    void dump() const;
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

struct Block : public Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;

    Block(SourceLocation location, std::vector<std::unique_ptr<Stmt>> statements)
        : Stmt(location), statements(std::move(statements)) {}

    void dump(size_t level = 0) const override;
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

struct CaseStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Block> block;

    CaseStmt(SourceLocation location, std::unique_ptr<Expr> condition, std::unique_ptr<Block> block)
        : Stmt(location), condition(std::move(condition)), block(std::move(block)) {}

    void dump(size_t level = 0) const override;
};

struct SwitchStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<CaseStmt>> cases;
    std::unique_ptr<Block> elseBlock;

    SwitchStmt(SourceLocation location, std::unique_ptr<Expr> condition, std::vector<std::unique_ptr<CaseStmt>> cases,
               std::unique_ptr<Block> elseBlock)
        : Stmt(location), condition(std::move(condition)), cases(std::move(cases)), elseBlock(std::move(elseBlock)) {}

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
    Type structType;
    std::vector<std::unique_ptr<FieldInitStmt>> fieldInitializers;

    StructInstantiationExpr(SourceLocation location, Type structType,
                            std::vector<std::unique_ptr<FieldInitStmt>> fieldInitializers)
        : Expr(location), structType(std::move(structType)), fieldInitializers(std::move(fieldInitializers)) {}

    void dump(size_t level = 0) const override;
};

struct ArrayInstantiationExpr : public Expr {
    std::vector<std::unique_ptr<Expr>> initializers;

    ArrayInstantiationExpr(SourceLocation location, std::vector<std::unique_ptr<Expr>> initializers)
        : Expr(location), initializers(std::move(initializers)) {}

    void dump(size_t level = 0) const override;
};

struct IntLiteral : public Expr {
    std::string_view value;

    IntLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
};

struct FloatLiteral : public Expr {
    std::string_view value;

    FloatLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
};

struct CharLiteral : public Expr {
    std::string_view value;

    CharLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
};

struct BoolLiteral : public Expr {
    std::string_view value;

    BoolLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

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
    std::unique_ptr<GenericTypes> genericTypes;

    CallExpr(SourceLocation location, std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> arguments,
             std::unique_ptr<GenericTypes> genericTypes = nullptr)
        : Expr(location),
          callee(std::move(callee)),
          arguments(std::move(arguments)),
          genericTypes(std::move(genericTypes)) {}

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

struct ArrayAtExpr : public AssignableExpr {
    std::unique_ptr<Expr> array;
    std::unique_ptr<Expr> index;

    ArrayAtExpr(SourceLocation location, std::unique_ptr<Expr> array, std::unique_ptr<Expr> index)
        : AssignableExpr(location), array(std::move(array)), index(std::move(index)) {}

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

struct RefPtrExpr : public Expr {
    std::unique_ptr<Expr> expr;

    RefPtrExpr(SourceLocation location, std::unique_ptr<Expr> expr) : Expr(location), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
};

struct DerefPtrExpr : public AssignableExpr {
    std::unique_ptr<Expr> expr;

    DerefPtrExpr(SourceLocation location, std::unique_ptr<Expr> expr)
        : AssignableExpr(location), expr(std::move(expr)) {}

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
    std::unique_ptr<Type> type;
    std::unique_ptr<Expr> initializer;
    bool isMutable;

    VarDecl(SourceLocation location, std::string_view identifier, std::unique_ptr<Type> type, bool isMutable,
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
    std::unique_ptr<GenericTypesDecl> genericTypes;

    FunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                 std::vector<std::unique_ptr<ParamDecl>> params, std::unique_ptr<Block> body,
                 std::unique_ptr<GenericTypesDecl> genericTypes = nullptr)
        : FuncDecl(location, std::move(identifier), std::move(type), std::move(params)),
          body(std::move(body)),
          genericTypes(std::move(genericTypes)) {}

    void dump(size_t level = 0) const override;
};

// Forware declaration
struct StructDecl;
struct MemberFunctionDecl : public FuncDecl {
    StructDecl* structBase;
    std::unique_ptr<FunctionDecl> function;

    MemberFunctionDecl(StructDecl* structBase, std::unique_ptr<FunctionDecl> function)
        : FuncDecl(function->location, function->identifier, function->type, {}),
          structBase(structBase),
          function(std::move(function)) {}

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
    std::vector<std::unique_ptr<MemberFunctionDecl>> functions;
    std::unique_ptr<GenericTypesDecl> genericTypes;

    StructDecl(SourceLocation location, std::string_view identifier, std::vector<std::unique_ptr<FieldDecl>> fields,
               std::vector<std::unique_ptr<MemberFunctionDecl>> functions,
               std::unique_ptr<GenericTypesDecl> genericTypes = nullptr)
        : Decl(location, std::move(identifier)),
          fields(std::move(fields)),
          functions(std::move(functions)),
          genericTypes(std::move(genericTypes)) {}

    void dump(size_t level = 0) const override;
};

struct DeclStmt : public Decl, public Stmt {
    SourceLocation location;
    std::unique_ptr<VarDecl> varDecl;

    DeclStmt(SourceLocation location, std::unique_ptr<VarDecl> varDecl)
        : Decl(location, varDecl->identifier), Stmt(location), location(location), varDecl(std::move(varDecl)) {}

    void dump(size_t level = 0) const override;
};

struct Assignment : public Stmt {
    std::unique_ptr<AssignableExpr> assignee;
    std::unique_ptr<Expr> expr;

    Assignment(SourceLocation location, std::unique_ptr<AssignableExpr> assignee, std::unique_ptr<Expr> expr)
        : Stmt(location), assignee(std::move(assignee)), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
};

struct DeferStmt : public Stmt {
    std::unique_ptr<Block> block;

    DeferStmt(SourceLocation location, std::unique_ptr<Block> block) : Stmt(location), block(std::move(block)) {}

    void dump(size_t level = 0) const override;
};

struct ErrDecl : public Decl {
    ErrDecl(SourceLocation location, std::string_view identifier) : Decl(location, std::move(identifier)) {}

    void dump(size_t level = 0) const override;
};

struct ErrDeclRefExpr : public Expr {
    std::string_view identifier;
    ErrDeclRefExpr(SourceLocation location, std::string_view identifier)
        : Expr(location), identifier(std::move(identifier)) {}

    void dump(size_t level = 0) const override;
};

struct ErrGroupDecl : public Decl {
    std::vector<std::unique_ptr<ErrDecl>> errs;

    ErrGroupDecl(SourceLocation location, std::string_view identifier, std::vector<std::unique_ptr<ErrDecl>> errs)
        : Decl(location, std::move(identifier)), errs(std::move(errs)) {}

    void dump(size_t level = 0) const override;
};

struct ErrUnwrapExpr : public Expr {
    std::unique_ptr<Expr> errToUnwrap;

    ErrUnwrapExpr(SourceLocation location, std::unique_ptr<Expr> errToUnwrap)
        : Expr(location), errToUnwrap(std::move(errToUnwrap)) {}

    void dump(size_t level = 0) const override;
};

struct CatchErrExpr : public Expr {
    std::unique_ptr<Expr> errTocatch;
    std::unique_ptr<DeclStmt> declaration;

    CatchErrExpr(SourceLocation location, std::unique_ptr<Expr> errTocatch, std::unique_ptr<DeclStmt> declaration)
        : Expr(location), errTocatch(std::move(errTocatch)), declaration(std::move(declaration)) {}

    void dump(size_t level = 0) const override;
};

struct TryErrExpr : public Expr {
    std::unique_ptr<Expr> errTotry;
    std::unique_ptr<DeclStmt> declaration;

    TryErrExpr(SourceLocation location, std::unique_ptr<Expr> errTotry, std::unique_ptr<DeclStmt> declaration)
        : Expr(location), errTotry(std::move(errTotry)), declaration(std::move(declaration)) {}

    void dump(size_t level = 0) const override;
};

struct ModuleDecl : public Decl {
    std::vector<std::unique_ptr<Decl>> declarations;

    ModuleDecl(SourceLocation location, std::string_view identifier,
               std::vector<std::unique_ptr<Decl>> declarations = {})
        : Decl(location, identifier), declarations(std::move(declarations)) {}

    void dump(size_t level = 0) const override;
};

struct ImportExpr : public Expr {
    std::string_view identifier;
    ImportExpr(SourceLocation location, std::string_view identifier) : Expr(location), identifier(identifier) {}

    void dump(size_t level = 0) const override;
};
}  // namespace DMZ
