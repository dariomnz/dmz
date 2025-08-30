#pragma once

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "lexer/Lexer.hpp"

namespace DMZ {

// struct ModuleID {
//     std::vector<std::string_view> modules;

//     ModuleID(std::vector<std::string_view> modules) : modules(std::move(modules)) {}
//     ModuleID() : modules({}) {}
//     void dump() const;
//     std::string to_string() const;
//     bool empty() const { return modules.empty(); };
//     friend std::ostream &operator<<(std::ostream &os, const ModuleID &t);
// };

struct ResolvedStmt {
    SourceLocation location;

    ResolvedStmt(SourceLocation location) : location(location) {}

    virtual ~ResolvedStmt() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
};

struct ResolvedExpr : public ConstantValueContainer<int>, public ResolvedStmt {
    Type type;

    ResolvedExpr(SourceLocation location, Type type) : ResolvedStmt(location), type(type) {}

    virtual ~ResolvedExpr() = default;

    void dump_constant_value(size_t level) const;
};

struct ResolvedDecl {
    SourceLocation location;
    std::string identifier;
    std::string symbolName;
    Type type;
    bool isMutable;

    ResolvedDecl(SourceLocation location, std::string_view identifier, Type type, bool isMutable)
        : location(location), identifier(std::move(identifier)), type(type), isMutable(isMutable) {}
    virtual ~ResolvedDecl() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
    virtual void dump_dependencies(size_t level = 0) const {};
};

struct ResolvedDependencies : public ResolvedDecl {
    std::unordered_set<ResolvedDependencies *> dependsOn;
    std::unordered_set<ResolvedDependencies *> isUsedBy;

    ResolvedDependencies(SourceLocation location, std::string_view identifier, Type type, bool isMutable)
        : ResolvedDecl(location, identifier, type, isMutable) {}
    virtual ~ResolvedDependencies() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
    void dump_dependencies(size_t level = 0) const override;
};

enum struct ResolvedDeclType {
    Module,
    ResolvedDecl,
    ResolvedStructDecl,
    ResolvedErrorDecl,
    ResolvedImportExpr,
    ResolvedModuleDecl,
    ResolvedMemberFunctionDecl,
    ResolvedFieldDecl,
    ResolvedGenericTypeDecl,
};

std::ostream &operator<<(std::ostream &os, const ResolvedDeclType &type);

struct ResolvedGenericTypeDecl : public ResolvedDecl {
    std::optional<Type> specializedType;

    ResolvedGenericTypeDecl(SourceLocation location, std::string_view identifier)
        : ResolvedDecl(location, identifier, Type::customType(identifier), false) {}

    void dump(size_t level = 0, bool onlySelf = false) const;
};

// Forward declaration
struct ResolvedDeferStmt;

struct ResolvedDeferRefStmt : public ResolvedStmt {
    ResolvedDeferStmt &resolvedDefer;

