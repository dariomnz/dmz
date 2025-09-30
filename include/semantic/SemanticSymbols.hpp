#pragma once

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "SemanticSymbolsTypes.hpp"
#include "lexer/Lexer.hpp"

namespace DMZ {

struct ResolvedStmt {
    SourceLocation location;

    ResolvedStmt(SourceLocation location) : location(location) {}

    virtual ~ResolvedStmt() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
};

struct ResolvedExpr : public ConstantValueContainer<int>, public ResolvedStmt {
    ptr<ResolvedType> type;

    ResolvedExpr(SourceLocation location, ptr<ResolvedType> type) : ResolvedStmt(location), type(std::move(type)) {}

    virtual ~ResolvedExpr() = default;

    void dump_constant_value(size_t level) const;
};

struct ResolvedDecl {
    SourceLocation location;
    std::string identifier;
    std::string symbolName;
    ptr<ResolvedType> type;
    bool isMutable;
    bool isPublic;
    bool isDependency = false;

    ResolvedDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, bool isMutable,
                 bool isPublic)
        : location(location),
          identifier(std::move(identifier)),
          type(std::move(type)),
          isMutable(isMutable),
          isPublic(isPublic) {}
    virtual ~ResolvedDecl() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
    virtual void dump_dependencies(size_t level = 0) const {};
    inline std::string name() const {
        if (symbolName.empty()) return identifier;
        return symbolName;
    }
};

struct ResolvedDependencies : public ResolvedDecl {
    std::unordered_set<ResolvedDependencies *> dependsOn;
    std::unordered_set<ResolvedDependencies *> isUsedBy;
    bool cachedIsNotNeeded = false;

    ResolvedDependencies(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, bool isMutable,
                         bool isPublic)
        : ResolvedDecl(location, identifier, std::move(type), isMutable, isPublic) {
        isDependency = true;
    }
    virtual ~ResolvedDependencies() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
    void dump_dependencies(size_t level = 0) const override;
};

struct ResolvedGenericTypeDecl : public ResolvedDecl {
    ptr<ResolvedType> specializedType;

    ResolvedGenericTypeDecl(SourceLocation location, std::string_view identifier)
        : ResolvedDecl(location, identifier, makePtr<ResolvedTypeGeneric>(location, this), false, false) {}

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
    std::vector<ptr<ResolvedStmt>> statements;
    std::vector<ptr<ResolvedDeferRefStmt>> defers;

