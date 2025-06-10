#pragma once

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "lexer/Lexer.hpp"

namespace DMZ {

struct ModuleID {
    std::vector<std::string_view> modules;

    ModuleID(std::vector<std::string_view> modules) : modules(std::move(modules)) {}
    ModuleID() : modules({}) {}
    void dump() const;
    std::string to_string() const;
    bool empty() const { return modules.empty(); };
    friend std::ostream &operator<<(std::ostream &os, const ModuleID &t);
};

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
    std::string_view identifier;
    ModuleID moduleID;
    Type type;
    bool isMutable;

    ResolvedDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID, Type type, bool isMutable)
        : location(location),
          identifier(std::move(identifier)),
          moduleID(std::move(moduleID)),
          type(type),
          isMutable(isMutable) {}
    virtual ~ResolvedDecl() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
};

struct ResolvedGenericTypeDecl : public ResolvedDecl {
    std::optional<Type> specializedType;

    ResolvedGenericTypeDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID)
        : ResolvedDecl(location, identifier, std::move(moduleID), Type::customType(identifier), false) {}

    void dump(size_t level = 0, bool onlySelf = false) const;
};

struct ResolvedGenericTypesDecl {
    std::vector<std::unique_ptr<ResolvedGenericTypeDecl>> types;

    ResolvedGenericTypesDecl(std::vector<std::unique_ptr<ResolvedGenericTypeDecl>> types) : types(std::move(types)) {}

    void dump(size_t level = 0, bool onlySelf = false) const;
};

// Forward declaration
struct ResolvedDeferRefStmt;

struct ResolvedBlock : public ResolvedStmt {
    std::vector<std::unique_ptr<ResolvedStmt>> statements;
    std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers;