    ResolvedDeferRefStmt(SourceLocation location, ResolvedDeferStmt &resolvedDefer)
        : ResolvedStmt(location), resolvedDefer(resolvedDefer) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedBlock : public ResolvedStmt {
    std::vector<std::unique_ptr<ResolvedStmt>> statements;
    std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers;

    ResolvedBlock(SourceLocation location, std::vector<std::unique_ptr<ResolvedStmt>> statements,
                  std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedStmt(location), statements(std::move(statements)), defers(std::move(defers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedDeferStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedBlock> block;
    bool isErrDefer;

    ResolvedDeferStmt(SourceLocation location, std::unique_ptr<ResolvedBlock> block, bool isErrDefer)
        : ResolvedStmt(location), block(std::move(block)), isErrDefer(isErrDefer) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
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

struct ResolvedCaseStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedExpr> condition;
    std::unique_ptr<ResolvedBlock> block;

    ResolvedCaseStmt(SourceLocation location, std::unique_ptr<ResolvedExpr> condition,
                     std::unique_ptr<ResolvedBlock> block)
        : ResolvedStmt(location), condition(std::move(condition)), block(std::move(block)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSwitchStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedExpr> condition;
    std::vector<std::unique_ptr<ResolvedCaseStmt>> cases;
    std::unique_ptr<ResolvedBlock> elseBlock;

    ResolvedSwitchStmt(SourceLocation location, std::unique_ptr<ResolvedExpr> condition,
                       std::vector<std::unique_ptr<ResolvedCaseStmt>> cases, std::unique_ptr<ResolvedBlock> elseBlock)
        : ResolvedStmt(location),
          condition(std::move(condition)),
          cases(std::move(cases)),
          elseBlock(std::move(elseBlock)) {}

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

struct ResolvedFuncDecl : public ResolvedDependencies {
    std::vector<std::unique_ptr<ResolvedParamDecl>> params;

    ResolvedFuncDecl(SourceLocation location, std::string_view identifier, Type type,
                     std::vector<std::unique_ptr<ResolvedParamDecl>> params)
        : ResolvedDependencies(location, std::move(identifier), type, false), params(std::move(params)) {}
};

struct ResolvedExternFunctionDecl : public ResolvedFuncDecl {
    ResolvedExternFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                               std::vector<std::unique_ptr<ResolvedParamDecl>> params)
        : ResolvedFuncDecl(location, std::move(identifier), type, std::move(params)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFunctionDecl : public ResolvedFuncDecl {
    const FunctionDecl *functionDecl;
    std::unique_ptr<ResolvedBlock> body;

    ResolvedFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                         std::vector<std::unique_ptr<ResolvedParamDecl>> params, const FunctionDecl *functionDecl,
                         std::unique_ptr<ResolvedBlock> body)
        : ResolvedFuncDecl(location, std::move(identifier), type, std::move(params)),
          functionDecl(functionDecl),
          body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSpecializedFunctionDecl : public ResolvedFunctionDecl {
    GenericTypes genericTypes;  // The types used for specialization
    ResolvedSpecializedFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                                    std::vector<std::unique_ptr<ResolvedParamDecl>> params,
                                    const FunctionDecl *functionDecl, std::unique_ptr<ResolvedBlock> body,
                                    GenericTypes genericTypes)
        : ResolvedFunctionDecl(location, identifier, type, std::move(params), functionDecl, std::move(body)),
          genericTypes(std::move(genericTypes)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedGenericFunctionDecl : public ResolvedFunctionDecl {
    std::vector<std::unique_ptr<ResolvedGenericTypeDecl>> genericTypeDecls = {};         // The types used for lookup
    std::vector<std::unique_ptr<ResolvedSpecializedFunctionDecl>> specializations = {};  // List of specializations
    std::vector<ResolvedDecl *> scopeToSpecialize;                                       // Scope use to specialize

    ResolvedGenericFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                                std::vector<std::unique_ptr<ResolvedParamDecl>> params,
                                const FunctionDecl *functionDecl, std::unique_ptr<ResolvedBlock> body,
                                std::vector<std::unique_ptr<ResolvedGenericTypeDecl>> genericTypeDecls,
                                std::vector<ResolvedDecl *> scopeToSpecialize)
        : ResolvedFunctionDecl(location, identifier, type, std::move(params), functionDecl, std::move(body)),
          genericTypeDecls(std::move(genericTypeDecls)),
          scopeToSpecialize(std::move(scopeToSpecialize)) {}
    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

// Forward declaration
struct ResolvedStructDecl;
struct ResolvedMemberFunctionDecl : public ResolvedFunctionDecl {
    const ResolvedStructDecl *structDecl;
    bool isStatic;

    ResolvedMemberFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                               std::vector<std::unique_ptr<ResolvedParamDecl>> params, const FunctionDecl *functionDecl,
                               std::unique_ptr<ResolvedBlock> body, const ResolvedStructDecl *structDecl, bool isStatic)
        : ResolvedFunctionDecl(location, identifier, type, std::move(params), functionDecl, std::move(body)),
          structDecl(structDecl),
          isStatic(isStatic) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedMemberGenericFunctionDecl : public ResolvedGenericFunctionDecl {
    const ResolvedStructDecl *structDecl;
    ResolvedMemberGenericFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                                      std::vector<std::unique_ptr<ResolvedParamDecl>> params,
                                      const FunctionDecl *functionDecl, std::unique_ptr<ResolvedBlock> body,
                                      std::vector<std::unique_ptr<ResolvedGenericTypeDecl>> genericTypeDecls,
                                      std::vector<ResolvedDecl *> scopeToSpecialize,
                                      const ResolvedStructDecl *structDecl)
        : ResolvedGenericFunctionDecl(location, identifier, type, std::move(params), functionDecl, std::move(body),
                                      std::move(genericTypeDecls), std::move(scopeToSpecialize)),
          structDecl(structDecl) {}
    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

struct ResolvedMemberSpecializedFunctionDecl : public ResolvedSpecializedFunctionDecl {
    const ResolvedStructDecl *structDecl;
    ResolvedMemberSpecializedFunctionDecl(SourceLocation location, std::string_view identifier, Type type,
                                          std::vector<std::unique_ptr<ResolvedParamDecl>> params,
                                          const FunctionDecl *functionDecl, std::unique_ptr<ResolvedBlock> body,
                                          GenericTypes genericTypes, const ResolvedStructDecl *structDecl)
        : ResolvedSpecializedFunctionDecl(location, identifier, type, std::move(params), functionDecl, std::move(body),
                                          std::move(genericTypes)),
          structDecl(structDecl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedStructDecl : public ResolvedDependencies {
    const StructDecl *structDecl;
    bool isPacked;
    std::vector<std::unique_ptr<ResolvedFieldDecl>> fields;
    std::vector<std::unique_ptr<ResolvedMemberFunctionDecl>> functions;

    ResolvedStructDecl(SourceLocation location, std::string_view identifier, const StructDecl *structDecl,
                       bool isPacked, std::vector<std::unique_ptr<ResolvedFieldDecl>> fields,
                       std::vector<std::unique_ptr<ResolvedMemberFunctionDecl>> functions)
        : ResolvedDependencies(location, std::move(identifier), Type::structType(identifier, this), false),
          structDecl(structDecl),
          isPacked(isPacked),
          fields(std::move(fields)),
          functions(std::move(functions)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

struct ResolvedSpecializedStructDecl : public ResolvedStructDecl {
    GenericTypes genericTypes;  // The types used for specialization
    ResolvedSpecializedStructDecl(SourceLocation location, std::string_view identifier, const StructDecl *structDecl,
                                  bool isPacked, std::vector<std::unique_ptr<ResolvedFieldDecl>> fields,
                                  std::vector<std::unique_ptr<ResolvedMemberFunctionDecl>> functions,
                                  GenericTypes genericTypes)
        : ResolvedStructDecl(location, identifier, structDecl, isPacked, std::move(fields), std::move(functions)),
          genericTypes(std::move(genericTypes)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedGenericStructDecl : public ResolvedStructDecl {
    std::vector<std::unique_ptr<ResolvedGenericTypeDecl>> genericTypeDecls = {};       // The types used for lookup
    std::vector<std::unique_ptr<ResolvedSpecializedStructDecl>> specializations = {};  // List of specializations
    std::vector<ResolvedDecl *> scopeToSpecialize;                                     // Scope use to specialize

    ResolvedGenericStructDecl(SourceLocation location, std::string_view identifier, const StructDecl *structDecl,
                              bool isPacked, std::vector<std::unique_ptr<ResolvedFieldDecl>> fields,
                              std::vector<std::unique_ptr<ResolvedMemberFunctionDecl>> functions,
                              std::vector<std::unique_ptr<ResolvedGenericTypeDecl>> genericTypeDecls,
                              std::vector<ResolvedDecl *> scopeToSpecialize)
        : ResolvedStructDecl(location, identifier, structDecl, isPacked, std::move(fields), std::move(functions)),
          genericTypeDecls(std::move(genericTypeDecls)),
          scopeToSpecialize(std::move(scopeToSpecialize)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

struct ResolvedIntLiteral : public ResolvedExpr {
    int value;

    ResolvedIntLiteral(SourceLocation location, int value)
        : ResolvedExpr(location, Type::builtinIN("i32")), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFloatLiteral : public ResolvedExpr {
    double value;

    ResolvedFloatLiteral(SourceLocation location, double value)
        : ResolvedExpr(location, Type::builtinF64()), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCharLiteral : public ResolvedExpr {
    char value;

    ResolvedCharLiteral(SourceLocation location, char value)
        : ResolvedExpr(location, Type::builtinUN("u8")), value(value) {}

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

struct ResolvedNullLiteral : public ResolvedExpr {
    ResolvedNullLiteral(SourceLocation location) : ResolvedExpr(location, Type::builtinVoid().pointer()) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSizeofExpr : public ResolvedExpr {
    Type sizeofType;

    ResolvedSizeofExpr(SourceLocation location, Type sizeofType)
        : ResolvedExpr(location, Type::builtinUN("u64")), sizeofType(sizeofType) {}

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

    ResolvedDeclRefExpr(SourceLocation location, ResolvedDecl &decl, Type type)
        : ResolvedAssignableExpr(location, type), decl(decl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedMemberExpr : public ResolvedAssignableExpr {
    std::unique_ptr<ResolvedExpr> base;
    const ResolvedDecl &member;

    ResolvedMemberExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> base, const ResolvedDecl &member)
        : ResolvedAssignableExpr(location, member.type), base(std::move(base)), member(member) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSelfMemberExpr : public ResolvedAssignableExpr {
    std::unique_ptr<ResolvedExpr> base;
    const ResolvedDecl &member;

    ResolvedSelfMemberExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> base, const ResolvedDecl &member)
        : ResolvedAssignableExpr(location, member.type), base(std::move(base)), member(member) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedArrayAtExpr : public ResolvedAssignableExpr {
    std::unique_ptr<ResolvedExpr> array;
    std::unique_ptr<ResolvedExpr> index;

    ResolvedArrayAtExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> array,
                        std::unique_ptr<ResolvedExpr> index)
        : ResolvedAssignableExpr(location, array->type.withoutArray().remove_pointer()),
          array(std::move(array)),
          index(std::move(index)) {}

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

struct ResolvedRefPtrExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> expr;

    ResolvedRefPtrExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> expr)
        : ResolvedExpr(location, expr->type.pointer()), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedDerefPtrExpr : public ResolvedAssignableExpr {
    std::unique_ptr<ResolvedExpr> expr;

    ResolvedDerefPtrExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> expr)
        : ResolvedAssignableExpr(location, expr->type.remove_pointer()), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedDeclStmt : public ResolvedDecl, public ResolvedStmt {
    SourceLocation location;
    std::unique_ptr<ResolvedVarDecl> varDecl;

    ResolvedDeclStmt(SourceLocation location, std::unique_ptr<ResolvedVarDecl> varDecl)
        : ResolvedDecl(location, varDecl->identifier, varDecl->type, varDecl->isMutable),
          ResolvedStmt(location),
          location(location),
          varDecl(std::move(varDecl)) {}

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
    std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers;

    ResolvedReturnStmt(SourceLocation location, std::unique_ptr<ResolvedExpr> expr,
                       std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedStmt(location), expr(std::move(expr)), defers(std::move(defers)) {}

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

struct ResolvedArrayInstantiationExpr : public ResolvedExpr {
    std::vector<std::unique_ptr<ResolvedExpr>> initializers;

    ResolvedArrayInstantiationExpr(SourceLocation location, Type type,
                                   std::vector<std::unique_ptr<ResolvedExpr>> initializers)
        : ResolvedExpr(location, type), initializers(std::move(initializers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrorDecl : public ResolvedDecl {
    ResolvedErrorDecl(SourceLocation location, std::string_view identifier)
        : ResolvedDecl(location, std::move(identifier), Type::builtinError(identifier), false) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrorGroupExprDecl : public ResolvedExpr, public ResolvedDecl {
    SourceLocation location;
    std::vector<std::unique_ptr<ResolvedErrorDecl>> errors;

    ResolvedErrorGroupExprDecl(SourceLocation location, std::vector<std::unique_ptr<ResolvedErrorDecl>> errors)
        : ResolvedExpr(location, Type::errorGroupType(this)),
          ResolvedDecl(location, "", Type::errorGroupType(this), false),
          location(location),
          errors(std::move(errors)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCatchErrorExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> errorToCatch;
    std::unique_ptr<ResolvedDeclStmt> declaration;

    ResolvedCatchErrorExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> errorToCatch,
                           std::unique_ptr<ResolvedDeclStmt> declaration)
        : ResolvedExpr(location, Type::builtinBool()),
          errorToCatch(std::move(errorToCatch)),
          declaration(std::move(declaration)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTryErrorExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> errorToTry;
    std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers;

    ResolvedTryErrorExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> errorToTry,
                         std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedExpr(location, errorToTry->type.withoutOptional()),
          errorToTry(std::move(errorToTry)),
          defers(std::move(defers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedOrElseErrorExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> errorToOrElse;
    std::unique_ptr<ResolvedExpr> orElseExpr;

    ResolvedOrElseErrorExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> errorToOrElse,
                            std::unique_ptr<ResolvedExpr> orElseExpr)
        : ResolvedExpr(location, errorToOrElse->type.withoutOptional()),
          errorToOrElse(std::move(errorToOrElse)),
          orElseExpr(std::move(orElseExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedModuleDecl : public ResolvedDependencies {
    std::vector<std::unique_ptr<ResolvedDecl>> declarations;

    ResolvedModuleDecl(SourceLocation location, std::string_view identifier,
                       std::vector<std::unique_ptr<ResolvedDecl>> declarations)
        : ResolvedDependencies(location, identifier, Type::moduleType(identifier, this), false),
          declarations(std::move(declarations)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

struct ResolvedImportExpr : public ResolvedExpr {
    ResolvedModuleDecl &moduleDecl;

    ResolvedImportExpr(SourceLocation location, ResolvedModuleDecl &moduleDecl)
        : ResolvedExpr(location, Type::moduleType(moduleDecl.identifier, &moduleDecl)), moduleDecl(moduleDecl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTestDecl : public ResolvedFunctionDecl {
    ResolvedTestDecl(SourceLocation location, std::string_view identifier, const FunctionDecl *functionDecl,
                     std::unique_ptr<ResolvedBlock> body)
        : ResolvedFunctionDecl(location, identifier, Type::builtinVoid(), {}, functionDecl, std::move(body)) {
        type.isOptional = true;
    }

    void dump(size_t level = 0, bool onlySelf = false) const override;
};
}  // namespace DMZ