    ResolvedBlock(SourceLocation location, std::vector<ptr<ResolvedStmt>> statements,
                  std::vector<ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedStmt(location), statements(std::move(statements)), defers(std::move(defers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedDeferStmt : public ResolvedStmt {
    ptr<ResolvedBlock> block;
    bool isErrDefer;

    ResolvedDeferStmt(SourceLocation location, ptr<ResolvedBlock> block, bool isErrDefer)
        : ResolvedStmt(location), block(std::move(block)), isErrDefer(isErrDefer) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedIfStmt : public ResolvedStmt {
    ptr<ResolvedExpr> condition;
    ptr<ResolvedBlock> trueBlock;
    ptr<ResolvedBlock> falseBlock;

    ResolvedIfStmt(SourceLocation location, ptr<ResolvedExpr> condition, ptr<ResolvedBlock> trueBlock,
                   ptr<ResolvedBlock> falseBlock = nullptr)
        : ResolvedStmt(location),
          condition(std::move(condition)),
          trueBlock(std::move(trueBlock)),
          falseBlock(std::move(falseBlock)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedWhileStmt : public ResolvedStmt {
    ptr<ResolvedExpr> condition;
    ptr<ResolvedBlock> body;

    ResolvedWhileStmt(SourceLocation location, ptr<ResolvedExpr> condition, ptr<ResolvedBlock> body)
        : ResolvedStmt(location), condition(std::move(condition)), body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCaptureDecl : public ResolvedDecl {
    ResolvedCaptureDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type)
        : ResolvedDecl(location, identifier, std::move(type), false, true) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedForStmt : public ResolvedStmt {
    std::vector<ptr<ResolvedExpr>> conditions;
    std::vector<ptr<ResolvedCaptureDecl>> captures;
    ptr<ResolvedBlock> body;

    ResolvedForStmt(SourceLocation location, std::vector<ptr<ResolvedExpr>> conditions,
                    std::vector<ptr<ResolvedCaptureDecl>> captures, ptr<ResolvedBlock> body)
        : ResolvedStmt(location),
          conditions(std::move(conditions)),
          captures(std::move(captures)),
          body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCaseStmt : public ResolvedStmt {
    ptr<ResolvedExpr> condition;
    ptr<ResolvedBlock> block;

    ResolvedCaseStmt(SourceLocation location, ptr<ResolvedExpr> condition, ptr<ResolvedBlock> block)
        : ResolvedStmt(location), condition(std::move(condition)), block(std::move(block)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSwitchStmt : public ResolvedStmt {
    ptr<ResolvedExpr> condition;
    std::vector<ptr<ResolvedCaseStmt>> cases;
    ptr<ResolvedBlock> elseBlock;

    ResolvedSwitchStmt(SourceLocation location, ptr<ResolvedExpr> condition, std::vector<ptr<ResolvedCaseStmt>> cases,
                       ptr<ResolvedBlock> elseBlock)
        : ResolvedStmt(location),
          condition(std::move(condition)),
          cases(std::move(cases)),
          elseBlock(std::move(elseBlock)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedParamDecl : public ResolvedDecl {
    bool isVararg = false;

    ResolvedParamDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, bool isMutable,
                      bool isVararg = false)
        : ResolvedDecl(location, std::move(identifier), std::move(type), isMutable, false), isVararg(isVararg) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFieldDecl : public ResolvedDecl {
    unsigned index;

    ResolvedFieldDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, unsigned index)
        : ResolvedDecl(location, std::move(identifier), std::move(type), false, true), index(index) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedVarDecl : public ResolvedDependencies {
    const VarDecl *varDecl;
    ptr<ResolvedExpr> initializer;

    ResolvedVarDecl(SourceLocation location, const VarDecl *varDecl, bool isPublic, std::string_view identifier,
                    ptr<ResolvedType> type, bool isMutable, ptr<ResolvedExpr> initializer = nullptr)
        : ResolvedDependencies(location, std::move(identifier), std::move(type), isMutable, isPublic),
          varDecl(varDecl),
          initializer(std::move(initializer)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFuncDecl : public ResolvedDependencies {
    std::vector<ptr<ResolvedParamDecl>> params;

    ResolvedFuncDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<ResolvedType> type,
                     std::vector<ptr<ResolvedParamDecl>> params)
        : ResolvedDependencies(location, std::move(identifier), std::move(type), false, isPublic),
          params(std::move(params)) {}

    ResolvedTypeFunction *getFnType() const {
        if (type->kind != ResolvedTypeKind::Function) dmz_unreachable("unexpected type in function " + type->to_str());
        return static_cast<ResolvedTypeFunction *>(type.get());
    }
};

struct ResolvedExternFunctionDecl : public ResolvedFuncDecl {
    ResolvedExternFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                               ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params)
        : ResolvedFuncDecl(location, isPublic, std::move(identifier), std::move(type), std::move(params)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFunctionDecl : public ResolvedFuncDecl {
    const FunctionDecl *functionDecl;
    ptr<ResolvedBlock> body;

    ResolvedFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<ResolvedType> type,
                         std::vector<ptr<ResolvedParamDecl>> params, const FunctionDecl *functionDecl,
                         ptr<ResolvedBlock> body)
        : ResolvedFuncDecl(location, isPublic, std::move(identifier), std::move(type), std::move(params)),
          functionDecl(functionDecl),
          body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSpecializedFunctionDecl : public ResolvedFunctionDecl {
    ptr<ResolvedTypeSpecialized> specializedTypes;  // The types used for specialization
    ResolvedSpecializedFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                    ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                                    const FunctionDecl *functionDecl, ptr<ResolvedBlock> body,
                                    ptr<ResolvedTypeSpecialized> specializedTypes)
        : ResolvedFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params), functionDecl,
                               std::move(body)),
          specializedTypes(std::move(specializedTypes)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedGenericFunctionDecl : public ResolvedFunctionDecl {
    std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls = {};         // The types used for lookup
    std::vector<ptr<ResolvedSpecializedFunctionDecl>> specializations = {};  // List of specializations
    std::vector<ResolvedDecl *> scopeToSpecialize;                           // Scope use to specialize
    ResolvedModuleDecl *saveCurrentModule;
    ResolvedStructDecl *saveCurrentStruct;

    ResolvedGenericFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                                const FunctionDecl *functionDecl, ptr<ResolvedBlock> body,
                                std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls,
                                std::vector<ResolvedDecl *> scopeToSpecialize, ResolvedModuleDecl *saveCurrentModule,
                                ResolvedStructDecl *saveCurrentStruct)
        : ResolvedFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params), functionDecl,
                               std::move(body)),
          genericTypeDecls(std::move(genericTypeDecls)),
          scopeToSpecialize(std::move(scopeToSpecialize)),
          saveCurrentModule(saveCurrentModule),
          saveCurrentStruct(saveCurrentStruct) {}
    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

// Forward declaration
struct ResolvedStructDecl;
struct ResolvedMemberFunctionDecl : public ResolvedFunctionDecl {
    const ResolvedStructDecl *structDecl;
    bool isStatic;

    ResolvedMemberFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                               ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                               const FunctionDecl *functionDecl, ptr<ResolvedBlock> body,
                               const ResolvedStructDecl *structDecl, bool isStatic)
        : ResolvedFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params), functionDecl,
                               std::move(body)),
          structDecl(structDecl),
          isStatic(isStatic) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedMemberGenericFunctionDecl : public ResolvedGenericFunctionDecl {
    const ResolvedStructDecl *structDecl;
    ResolvedMemberGenericFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                      ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                                      const FunctionDecl *functionDecl, ptr<ResolvedBlock> body,
                                      std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls,
                                      std::vector<ResolvedDecl *> scopeToSpecialize,
                                      ResolvedModuleDecl *saveCurrentModule, ResolvedStructDecl *saveCurrentStruct,
                                      const ResolvedStructDecl *structDecl)
        : ResolvedGenericFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params), functionDecl,
                                      std::move(body), std::move(genericTypeDecls), std::move(scopeToSpecialize),
                                      saveCurrentModule, saveCurrentStruct),
          structDecl(structDecl) {}
    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

struct ResolvedMemberSpecializedFunctionDecl : public ResolvedSpecializedFunctionDecl {
    const ResolvedStructDecl *structDecl;
    ResolvedMemberSpecializedFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                          ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                                          const FunctionDecl *functionDecl, ptr<ResolvedBlock> body,
                                          ptr<ResolvedTypeSpecialized> specializedTypes,
                                          const ResolvedStructDecl *structDecl)
        : ResolvedSpecializedFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params),
                                          functionDecl, std::move(body), std::move(specializedTypes)),
          structDecl(structDecl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedStructDecl : public ResolvedDependencies {
    const StructDecl *structDecl;
    bool isPacked;
    std::vector<ptr<ResolvedFieldDecl>> fields;
    std::vector<ptr<ResolvedMemberFunctionDecl>> functions;

    ResolvedStructDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                       const StructDecl *structDecl, bool isPacked, std::vector<ptr<ResolvedFieldDecl>> fields,
                       std::vector<ptr<ResolvedMemberFunctionDecl>> functions)
        : ResolvedDependencies(location, std::move(identifier), makePtr<ResolvedTypeStructDecl>(location, this), false,
                               isPublic),
          structDecl(structDecl),
          isPacked(isPacked),
          fields(std::move(fields)),
          functions(std::move(functions)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

struct ResolvedSpecializedStructDecl : public ResolvedStructDecl {
    ptr<ResolvedTypeSpecialized> specializedTypes;  // The types used for specialization
    ResolvedSpecializedStructDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                  const StructDecl *structDecl, bool isPacked,
                                  std::vector<ptr<ResolvedFieldDecl>> fields,
                                  std::vector<ptr<ResolvedMemberFunctionDecl>> functions,
                                  ptr<ResolvedTypeSpecialized> specializedTypes)
        : ResolvedStructDecl(location, isPublic, identifier, structDecl, isPacked, std::move(fields),
                             std::move(functions)),
          specializedTypes(std::move(specializedTypes)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedGenericStructDecl : public ResolvedStructDecl {
    std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls = {};       // The types used for lookup
    std::vector<ptr<ResolvedSpecializedStructDecl>> specializations = {};  // List of specializations
    std::vector<ResolvedDecl *> scopeToSpecialize;                         // Scope use to specialize
    ResolvedModuleDecl *saveCurrentModule;

    ResolvedGenericStructDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                              const StructDecl *structDecl, bool isPacked, std::vector<ptr<ResolvedFieldDecl>> fields,
                              std::vector<ptr<ResolvedMemberFunctionDecl>> functions,
                              std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls,
                              std::vector<ResolvedDecl *> scopeToSpecialize, ResolvedModuleDecl *saveCurrentModule)
        : ResolvedStructDecl(location, isPublic, identifier, structDecl, isPacked, std::move(fields),
                             std::move(functions)),
          genericTypeDecls(std::move(genericTypeDecls)),
          scopeToSpecialize(std::move(scopeToSpecialize)),
          saveCurrentModule(saveCurrentModule) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

struct ResolvedIntLiteral : public ResolvedExpr {
    int value;

    ResolvedIntLiteral(SourceLocation location, int value)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::Int, 32)), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFloatLiteral : public ResolvedExpr {
    double value;

    ResolvedFloatLiteral(SourceLocation location, double value)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::Float, 64)), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCharLiteral : public ResolvedExpr {
    char value;

    ResolvedCharLiteral(SourceLocation location, char value)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 8)), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedBoolLiteral : public ResolvedExpr {
    bool value;

    ResolvedBoolLiteral(SourceLocation location, bool value)
        : ResolvedExpr(location, makePtr<ResolvedTypeBool>(location)), value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedStringLiteral : public ResolvedExpr {
    std::string value;

    ResolvedStringLiteral(SourceLocation location, std::string_view value)
        : ResolvedExpr(location, makePtr<ResolvedTypePointer>(
                                     location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 8))),
          value(value) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedNullLiteral : public ResolvedExpr {
    ResolvedNullLiteral(SourceLocation location)
        : ResolvedExpr(location, makePtr<ResolvedTypePointer>(location, makePtr<ResolvedTypeVoid>(location))) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSizeofExpr : public ResolvedExpr {
    ptr<ResolvedType> sizeofType;

    ResolvedSizeofExpr(SourceLocation location, ptr<ResolvedType> sizeofType)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 64)),
          sizeofType(std::move(sizeofType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCallExpr : public ResolvedExpr {
    ptr<ResolvedExpr> callee;
    std::vector<ptr<ResolvedExpr>> arguments;

    ResolvedCallExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> callee,
                     std::vector<ptr<ResolvedExpr>> arguments)
        : ResolvedExpr(location, std::move(type)), callee(std::move(callee)), arguments(std::move(arguments)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedAssignableExpr : public ResolvedExpr {
    ResolvedAssignableExpr(SourceLocation location, ptr<ResolvedType> type) : ResolvedExpr(location, std::move(type)) {}
};

struct ResolvedDeclRefExpr : public ResolvedAssignableExpr {
    const ResolvedDecl &decl;

    ResolvedDeclRefExpr(SourceLocation location, ResolvedDecl &decl, ptr<ResolvedType> type)
        : ResolvedAssignableExpr(location, std::move(type)), decl(decl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedMemberExpr : public ResolvedAssignableExpr {
    ptr<ResolvedExpr> base;
    const ResolvedDecl &member;

    ResolvedMemberExpr(SourceLocation location, ptr<ResolvedExpr> base, const ResolvedDecl &member)
        : ResolvedAssignableExpr(location, member.type->clone()), base(std::move(base)), member(member) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSelfMemberExpr : public ResolvedAssignableExpr {
    ptr<ResolvedExpr> base;
    const ResolvedDecl &member;

    ResolvedSelfMemberExpr(SourceLocation location, ptr<ResolvedExpr> base, const ResolvedDecl &member)
        : ResolvedAssignableExpr(location, member.type->clone()), base(std::move(base)), member(member) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedArrayAtExpr : public ResolvedAssignableExpr {
    ptr<ResolvedExpr> array;
    ptr<ResolvedExpr> index;

    ResolvedArrayAtExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> array,
                        ptr<ResolvedExpr> index)
        : ResolvedAssignableExpr(location, std::move(type)), array(std::move(array)), index(std::move(index)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedGroupingExpr : public ResolvedExpr {
    ptr<ResolvedExpr> expr;

    ResolvedGroupingExpr(SourceLocation location, ptr<ResolvedExpr> expr)
        : ResolvedExpr(location, expr->type->clone()), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedBinaryOperator : public ResolvedExpr {
    TokenType op;
    ptr<ResolvedExpr> lhs;
    ptr<ResolvedExpr> rhs;

    ResolvedBinaryOperator(SourceLocation location, TokenType op, ptr<ResolvedExpr> lhs, ptr<ResolvedExpr> rhs)
        : ResolvedExpr(location, lhs->type->clone()), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedUnaryOperator : public ResolvedExpr {
    TokenType op;
    ptr<ResolvedExpr> operand;

    ResolvedUnaryOperator(SourceLocation location, ptr<ResolvedType> type, TokenType op, ptr<ResolvedExpr> operand)
        : ResolvedExpr(location, std::move(type)), op(op), operand(std::move(operand)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedRefPtrExpr : public ResolvedExpr {
    ptr<ResolvedExpr> expr;

    ResolvedRefPtrExpr(SourceLocation location, ptr<ResolvedExpr> expr)
        : ResolvedExpr(location, makePtr<ResolvedTypePointer>(location, expr->type->clone())), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedDerefPtrExpr : public ResolvedAssignableExpr {
    ptr<ResolvedExpr> expr;

    ResolvedDerefPtrExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> expr)
        : ResolvedAssignableExpr(location, std::move(type)), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedDeclStmt : public ResolvedDependencies, public ResolvedStmt {
    SourceLocation location;
    ptr<ResolvedVarDecl> varDecl;
    bool initialized = false;

    ResolvedDeclStmt(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedVarDecl> varDecl)
        : ResolvedDependencies(location, varDecl->identifier, std::move(type), varDecl->isMutable, varDecl->isPublic),
          ResolvedStmt(location),
          location(location),
          varDecl(std::move(varDecl)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedAssignment : public ResolvedStmt {
    ptr<ResolvedAssignableExpr> assignee;
    ptr<ResolvedExpr> expr;

    ResolvedAssignment(SourceLocation location, ptr<ResolvedAssignableExpr> assignee, ptr<ResolvedExpr> expr)
        : ResolvedStmt(location), assignee(std::move(assignee)), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedReturnStmt : public ResolvedStmt {
    ptr<ResolvedExpr> expr;
    std::vector<ptr<ResolvedDeferRefStmt>> defers;

    ResolvedReturnStmt(SourceLocation location, ptr<ResolvedExpr> expr, std::vector<ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedStmt(location), expr(std::move(expr)), defers(std::move(defers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFieldInitStmt : public ResolvedStmt {
    const ResolvedFieldDecl &field;
    ptr<ResolvedExpr> initializer;

    ResolvedFieldInitStmt(SourceLocation location, const ResolvedFieldDecl &field, ptr<ResolvedExpr> initializer)
        : ResolvedStmt(location), field(field), initializer(std::move(initializer)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedStructInstantiationExpr : public ResolvedExpr {
    ResolvedStructDecl &structDecl;
    std::vector<ptr<ResolvedFieldInitStmt>> fieldInitializers;

    ResolvedStructInstantiationExpr(SourceLocation location, ResolvedStructDecl &structDecl,
                                    std::vector<ptr<ResolvedFieldInitStmt>> fieldInitializers)
        : ResolvedExpr(location, makePtr<ResolvedTypeStruct>(structDecl.type->location, &structDecl)),
          structDecl(structDecl),
          fieldInitializers(std::move(fieldInitializers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedArrayInstantiationExpr : public ResolvedExpr {
    std::vector<ptr<ResolvedExpr>> initializers;

    ResolvedArrayInstantiationExpr(SourceLocation location, ptr<ResolvedType> type,
                                   std::vector<ptr<ResolvedExpr>> initializers)
        : ResolvedExpr(location, std::move(type)), initializers(std::move(initializers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedRangeExpr : public ResolvedExpr {
    ptr<ResolvedExpr> startExpr;
    ptr<ResolvedExpr> endExpr;

    ResolvedRangeExpr(SourceLocation location, ptr<ResolvedExpr> startExpr, ptr<ResolvedExpr> endExpr)
        : ResolvedExpr(location, makePtr<ResolvedTypeRange>(location)),
          startExpr(std::move(startExpr)),
          endExpr(std::move(endExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrorDecl : public ResolvedDecl {
    ResolvedErrorDecl(SourceLocation location, std::string_view identifier)
        : ResolvedDecl(location, std::move(identifier), makePtr<ResolvedTypeError>(location), false, true) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrorInPlaceExpr : public ResolvedExpr {
    std::string identifier;
    ResolvedErrorInPlaceExpr(SourceLocation location, std::string_view identifier)
        : ResolvedExpr(location, makePtr<ResolvedTypeError>(location)), identifier(identifier) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedErrorGroupExprDecl : public ResolvedExpr, public ResolvedDependencies {
    SourceLocation location;
    std::vector<ptr<ResolvedErrorDecl>> errors;

    ResolvedErrorGroupExprDecl(SourceLocation location, std::vector<ptr<ResolvedErrorDecl>> errors)
        : ResolvedExpr(location, makePtr<ResolvedTypeErrorGroup>(location, this)),
          ResolvedDependencies(location, "", makePtr<ResolvedTypeErrorGroup>(location, this), false, true),
          location(location),
          errors(std::move(errors)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCatchErrorExpr : public ResolvedExpr {
    ptr<ResolvedExpr> errorToCatch;

    ResolvedCatchErrorExpr(SourceLocation location, ptr<ResolvedExpr> errorToCatch)
        : ResolvedExpr(location, makePtr<ResolvedTypeError>(location)), errorToCatch(std::move(errorToCatch)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTryErrorExpr : public ResolvedExpr {
    ptr<ResolvedExpr> errorToTry;
    std::vector<ptr<ResolvedDeferRefStmt>> defers;

    ResolvedTryErrorExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> errorToTry,
                         std::vector<ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedExpr(location, std::move(type)), errorToTry(std::move(errorToTry)), defers(std::move(defers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedOrElseErrorExpr : public ResolvedExpr {
    ptr<ResolvedExpr> errorToOrElse;
    ptr<ResolvedExpr> orElseExpr;

    ResolvedOrElseErrorExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> errorToOrElse,
                            ptr<ResolvedExpr> orElseExpr)
        : ResolvedExpr(location, std::move(type)),
          errorToOrElse(std::move(errorToOrElse)),
          orElseExpr(std::move(orElseExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedModuleDecl : public ResolvedDependencies {
    const ModuleDecl &moduleDecl;
    std::filesystem::path module_path;
    std::vector<ptr<ResolvedDecl>> declarations;

    ResolvedModuleDecl(SourceLocation location, std::string_view identifier, const ModuleDecl &moduleDecl,
                       std::filesystem::path module_path, std::vector<ptr<ResolvedDecl>> declarations)
        : ResolvedDependencies(location, identifier, makePtr<ResolvedTypeModule>(location, this), false, true),
          moduleDecl(moduleDecl),
          module_path(std::move(module_path)),
          declarations(std::move(declarations)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0) const override;
};

struct ResolvedImportExpr : public ResolvedExpr {
    ResolvedModuleDecl &moduleDecl;

    ResolvedImportExpr(SourceLocation location, ResolvedModuleDecl &moduleDecl)
        : ResolvedExpr(location, makePtr<ResolvedTypeModule>(location, &moduleDecl)), moduleDecl(moduleDecl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTestDecl : public ResolvedFunctionDecl {
    ResolvedTestDecl(SourceLocation location, std::string_view identifier, const FunctionDecl *functionDecl,
                     ptr<ResolvedBlock> body)
        : ResolvedFunctionDecl(location, true, identifier,
                               makePtr<ResolvedTypeFunction>(
                                   location, this, std::vector<ptr<ResolvedType>>{},
                                   makePtr<ResolvedTypeOptional>(location, makePtr<ResolvedTypeVoid>(location))),
                               {}, functionDecl, std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};
}  // namespace DMZ
