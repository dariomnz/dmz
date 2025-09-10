#pragma once

#include "DMZPCH.hpp"
#include "Debug.hpp"
#include "lexer/Lexer.hpp"

namespace DMZ {

[[maybe_unused]] static inline std::string get_op_str(TokenType op) {
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
struct Expr;

// struct GenericTypes {
//     std::vector<ptr<Expr>> types;

//     GenericTypes(std::vector<ptr<Expr>> types) noexcept;

//     GenericTypes(const GenericTypes& other);
//     GenericTypes& operator=(const GenericTypes& other);
//     GenericTypes(GenericTypes&& other) noexcept;

//     void dump() const;
//     std::string to_str() const;
// };

// Forward declaration
// struct ResolvedDecl;
// struct Type {
//     enum class Kind { Void, Bool, Int, UInt, Float, Struct, Custom, Generic, Error, Module, ErrorGroup };

//     static std::string KindString(Kind k) {
//         switch (k) {
//             case Kind::Void:
//                 return "Void";
//             case Kind::Bool:
//                 return "Bool";
//             case Kind::Int:
//                 return "Int";
//             case Kind::UInt:
//                 return "UInt";
//             case Kind::Float:
//                 return "Float";
//             case Kind::Struct:
//                 return "Struct";
//             case Kind::Custom:
//                 return "Custom";
//             case Kind::Generic:
//                 return "Generic";
//             case Kind::Error:
//                 return "Error";
//             case Kind::Module:
//                 return "Module";
//             case Kind::ErrorGroup:
//                 return "ErrorGroup";
//         }
//         dmz_unreachable("unexpected kind " + std::to_string(static_cast<int>(k)));
//     }

//     Kind kind = Kind::Void;
//     std::string name = "";
//     int size = 0;
//     std::optional<int> isArray = std::nullopt;
//     std::optional<int> isPointer = std::nullopt;
//     bool isOptional = false;
//     std::optional<GenericTypes> genericTypes = std::nullopt;
//     ResolvedDecl* decl = nullptr;  // For user defined types
//     SourceLocation location = {};

//     static Type builtinVoid() { return {Kind::Void, "void"}; }
//     static Type builtinBool() { return {Kind::Bool}; }
//     static Type builtinIN(const std::string_view& name) {
//         auto num = name.substr(1);
//         int result = 0;
//         auto res = std::from_chars(num.data(), num.data() + num.size(), result);
//         if (result == 0 || res.ec != std::errc()) {
//             dmz_unreachable("unexpected size of 0 in i type");
//         }
//         return {Kind::Int, std::string(name), result};
//     }
//     static Type builtinUN(const std::string_view& name) {
//         auto num = name.substr(1);
//         int result = 0;
//         auto res = std::from_chars(num.data(), num.data() + num.size(), result);
//         if (result == 0 || res.ec != std::errc()) {
//             dmz_unreachable("unexpected size of 0 in u type");
//         }
//         return {Kind::UInt, std::string(name), result};
//     }
//     static Type builtinF16() { return {Kind::Float, "f16", 16}; }
//     static Type builtinF32() { return {Kind::Float, "f32", 32}; }
//     static Type builtinF64() { return {Kind::Float, "f64", 64}; }
//     static Type builtinString(int size) { return Type{.kind = Kind::UInt, .name = "u8", .size = 8, .isPointer = 1}; }
//     static Type moduleType(const std::string_view& name, ResolvedDecl* decl) {
//         return Type{.kind = Kind::Module, .name = std::string(name), .decl = decl};
//     }
//     static Type errorGroupType(ResolvedDecl* decl) { return Type{.kind = Kind::ErrorGroup, .decl = decl}; }
//     static Type customType(const std::string_view& name) { return {Kind::Custom, std::string(name)}; }
//     static Type structType(const std::string_view& name, ResolvedDecl* decl) {
//         return Type{.kind = Kind::Struct, .name = std::string(name), .decl = decl};
//     }
//     static Type structType(Type t, ResolvedDecl* decl) {
//         if (t.kind != Kind::Custom) dmz_unreachable("expected custom type to convert to struct");
//         t.kind = Kind::Struct;
//         t.decl = decl;
//         return t;
//     }
//     static Type genericType(Type t) {
//         if (t.kind != Kind::Custom) dmz_unreachable("expected custom type to convert to generic");
//         t.kind = Kind::Generic;
//         return t;
//     }
//     static Type specializeType(Type genType, Type t) {
//         if (genType.kind == Kind::Generic) dmz_unreachable("expected generic type to convert to specialized");
//         if (t.kind == Kind::Custom) {
//             dmz_unreachable("expected resolved type to convert to specialized");
//         }

//         genType.name = t.name;
//         genType.kind = t.kind;
//         genType.decl = t.decl;
//         genType.size = t.size;
//         return genType;
//     }
//     static Type builtinError(const std::string_view& name) { return {Kind::Error, std::string(name)}; }

//     bool operator==(const Type& otro) const {
//         return (kind == otro.kind && name == otro.name && isArray == otro.isArray && isOptional == otro.isOptional &&
//                 isPointer == otro.isPointer);
//     }

//     static bool can_convert(const Type& to, const Type& from);
//     static bool compare(const Type& lhs, const Type& rhs);

//     void dump() const;
//     std::string to_str(bool removeKind = true) const;
//     Type withoutOptional() const {
//         Type t = *this;
//         t.isOptional = false;
//         return t;
//     }
//     Type withoutArray() const {
//         Type t = *this;
//         t.isArray = std::nullopt;
//         return t;
//     }
//     Type pointer() const {
//         Type t = *this;
//         t.isPointer = t.isPointer.has_value() ? *t.isPointer + 1 : 1;
//         return t;
//     }
//     Type remove_pointer() const {
//         Type t = *this;
//         if (t.isPointer) {
//             if (*t.isPointer == 1) {
//                 t.isPointer = std::nullopt;
//             } else {
//                 t.isPointer = *t.isPointer - 1;
//             }
//         }
//         return t;
//     }

//     friend std::ostream& operator<<(std::ostream& os, const Type& t);
// };

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
    std::string identifier;

    Decl(SourceLocation location, std::string_view identifier)
        : location(location), identifier(std::move(identifier)) {}
    virtual ~Decl() = default;

    virtual void dump(size_t level = 0) const = 0;
    virtual std::string to_str() const = 0;
};

struct GenericTypeDecl : public Decl {
    GenericTypeDecl(SourceLocation location, std::string_view identifier) : Decl(location, identifier) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct Stmt {
    SourceLocation location;
    Stmt(SourceLocation location) : location(location) {}

    virtual ~Stmt() = default;

    virtual void dump(size_t level = 0) const = 0;
    virtual std::string to_str() const = 0;
};

struct Expr : public Stmt {
    Expr(SourceLocation location) : Stmt(location) {}
};

struct Type : public Expr {
    Type(SourceLocation location) : Expr(location) {}
};

struct TypeVoid : public Type {
    TypeVoid(SourceLocation location) : Type(location) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct TypeNumber : public Type {
    std::string name;
    TypeNumber(SourceLocation location, std::string_view name) : Type(location), name(name) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct TypeBool : public Type {
    TypeBool(SourceLocation location) : Type(location) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct Block : public Stmt {
    std::vector<ptr<Stmt>> statements;

    Block(SourceLocation location, std::vector<ptr<Stmt>> statements)
        : Stmt(location), statements(std::move(statements)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct IfStmt : public Stmt {
    ptr<Expr> condition;
    ptr<Block> trueBlock;
    ptr<Block> falseBlock;

    IfStmt(SourceLocation location, ptr<Expr> condition, ptr<Block> trueBlock, ptr<Block> falseBlock = nullptr)
        : Stmt(location),
          condition(std::move(condition)),
          trueBlock(std::move(trueBlock)),
          falseBlock(std::move(falseBlock)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct WhileStmt : public Stmt {
    ptr<Expr> condition;
    ptr<Block> body;

    WhileStmt(SourceLocation location, ptr<Expr> condition, ptr<Block> body)
        : Stmt(location), condition(std::move(condition)), body(std::move(body)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct CaseStmt : public Stmt {
    ptr<Expr> condition;
    ptr<Block> block;

    CaseStmt(SourceLocation location, ptr<Expr> condition, ptr<Block> block)
        : Stmt(location), condition(std::move(condition)), block(std::move(block)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct SwitchStmt : public Stmt {
    ptr<Expr> condition;
    std::vector<ptr<CaseStmt>> cases;
    ptr<Block> elseBlock;

    SwitchStmt(SourceLocation location, ptr<Expr> condition, std::vector<ptr<CaseStmt>> cases, ptr<Block> elseBlock)
        : Stmt(location), condition(std::move(condition)), cases(std::move(cases)), elseBlock(std::move(elseBlock)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ReturnStmt : public Stmt {
    ptr<Expr> expr;

    ReturnStmt(SourceLocation location, ptr<Expr> expr = nullptr) : Stmt(location), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct FieldInitStmt : public Stmt {
    std::string identifier;
    ptr<Expr> initializer;

    FieldInitStmt(SourceLocation location, std::string_view identifier, ptr<Expr> initializer)
        : Stmt(location), identifier(identifier), initializer(std::move(initializer)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct StructInstantiationExpr : public Expr {
    ptr<Expr> base;
    std::vector<ptr<FieldInitStmt>> fieldInitializers;

    StructInstantiationExpr(SourceLocation location, ptr<Expr> base, std::vector<ptr<FieldInitStmt>> fieldInitializers)
        : Expr(location), base(std::move(base)), fieldInitializers(std::move(fieldInitializers)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ArrayInstantiationExpr : public Expr {
    std::vector<ptr<Expr>> initializers;

    ArrayInstantiationExpr(SourceLocation location, std::vector<ptr<Expr>> initializers)
        : Expr(location), initializers(std::move(initializers)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct IntLiteral : public Expr {
    std::string value;

    IntLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct FloatLiteral : public Expr {
    std::string value;

    FloatLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct CharLiteral : public Expr {
    std::string value;

    CharLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct BoolLiteral : public Expr {
    std::string value;

    BoolLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct StringLiteral : public Expr {
    std::string value;

    StringLiteral(SourceLocation location, std::string_view value) : Expr(location), value(value) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct NullLiteral : public Expr {
    NullLiteral(SourceLocation location) : Expr(location) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct SizeofExpr : public Expr {
    ptr<Expr> sizeofType;
    SizeofExpr(SourceLocation location, ptr<Expr> sizeofType) : Expr(location), sizeofType(std::move(sizeofType)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct CallExpr : public Expr {
    ptr<Expr> callee;
    std::vector<ptr<Expr>> arguments;

    CallExpr(SourceLocation location, ptr<Expr> callee, std::vector<ptr<Expr>> arguments)
        : Expr(location), callee(std::move(callee)), arguments(std::move(arguments)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct AssignableExpr : public Expr {
    AssignableExpr(SourceLocation location) : Expr(location) {}
};

struct DeclRefExpr : public AssignableExpr {
    std::string identifier;

    DeclRefExpr(SourceLocation location, std::string_view identifier)
        : AssignableExpr(location), identifier(identifier) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct MemberExpr : public AssignableExpr {
    ptr<Expr> base;
    std::string field;

    MemberExpr(SourceLocation location, ptr<Expr> base, std::string_view field)
        : AssignableExpr(location), base(std::move(base)), field(std::move(field)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct GenericExpr : public Expr {
    ptr<Expr> base;
    std::vector<ptr<Expr>> types;

    GenericExpr(SourceLocation location, ptr<Expr> base, std::vector<ptr<Expr>> types)
        : Expr(location), base(std::move(base)), types(std::move(types)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct SelfMemberExpr : public AssignableExpr {
    std::string field;

    SelfMemberExpr(SourceLocation location, std::string_view field)
        : AssignableExpr(location), field(std::move(field)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ArrayAtExpr : public AssignableExpr {
    ptr<Expr> array;
    ptr<Expr> index;

    ArrayAtExpr(SourceLocation location, ptr<Expr> array, ptr<Expr> index)
        : AssignableExpr(location), array(std::move(array)), index(std::move(index)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct GroupingExpr : public Expr {
    ptr<Expr> expr;

    GroupingExpr(SourceLocation location, ptr<Expr> expr) : Expr(location), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct BinaryOperator : public Expr {
    ptr<Expr> lhs;
    ptr<Expr> rhs;
    TokenType op;

    BinaryOperator(SourceLocation location, ptr<Expr> lhs, ptr<Expr> rhs, TokenType op)
        : Expr(location), lhs(std::move(lhs)), rhs(std::move(rhs)), op(op) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct UnaryOperator : public Expr {
    ptr<Expr> operand;
    TokenType op;

    UnaryOperator(SourceLocation location, ptr<Expr> operand, TokenType op)
        : Expr(location), operand(std::move(operand)), op(op) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct RefPtrExpr : public Expr {
    ptr<Expr> expr;

    RefPtrExpr(SourceLocation location, ptr<Expr> expr) : Expr(location), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct DerefPtrExpr : public AssignableExpr {
    ptr<Expr> expr;

    DerefPtrExpr(SourceLocation location, ptr<Expr> expr) : AssignableExpr(location), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ParamDecl : public Decl {
    ptr<Expr> type;
    bool isMutable;
    bool isVararg = false;

    ParamDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type, bool isMutable,
              bool isVararg = false)
        : Decl(location, std::move(identifier)), type(std::move(type)), isMutable(isMutable), isVararg(isVararg) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct VarDecl : public Decl {
    ptr<Expr> type;
    ptr<Expr> initializer;
    bool isMutable;

    VarDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type, bool isMutable,
            ptr<Expr> initializer = nullptr)
        : Decl(location, std::move(identifier)),
          type(std::move(type)),
          initializer(std::move(initializer)),
          isMutable(isMutable) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct FuncDecl : public Decl {
    ptr<Expr> type;
    std::vector<ptr<ParamDecl>> params;

    FuncDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type, std::vector<ptr<ParamDecl>> params)
        : Decl(location, std::move(identifier)), type(std::move(type)), params(std::move(params)) {}
};

struct ExternFunctionDecl : public FuncDecl {
    ExternFunctionDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type,
                       std::vector<ptr<ParamDecl>> params)
        : FuncDecl(location, std::move(identifier), std::move(type), std::move(params)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct FunctionDecl : public FuncDecl {
    ptr<Block> body;

    FunctionDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type,
                 std::vector<ptr<ParamDecl>> params, ptr<Block> body)
        : FuncDecl(location, std::move(identifier), std::move(type), std::move(params)), body(std::move(body)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct GenericFunctionDecl : public FunctionDecl {
    std::vector<ptr<GenericTypeDecl>> genericTypes;

    GenericFunctionDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type,
                        std::vector<ptr<ParamDecl>> params, ptr<Block> body,
                        std::vector<ptr<GenericTypeDecl>> genericTypes)
        : FunctionDecl(location, std::move(identifier), std::move(type), std::move(params), std::move(body)),
          genericTypes(std::move(genericTypes)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

// Forware declaration
struct StructDecl;
struct MemberFunctionDecl : public FunctionDecl {
    StructDecl* structBase;
    bool isStatic;

    MemberFunctionDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type,
                       std::vector<ptr<ParamDecl>> params, ptr<Block> body, StructDecl* structBase, bool isStatic)
        : FunctionDecl(location, std::move(identifier), std::move(type), std::move(params), std::move(body)),
          structBase(structBase),
          isStatic(isStatic) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct MemberGenericFunctionDecl : public GenericFunctionDecl {
    StructDecl* structBase;

    MemberGenericFunctionDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type,
                              std::vector<ptr<ParamDecl>> params, ptr<Block> body,
                              std::vector<ptr<GenericTypeDecl>> genericTypes, StructDecl* structBase)
        : GenericFunctionDecl(location, std::move(identifier), std::move(type), std::move(params), std::move(body),
                              std::move(genericTypes)),
          structBase(structBase) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct FieldDecl : public Decl {
    ptr<Expr> type;

    FieldDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type)
        : Decl(location, std::move(identifier)), type(std::move(type)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct StructDecl : public Decl {
    bool isPacked;
    std::vector<ptr<FieldDecl>> fields;
    std::vector<ptr<MemberFunctionDecl>> functions;

    StructDecl(SourceLocation location, std::string_view identifier, bool isPacked, std::vector<ptr<FieldDecl>> fields,
               std::vector<ptr<MemberFunctionDecl>> functions)
        : Decl(location, std::move(identifier)),
          isPacked(isPacked),
          fields(std::move(fields)),
          functions(std::move(functions)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct GenericStructDecl : public StructDecl {
    std::vector<ptr<GenericTypeDecl>> genericTypes;

    GenericStructDecl(SourceLocation location, std::string_view identifier, bool isPacked,
                      std::vector<ptr<FieldDecl>> fields, std::vector<ptr<MemberFunctionDecl>> functions,
                      std::vector<ptr<GenericTypeDecl>> genericTypes)
        : StructDecl(location, std::move(identifier), isPacked, std::move(fields), std::move(functions)),
          genericTypes(std::move(genericTypes)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct DeclStmt : public Decl, public Stmt {
    SourceLocation location;
    ptr<VarDecl> varDecl;

    DeclStmt(SourceLocation location, ptr<VarDecl> varDecl)
        : Decl(location, varDecl->identifier), Stmt(location), location(location), varDecl(std::move(varDecl)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct Assignment : public Stmt {
    ptr<AssignableExpr> assignee;
    ptr<Expr> expr;

    Assignment(SourceLocation location, ptr<AssignableExpr> assignee, ptr<Expr> expr)
        : Stmt(location), assignee(std::move(assignee)), expr(std::move(expr)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct DeferStmt : public Stmt {
    ptr<Block> block;
    bool isErrDefer = false;

    DeferStmt(SourceLocation location, ptr<Block> block, bool isErrDefer)
        : Stmt(location), block(std::move(block)), isErrDefer(isErrDefer) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ErrorDecl : public Decl {
    ErrorDecl(SourceLocation location, std::string_view identifier) : Decl(location, std::move(identifier)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ErrorGroupExprDecl : public Expr, public Decl {
    SourceLocation location;
    std::vector<ptr<ErrorDecl>> errs;

    ErrorGroupExprDecl(SourceLocation location, std::vector<ptr<ErrorDecl>> errs)
        : Expr(location), Decl(location, ""), errs(std::move(errs)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct CatchErrorExpr : public Expr {
    ptr<Expr> errorToCatch;

    CatchErrorExpr(SourceLocation location, ptr<Expr> errorToCatch)
        : Expr(location), errorToCatch(std::move(errorToCatch)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct TryErrorExpr : public Expr {
    ptr<Expr> errorToTry;

    TryErrorExpr(SourceLocation location, ptr<Expr> errorToTry) : Expr(location), errorToTry(std::move(errorToTry)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct OrElseErrorExpr : public Expr {
    ptr<Expr> errorToOrElse;
    ptr<Expr> orElseExpr;

    OrElseErrorExpr(SourceLocation location, ptr<Expr> errorToOrElse, ptr<Expr> orElseExpr)
        : Expr(location), errorToOrElse(std::move(errorToOrElse)), orElseExpr(std::move(orElseExpr)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ModuleDecl : public Decl {
    std::vector<ptr<Decl>> declarations;

    ModuleDecl(SourceLocation location, std::string_view identifier, std::vector<ptr<Decl>> declarations = {})
        : Decl(location, identifier), declarations(std::move(declarations)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ImportExpr : public Expr {
    std::string identifier;
    ImportExpr(SourceLocation location, std::string_view identifier) : Expr(location), identifier(identifier) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct TestDecl : public FunctionDecl {
    TestDecl(SourceLocation location, std::string_view identifier, ptr<Block> body)
        : FunctionDecl(location, identifier,
                       makePtr<UnaryOperator>(location, makePtr<TypeVoid>(location), TokenType::op_excla_mark), {},
                       std::move(body)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};
}  // namespace DMZ