    ResolvedBlock(SourceLocation location, std::vector<std::unique_ptr<ResolvedStmt>> statements,
                  std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedStmt(location), statements(std::move(statements)), defers(std::move(defers)) {}

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
        : ResolvedDecl(location, std::move(identifier), ModuleID{}, type, isMutable), isVararg(isVararg) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFieldDecl : public ResolvedDecl {
    unsigned index;

    ResolvedFieldDecl(SourceLocation location, std::string_view identifier, Type type, unsigned index)
        : ResolvedDecl(location, std::move(identifier), ModuleID{}, type, false), index(index) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedVarDecl : public ResolvedDecl {
    std::unique_ptr<ResolvedExpr> initializer;

    ResolvedVarDecl(SourceLocation location, std::string_view identifier, Type type, bool isMutable,
                    std::unique_ptr<ResolvedExpr> initializer = nullptr)
        : ResolvedDecl(location, std::move(identifier), ModuleID{}, type, isMutable),
          initializer(std::move(initializer)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFuncDecl : public ResolvedDecl {
    std::vector<std::unique_ptr<ResolvedParamDecl>> params;

    ResolvedFuncDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID, Type type,
                     std::vector<std::unique_ptr<ResolvedParamDecl>> params)
        : ResolvedDecl(location, std::move(identifier), std::move(moduleID), type, false), params(std::move(params)) {}
};

struct ResolvedExternFunctionDecl : public ResolvedFuncDecl {
    ResolvedExternFunctionDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID, Type type,
                               std::vector<std::unique_ptr<ResolvedParamDecl>> params)
        : ResolvedFuncDecl(location, std::move(identifier), std::move(moduleID), type, std::move(params)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSpecializedFunctionDecl : public ResolvedFuncDecl {
    GenericTypes genericTypes;
    std::unique_ptr<ResolvedBlock> body;

    ResolvedSpecializedFunctionDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID, Type type,
                                    std::vector<std::unique_ptr<ResolvedParamDecl>> params, GenericTypes genericTypes,
                                    std::unique_ptr<ResolvedBlock> body)
        : ResolvedFuncDecl(location, std::move(identifier), std::move(moduleID), type, std::move(params)),
          genericTypes(std::move(genericTypes)),
          body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFunctionDecl : public ResolvedFuncDecl {
    std::unique_ptr<ResolvedGenericTypesDecl> genericTypes;
    const FunctionDecl *functionDecl;
    std::unique_ptr<ResolvedBlock> body;

    std::vector<std::unique_ptr<ResolvedSpecializedFunctionDecl>> specializations;

    ResolvedFunctionDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID, Type type,
                         std::vector<std::unique_ptr<ResolvedParamDecl>> params,
                         std::unique_ptr<ResolvedGenericTypesDecl> genericTypes, const FunctionDecl *functionDecl,
                         std::unique_ptr<ResolvedBlock> body)
        : ResolvedFuncDecl(location, std::move(identifier), std::move(moduleID), type, std::move(params)),
          genericTypes(std::move(genericTypes)),
          functionDecl(functionDecl),
          body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedStructDecl : public ResolvedDecl {
    std::vector<std::unique_ptr<ResolvedFieldDecl>> fields;

    ResolvedStructDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID, Type type,
                       std::vector<std::unique_ptr<ResolvedFieldDecl>> fields)
        : ResolvedDecl(location, std::move(identifier), std::move(moduleID), type, false), fields(std::move(fields)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedMemberFunctionDecl : public ResolvedFuncDecl {
    const ResolvedStructDecl &structDecl;
    std::unique_ptr<ResolvedFunctionDecl> function;

    ResolvedMemberFunctionDecl(SourceLocation location, std::string_view identifier, const ResolvedStructDecl& structDecl,
                               ModuleID moduleID, std::unique_ptr<ResolvedFunctionDecl> function)
        : ResolvedFuncDecl(location, std::move(identifier), std::move(moduleID), function->type, {}),
          structDecl(structDecl),
          function(std::move(function)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
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

struct ResolvedArrayAtExpr : public ResolvedAssignableExpr {
    std::unique_ptr<ResolvedExpr> array;
    std::unique_ptr<ResolvedExpr> index;

    ResolvedArrayAtExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> array,
                        std::unique_ptr<ResolvedExpr> index)
        : ResolvedAssignableExpr(location, array->type.withoutArray()),
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

struct ResolvedDeferStmt : public ResolvedStmt {
    std::unique_ptr<ResolvedBlock> block;

    ResolvedDeferStmt(SourceLocation location, std::unique_ptr<ResolvedBlock> block)
        : ResolvedStmt(location), block(std::move(block)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedDeferRefStmt : public ResolvedStmt {
    ResolvedDeferStmt &resolvedDefer;

    ResolvedDeferRefStmt(ResolvedDeferStmt &resolvedDefer)
        : ResolvedStmt(resolvedDefer.location), resolvedDefer(resolvedDefer) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrDecl : public ResolvedDecl {
    ResolvedErrDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID)
        : ResolvedDecl(location, std::move(identifier), std::move(moduleID), Type::builtinErr(identifier), false) {}

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

    ResolvedErrGroupDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID,
                         std::vector<std::unique_ptr<ResolvedErrDecl>> errs)
        : ResolvedDecl(location, std::move(identifier), std::move(moduleID), Type::builtinErr("errGroup"), false),
          errs(std::move(errs)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrUnwrapExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> errToUnwrap;
    std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers;

    ResolvedErrUnwrapExpr(SourceLocation location, Type unwrapType, std::unique_ptr<ResolvedExpr> errToUnwrap,
                          std::vector<std::unique_ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedExpr(location, unwrapType), errToUnwrap(std::move(errToUnwrap)), defers(std::move(defers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCatchErrExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> errToCatch;
    std::unique_ptr<ResolvedDeclStmt> declaration;

    ResolvedCatchErrExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> errToCatch,
                         std::unique_ptr<ResolvedDeclStmt> declaration)
        : ResolvedExpr(location, Type::builtinBool()),
          errToCatch(std::move(errToCatch)),
          declaration(std::move(declaration)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTryErrExpr : public ResolvedExpr {
    std::unique_ptr<ResolvedExpr> errToTry;
    std::unique_ptr<ResolvedDeclStmt> declaration;

    ResolvedTryErrExpr(SourceLocation location, std::unique_ptr<ResolvedExpr> errToTry,
                       std::unique_ptr<ResolvedDeclStmt> declaration)
        : ResolvedExpr(location, Type::builtinBool()),
          errToTry(std::move(errToTry)),
          declaration(std::move(declaration)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedModuleDecl : public ResolvedDecl {
    std::unique_ptr<ResolvedModuleDecl> nestedModule;
    std::vector<std::unique_ptr<ResolvedDecl>> declarations;

    ResolvedModuleDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID,
                       std::unique_ptr<ResolvedModuleDecl> nestedModule,
                       std::vector<std::unique_ptr<ResolvedDecl>> declarations = {})
        : ResolvedDecl(location, identifier, std::move(moduleID), Type::builtinVoid(), false),
          nestedModule(std::move(nestedModule)),
          declarations(std::move(declarations)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedImportDecl : public ResolvedDecl {
    std::unique_ptr<ResolvedImportDecl> nestedImport;
    std::string_view alias;

    ResolvedImportDecl(SourceLocation location, std::string_view identifier, ModuleID moduleID,
                       std::unique_ptr<ResolvedImportDecl> nestedImport, std::string_view alias)
        : ResolvedDecl(location, identifier, std::move(moduleID), Type::builtinVoid(), false),
          nestedImport(std::move(nestedImport)),
          alias(std::move(alias)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedModuleDeclRefExpr : public ResolvedExpr {
    ModuleID moduleID;
    std::unique_ptr<ResolvedExpr> expr;
    ResolvedModuleDeclRefExpr(SourceLocation location, ModuleID moduleID, std::unique_ptr<ResolvedExpr> expr)
        : ResolvedExpr(location, expr->type), moduleID(std::move(moduleID)), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};
}  // namespace DMZ
