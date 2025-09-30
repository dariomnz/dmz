#pragma once

#include <string>

#include "DMZPCH.hpp"
#include "Debug.hpp"
#include "lexer/Lexer.hpp"

namespace DMZ {

// Forward declaration
struct Expr;

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
    bool isPublic;
    std::string identifier;

    Decl(SourceLocation location, bool isPublic, std::string_view identifier)
        : location(location), isPublic(isPublic), identifier(std::move(identifier)) {}
    virtual ~Decl() = default;

    virtual void dump(size_t level = 0) const = 0;
    virtual std::string to_str() const = 0;
};

struct GenericTypeDecl : public Decl {
    GenericTypeDecl(SourceLocation location, std::string_view identifier) : Decl(location, true, identifier) {}

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

struct TypeSlice : public Type {
    ptr<Expr> sliceType;
    TypeSlice(SourceLocation location, ptr<Expr> sliceType) : Type(location), sliceType(std::move(sliceType)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct TypeFunction : public Type {
    std::vector<ptr<Expr>> paramsTypes;
    ptr<Expr> returnType;
    TypeFunction(SourceLocation location, std::vector<ptr<Expr>> paramsTypes, ptr<Expr> returnType)
        : Type(location), paramsTypes(std::move(paramsTypes)), returnType(std::move(returnType)) {}

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

struct CaptureDecl : public Decl {
    CaptureDecl(SourceLocation location, std::string_view identifier) : Decl(location, true, identifier) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ForStmt : public Stmt {
    std::vector<ptr<Expr>> conditions;
    std::vector<ptr<CaptureDecl>> captures;
    ptr<Block> body;

    ForStmt(SourceLocation location, std::vector<ptr<Expr>> conditions, std::vector<ptr<CaptureDecl>> captures,
            ptr<Block> body)
        : Stmt(location), conditions(std::move(conditions)), captures(std::move(captures)), body(std::move(body)) {}

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

struct RangeExpr : public Expr {
    ptr<Expr> startExpr;
    ptr<Expr> endExpr;
    RangeExpr(SourceLocation location, ptr<Expr> startExpr, ptr<Expr> endExpr)
        : Expr(location), startExpr(std::move(startExpr)), endExpr(std::move(endExpr)) {}

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
        : Decl(location, true, std::move(identifier)),
          type(std::move(type)),
          isMutable(isMutable),
          isVararg(isVararg) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct VarDecl : public Decl {
    ptr<Expr> type;
    ptr<Expr> initializer;
    bool isMutable;

    VarDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<Expr> type, bool isMutable,
            ptr<Expr> initializer = nullptr)
        : Decl(location, isPublic, std::move(identifier)),
          type(std::move(type)),
          initializer(std::move(initializer)),
          isMutable(isMutable) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct FuncDecl : public Decl {
    ptr<Expr> type;
    std::vector<ptr<ParamDecl>> params;

    FuncDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<Expr> type,
             std::vector<ptr<ParamDecl>> params)
        : Decl(location, isPublic, std::move(identifier)), type(std::move(type)), params(std::move(params)) {}
};

struct ExternFunctionDecl : public FuncDecl {
    ExternFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<Expr> type,
                       std::vector<ptr<ParamDecl>> params)
        : FuncDecl(location, isPublic, std::move(identifier), std::move(type), std::move(params)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct FunctionDecl : public FuncDecl {
    ptr<Block> body;

    FunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<Expr> type,
                 std::vector<ptr<ParamDecl>> params, ptr<Block> body)
        : FuncDecl(location, isPublic, std::move(identifier), std::move(type), std::move(params)),
          body(std::move(body)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct GenericFunctionDecl : public FunctionDecl {
    std::vector<ptr<GenericTypeDecl>> genericTypes;

    GenericFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<Expr> type,
                        std::vector<ptr<ParamDecl>> params, ptr<Block> body,
                        std::vector<ptr<GenericTypeDecl>> genericTypes)
        : FunctionDecl(location, isPublic, std::move(identifier), std::move(type), std::move(params), std::move(body)),
          genericTypes(std::move(genericTypes)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

// Forware declaration
struct StructDecl;
struct MemberFunctionDecl : public FunctionDecl {
    StructDecl* structBase;
    bool isStatic;

    MemberFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<Expr> type,
                       std::vector<ptr<ParamDecl>> params, ptr<Block> body, StructDecl* structBase, bool isStatic)
        : FunctionDecl(location, isPublic, std::move(identifier), std::move(type), std::move(params), std::move(body)),
          structBase(structBase),
          isStatic(isStatic) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct MemberGenericFunctionDecl : public GenericFunctionDecl {
    StructDecl* structBase;

    MemberGenericFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<Expr> type,
                              std::vector<ptr<ParamDecl>> params, ptr<Block> body,
                              std::vector<ptr<GenericTypeDecl>> genericTypes, StructDecl* structBase)
        : GenericFunctionDecl(location, isPublic, std::move(identifier), std::move(type), std::move(params),
                              std::move(body), std::move(genericTypes)),
          structBase(structBase) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct FieldDecl : public Decl {
    ptr<Expr> type;

    FieldDecl(SourceLocation location, std::string_view identifier, ptr<Expr> type)
        : Decl(location, true, std::move(identifier)), type(std::move(type)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct StructDecl : public Decl {
    bool isPacked;
    std::vector<ptr<FieldDecl>> fields;
    std::vector<ptr<MemberFunctionDecl>> functions;

    StructDecl(SourceLocation location, bool isPublic, std::string_view identifier, bool isPacked,
               std::vector<ptr<FieldDecl>> fields, std::vector<ptr<MemberFunctionDecl>> functions)
        : Decl(location, isPublic, std::move(identifier)),
          isPacked(isPacked),
          fields(std::move(fields)),
          functions(std::move(functions)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct GenericStructDecl : public StructDecl {
    std::vector<ptr<GenericTypeDecl>> genericTypes;

    GenericStructDecl(SourceLocation location, bool isPublic, std::string_view identifier, bool isPacked,
                      std::vector<ptr<FieldDecl>> fields, std::vector<ptr<MemberFunctionDecl>> functions,
                      std::vector<ptr<GenericTypeDecl>> genericTypes)
        : StructDecl(location, isPublic, std::move(identifier), isPacked, std::move(fields), std::move(functions)),
          genericTypes(std::move(genericTypes)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct DeclStmt : public Decl, public Stmt {
    SourceLocation location;
    ptr<VarDecl> varDecl;

    DeclStmt(SourceLocation location, ptr<VarDecl> varDecl)
        : Decl(location, varDecl->isPublic, varDecl->identifier),
          Stmt(location),
          location(location),
          varDecl(std::move(varDecl)) {}

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

struct AssignmentOperator : public Assignment {
    TokenType op;

    AssignmentOperator(SourceLocation location, ptr<AssignableExpr> assignee, ptr<Expr> expr, TokenType op)
        : Assignment(location, std::move(assignee), std::move(expr)), op(op) {}

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
    ErrorDecl(SourceLocation location, std::string_view identifier) : Decl(location, true, std::move(identifier)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ErrorInPlaceExpr : public Expr {
    std::string identifier;
    ErrorInPlaceExpr(SourceLocation location, std::string_view identifier) : Expr(location), identifier(identifier) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ErrorGroupExprDecl : public Expr, public Decl {
    SourceLocation location;
    std::vector<ptr<ErrorDecl>> errs;

    ErrorGroupExprDecl(SourceLocation location, std::vector<ptr<ErrorDecl>> errs)
        : Expr(location), Decl(location, true, ""), errs(std::move(errs)) {}

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
    std::filesystem::path module_path;
    std::vector<ptr<Decl>> declarations;

    ModuleDecl(SourceLocation location, std::string_view identifier, std::filesystem::path module_path,
               std::vector<ptr<Decl>> declarations)
        : Decl(location, true, identifier),
          module_path(std::move(module_path)),
          declarations(std::move(declarations)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ImportExpr : public Expr {
    std::string identifier;
    std::string module_id;
    std::filesystem::path module_path;
    ImportExpr(SourceLocation location, std::string_view identifier, std::string module_id,
               std::filesystem::path module_path)
        : Expr(location),
          identifier(identifier),
          module_id(std::move(module_id)),
          module_path(std::move(module_path)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct TestDecl : public FunctionDecl {
    TestDecl(SourceLocation location, std::string_view identifier, ptr<Block> body)
        : FunctionDecl(location, true, identifier,
                       makePtr<UnaryOperator>(location, makePtr<TypeVoid>(location), TokenType::op_excla_mark), {},
                       std::move(body)) {}

    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};
}  // namespace DMZ
