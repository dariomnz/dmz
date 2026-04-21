#pragma once

#include "DMZPCH.hpp"
#include "SemanticSymbolsTypes.hpp"
#include "UtilsPtr.hpp"
#include "lexer/Lexer.hpp"
#include "parser/ParserSymbols.hpp"
#include "semantic/ResolvedScope.hpp"

namespace DMZ {

enum class ResolvedState {
    Error,          // Error in resolution
    Unresolved,     // Only name registered, nothing resolved
    InProgress,     // Resolution in progress (for cycle detection)
    DeclResolved,   // Signature/type resolved, body pending
    FullyResolved,  // Completely resolved including body
};

std::ostream &operator<<(std::ostream &os, const ResolvedState &state);

struct ResolvedCatchErrorExpr;

struct ResolvedStmt {
    SourceLocation location;

    ResolvedStmt(SourceLocation location) : location(location) {}

    virtual ~ResolvedStmt() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;

    virtual std::string_view className() const { return type_name<decltype(*this)>(); }
};

struct ResolvedExpr : public ConstantValueContainer<int>, public ResolvedStmt {
    ptr<ResolvedType> type;

    ResolvedExpr(SourceLocation location, ptr<ResolvedType> type) : ResolvedStmt(location), type(std::move(type)) {}

    virtual ~ResolvedExpr() = default;

    void dump_constant_value(size_t level) const;
    virtual bool isLiteral() const { return false; }
    DMZ_TYPE_NAME();
};

struct ResolvedDecl : public ConstantValueContainer<int> {
    SourceLocation location;
    std::string identifier;
    std::string symbolName;
    ptr<ResolvedType> type;
    bool isMutable;
    bool isPublic;
    ResolvedState state = ResolvedState::Unresolved;

    ResolvedDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, bool isMutable,
                 bool isPublic)
        : location(location),
          identifier(std::move(identifier)),
          type(std::move(type)),
          isMutable(isMutable),
          isPublic(isPublic) {}
    virtual ~ResolvedDecl() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
    virtual void dump_dependencies([[maybe_unused]] size_t level = 0, [[maybe_unused]] bool dot_format = false) const;
    virtual std::string name() const {
        if (symbolName.empty()) return identifier;
        return symbolName;
    }

    virtual std::string_view className() const { return type_name<decltype(*this)>(); }
};

struct ResolvedGenericTypeDecl : public ResolvedDecl {
    ptr<ResolvedType> specializedType;

    ResolvedGenericTypeDecl(SourceLocation location, std::string_view identifier)
        : ResolvedDecl(location, identifier, makePtr<ResolvedTypeGeneric>(location, this), false, false) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;

    DMZ_TYPE_NAME();
    static std::string generic_types_to_str(const std::vector<ptr<ResolvedGenericTypeDecl>> &genericTypeDecls);
};

// Forward declaration
struct ResolvedDeferStmt;

struct ResolvedDeferRefStmt : public ResolvedStmt {
    ResolvedDeferStmt &resolvedDefer;

    ResolvedDeferRefStmt(SourceLocation location, ResolvedDeferStmt &resolvedDefer)
        : ResolvedStmt(location), resolvedDefer(resolvedDefer) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedBlock : public ResolvedStmt {
    std::vector<ptr<ResolvedStmt>> statements;
    std::vector<ptr<ResolvedDeferRefStmt>> defers;
    ptr<ResolvedScope> scope;

    ResolvedBlock(SourceLocation location, std::vector<ptr<ResolvedStmt>> statements,
                  std::vector<ptr<ResolvedDeferRefStmt>> defers, ptr<ResolvedScope> scope)
        : ResolvedStmt(location),
          statements(std::move(statements)),
          defers(std::move(defers)),
          scope(std::move(scope)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedDeferStmt : public ResolvedStmt {
    ptr<ResolvedBlock> block;
    bool isErrDefer;

    ResolvedDeferStmt(SourceLocation location, ptr<ResolvedBlock> block, bool isErrDefer)
        : ResolvedStmt(location), block(std::move(block)), isErrDefer(isErrDefer) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedIfStmt : public ResolvedStmt {
    ptr<ResolvedExpr> condition;
    ptr<ResolvedBlock> trueBlock;
    ptr<ResolvedBlock> falseBlock;
    bool isInline;

    ResolvedIfStmt(SourceLocation location, ptr<ResolvedExpr> condition, ptr<ResolvedBlock> trueBlock,
                   ptr<ResolvedBlock> falseBlock = nullptr, bool isInline = false)
        : ResolvedStmt(location),
          condition(std::move(condition)),
          trueBlock(std::move(trueBlock)),
          falseBlock(std::move(falseBlock)),
          isInline(isInline) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedWhileStmt : public ResolvedStmt {
    ptr<ResolvedExpr> condition;
    ptr<ResolvedBlock> body;

    ResolvedWhileStmt(SourceLocation location, ptr<ResolvedExpr> condition, ptr<ResolvedBlock> body)
        : ResolvedStmt(location), condition(std::move(condition)), body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedBreakStmt : public ResolvedStmt {
    std::vector<ptr<ResolvedDeferRefStmt>> defers;
    ptr<ResolvedExpr> expr;
    ResolvedCatchErrorExpr *targetCatch;

    ResolvedBreakStmt(SourceLocation location, std::vector<ptr<ResolvedDeferRefStmt>> defers,
                      ptr<ResolvedExpr> expr = nullptr, ResolvedCatchErrorExpr *targetCatch = nullptr)
        : ResolvedStmt(location), defers(std::move(defers)), expr(std::move(expr)), targetCatch(targetCatch) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedContinueStmt : public ResolvedStmt {
    std::vector<ptr<ResolvedDeferRefStmt>> defers;
    ResolvedContinueStmt(SourceLocation location, std::vector<ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedStmt(location), defers(std::move(defers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedCaptureDecl : public ResolvedDecl {
    ResolvedCaptureDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type)
        : ResolvedDecl(location, identifier, std::move(type), false, true) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedForStmt : public ResolvedStmt {
    std::vector<ptr<ResolvedExpr>> conditions;
    std::vector<ptr<ResolvedCaptureDecl>> captures;
    ptr<ResolvedBlock> body;
    ptr<ResolvedScope> scope;
    bool isInline;

    ResolvedForStmt(SourceLocation location, std::vector<ptr<ResolvedExpr>> conditions,
                    std::vector<ptr<ResolvedCaptureDecl>> captures, ptr<ResolvedBlock> body, ptr<ResolvedScope> scope,
                    bool isInline = false)
        : ResolvedStmt(location),
          conditions(std::move(conditions)),
          captures(std::move(captures)),
          body(std::move(body)),
          scope(std::move(scope)),
          isInline(isInline) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedCaseStmt : public ResolvedStmt {
    std::vector<ptr<ResolvedExpr>> conditions;
    ptr<ResolvedBlock> block;

    ResolvedCaseStmt(SourceLocation location, std::vector<ptr<ResolvedExpr>> conditions, ptr<ResolvedBlock> block)
        : ResolvedStmt(location), conditions(std::move(conditions)), block(std::move(block)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedSwitchStmt : public ResolvedStmt {
    ptr<ResolvedExpr> condition;
    std::vector<ptr<ResolvedCaseStmt>> cases;
    ptr<ResolvedBlock> elseBlock;
    bool isInline;

    ResolvedSwitchStmt(SourceLocation location, ptr<ResolvedExpr> condition, std::vector<ptr<ResolvedCaseStmt>> cases,
                       ptr<ResolvedBlock> elseBlock, bool isInline = false)
        : ResolvedStmt(location),
          condition(std::move(condition)),
          cases(std::move(cases)),
          elseBlock(std::move(elseBlock)),
          isInline(isInline) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedParamDecl : public ResolvedDecl {
    bool isVararg = false;

    ptr<ResolvedExpr> resolvedTypeExpr = nullptr;
    ResolvedParamDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, bool isMutable,
                      bool isVararg = false)
        : ResolvedDecl(location, std::move(identifier), std::move(type), isMutable, false), isVararg(isVararg) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedFieldDecl : public ResolvedDecl {
    int index;
    ptr<ResolvedExpr> default_initializer;

    ptr<ResolvedExpr> resolvedTypeExpr = nullptr;
    ResolvedFieldDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, int index,
                      ptr<ResolvedExpr> default_initializer)
        : ResolvedDecl(location, std::move(identifier), std::move(type), false, true),
          index(index),
          default_initializer(std::move(default_initializer)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedVarDecl : public ResolvedDecl {
    ptr<ResolvedExpr> resolvedTypeExpr = nullptr;
    const VarDecl *varDecl;
    ptr<ResolvedExpr> initializer;
    bool isGlobal;
    struct ResolvedDeclStmt *parentDeclStmt = nullptr;
    bool nameResolved = false;
    ptr<ResolvedScope> scope;

    ResolvedVarDecl(SourceLocation location, const VarDecl *varDecl, bool isPublic, std::string_view identifier,
                    ptr<ResolvedType> type, bool isMutable, ptr<ResolvedScope> scope,
                    ptr<ResolvedExpr> initializer = nullptr, bool isGlobal = false)
        : ResolvedDecl(location, std::move(identifier), std::move(type), isMutable, isPublic),
          varDecl(varDecl),
          initializer(std::move(initializer)),
          isGlobal(isGlobal),
          scope(std::move(scope)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedFuncDecl : public ResolvedDecl {
    std::vector<ptr<ResolvedParamDecl>> params;
    ptr<ResolvedScope> scope;

    ResolvedFuncDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<ResolvedType> type,
                     std::vector<ptr<ResolvedParamDecl>> params, ptr<ResolvedScope> scope)
        : ResolvedDecl(location, std::move(identifier), std::move(type), false, isPublic),
          params(std::move(params)),
          scope(std::move(scope)) {}

    ResolvedTypeFunction *getFnType() const {
        if (type->kind != ResolvedTypeKind::Function)
            dmz_unreachable(location, "unexpected type in function " + type->to_str());
        return static_cast<ResolvedTypeFunction *>(type.get());
    }
};

struct ResolvedExternFunctionDecl : public ResolvedFuncDecl {
    ResolvedExternFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                               ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                               ptr<ResolvedScope> scope)
        : ResolvedFuncDecl(location, isPublic, std::move(identifier), std::move(type), std::move(params),
                           std::move(scope)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedFunctionDecl : public ResolvedFuncDecl {
    const FunctionDecl *functionDecl;
    ptr<ResolvedBlock> body;

    ResolvedFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier, ptr<ResolvedType> type,
                         std::vector<ptr<ResolvedParamDecl>> params, ptr<ResolvedScope> scope,
                         const FunctionDecl *functionDecl)
        : ResolvedFuncDecl(location, isPublic, std::move(identifier), std::move(type), std::move(params),
                           std::move(scope)),
          functionDecl(functionDecl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedLambdaFunctionDecl : public ResolvedFunctionDecl {
    std::vector<ptr<ResolvedDecl>> captures;

    llvm::GlobalVariable *globalCaptureBuffer = nullptr;

    ResolvedLambdaFunctionDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type,
                               std::vector<ptr<ResolvedParamDecl>> params, ptr<ResolvedScope> scope,
                               std::vector<ptr<ResolvedDecl>> captures)
        : ResolvedFunctionDecl(location, false, identifier, std::move(type), std::move(params), std::move(scope),
                               nullptr),
          captures(std::move(captures)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedSpecializedFunctionDecl : public ResolvedFunctionDecl {
    struct ResolvedGenericFunctionDecl *genFunc;
    ptr<ResolvedTypeSpecialized> specializedTypes;  // The types used for specialization
    ResolvedSpecializedFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                    ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                                    ptr<ResolvedScope> scope, const FunctionDecl *functionDecl,
                                    struct ResolvedGenericFunctionDecl *genFunc,
                                    ptr<ResolvedTypeSpecialized> specializedTypes)
        : ResolvedFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params), std::move(scope),
                               functionDecl),
          genFunc(genFunc),
          specializedTypes(std::move(specializedTypes)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
    std::string name() const override;
};

struct ResolvedGenericFunctionDecl : public ResolvedFunctionDecl {
    std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls = {};         // The types used for lookup
    std::vector<ptr<ResolvedSpecializedFunctionDecl>> specializations = {};  // List of specializations
    ptr<ResolvedScope> genericScope;

    ResolvedGenericFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                                ptr<ResolvedScope> scope, ptr<ResolvedScope> genericScope,
                                const FunctionDecl *functionDecl,
                                std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls)
        : ResolvedFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params), std::move(scope),
                               functionDecl),
          genericTypeDecls(std::move(genericTypeDecls)),
          genericScope(std::move(genericScope)) {}
    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
    std::string name() const override;
};

// Forward declaration
struct ResolvedStructDecl;
struct ResolvedMemberFunctionDecl : public ResolvedFunctionDecl {
    const ResolvedDecl *parentDecl;
    bool isStatic;

    ResolvedMemberFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                               ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                               ptr<ResolvedScope> scope, const FunctionDecl *functionDecl,
                               const ResolvedDecl *parentDecl, bool isStatic)
        : ResolvedFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params), std::move(scope),
                               functionDecl),
          parentDecl(parentDecl),
          isStatic(isStatic) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedBuiltinFunctionDecl : public ResolvedMemberFunctionDecl {
    ResolvedBuiltinFunctionDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type,
                                std::vector<ptr<ResolvedParamDecl>> params, bool isStatic)
        : ResolvedMemberFunctionDecl(location, true, identifier, std::move(type), std::move(params), nullptr, nullptr,
                                     nullptr, isStatic) {}
};

struct ResolvedMemberGenericFunctionDecl : public ResolvedGenericFunctionDecl {
    const ResolvedDecl *parentDecl;
    bool isStatic;
    ResolvedMemberGenericFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                      ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                                      ptr<ResolvedScope> scope, ptr<ResolvedScope> genericScope,
                                      const FunctionDecl *functionDecl,
                                      std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls,
                                      const ResolvedDecl *parentDecl, bool isStatic)
        : ResolvedGenericFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params),
                                      std::move(scope), std::move(genericScope), functionDecl,
                                      std::move(genericTypeDecls)),
          parentDecl(parentDecl),
          isStatic(isStatic) {}
    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
};

struct ResolvedMemberSpecializedFunctionDecl : public ResolvedSpecializedFunctionDecl {
    const ResolvedStructDecl *structDecl;
    bool isStatic;
    ResolvedMemberSpecializedFunctionDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                          ptr<ResolvedType> type, std::vector<ptr<ResolvedParamDecl>> params,
                                          ptr<ResolvedScope> scope, const FunctionDecl *functionDecl,
                                          struct ResolvedGenericFunctionDecl *genFunc,
                                          ptr<ResolvedTypeSpecialized> specializedTypes,
                                          const ResolvedStructDecl *structDecl, bool isStatic)
        : ResolvedSpecializedFunctionDecl(location, isPublic, identifier, std::move(type), std::move(params),
                                          std::move(scope), functionDecl, genFunc, std::move(specializedTypes)),
          structDecl(structDecl),
          isStatic(isStatic) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedStructDecl : public ResolvedDecl {
    const StructDecl *structDecl;
    bool isPacked;
    bool isTuple = false;
    std::vector<ptr<ResolvedFieldDecl>> fields;
    std::vector<ptr<ResolvedMemberFunctionDecl>> functions;
    std::vector<std::string> fields_strs;
    std::vector<std::string> functions_strs;
    ptr<ResolvedScope> scope;

    // Lazy resolution state
    bool membersResolved = false;
    bool functionsResolved = false;
    bool functionBodiesResolved = false;

    ResolvedStructDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                       const StructDecl *structDecl, bool isPacked, std::vector<ptr<ResolvedFieldDecl>> fields,
                       std::vector<ptr<ResolvedMemberFunctionDecl>> functions, ptr<ResolvedScope> scope)
        : ResolvedDecl(location, std::move(identifier), makePtr<ResolvedTypeStructDecl>(location, this), false,
                       isPublic),
          structDecl(structDecl),
          isPacked(isPacked),
          fields(std::move(fields)),
          functions(std::move(functions)),
          scope(std::move(scope)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
};

// Forward declaration
struct ResolvedGenericStructDecl;
struct ResolvedSpecializedStructDecl : public ResolvedStructDecl {
    ResolvedGenericStructDecl *genStruct;
    ptr<ResolvedTypeSpecialized> specializedTypes;  // The types used for specialization
    ResolvedSpecializedStructDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                                  const StructDecl *structDecl, bool isPacked,
                                  std::vector<ptr<ResolvedFieldDecl>> fields,
                                  std::vector<ptr<ResolvedMemberFunctionDecl>> functions, ptr<ResolvedScope> scope,
                                  ResolvedGenericStructDecl *genStruct, ptr<ResolvedTypeSpecialized> specializedTypes)
        : ResolvedStructDecl(location, isPublic, identifier, structDecl, isPacked, std::move(fields),
                             std::move(functions), std::move(scope)),
          genStruct(genStruct),
          specializedTypes(std::move(specializedTypes)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
    std::string name() const override;
};

struct ResolvedUnionDecl : public ResolvedStructDecl {
    ptr<ResolvedFieldDecl> tag;

    ResolvedUnionDecl(SourceLocation location, bool isPublic, std::string_view identifier, const UnionDecl *unionDecl,
                      bool isPacked, std::vector<ptr<ResolvedFieldDecl>> fields,
                      std::vector<ptr<ResolvedMemberFunctionDecl>> functions, ptr<ResolvedScope> scope)
        : ResolvedStructDecl(location, isPublic, identifier, unionDecl, isPacked, std::move(fields),
                             std::move(functions), std::move(scope)),
          tag(makePtr<ResolvedFieldDecl>(location, "tag", ResolvedTypeNumber::usize(location), -1, nullptr)) {
        this->type = makePtr<ResolvedTypeUnionDecl>(location, this);
    }

    const UnionDecl *unionDecl() const { return static_cast<const UnionDecl *>(structDecl); }

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
};

struct ResolvedGenericStructDecl : public ResolvedStructDecl {
    std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls = {};       // The types used for lookup
    std::vector<ptr<ResolvedSpecializedStructDecl>> specializations = {};  // List of specializations
    ptr<ResolvedScope> genericScope;

    ResolvedGenericStructDecl(SourceLocation location, bool isPublic, std::string_view identifier,
                              const StructDecl *structDecl, bool isPacked, std::vector<ptr<ResolvedFieldDecl>> fields,
                              std::vector<ptr<ResolvedMemberFunctionDecl>> functions, ptr<ResolvedScope> scope,
                              ptr<ResolvedScope> genericScope,
                              std::vector<ptr<ResolvedGenericTypeDecl>> genericTypeDecls)
        : ResolvedStructDecl(location, isPublic, identifier, structDecl, isPacked, std::move(fields),
                             std::move(functions), std::move(scope)),
          genericTypeDecls(std::move(genericTypeDecls)),
          genericScope(std::move(genericScope)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
    std::string name() const override;
};

struct ResolvedIntLiteral : public ResolvedExpr {
    int value;

    ResolvedIntLiteral(SourceLocation location, int value)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::Int, 32)), value(value) {}

    bool isLiteral() const override { return true; }
    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedFloatLiteral : public ResolvedExpr {
    double value;

    ResolvedFloatLiteral(SourceLocation location, double value)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::Float, 64)), value(value) {}

    bool isLiteral() const override { return true; }
    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedCharLiteral : public ResolvedExpr {
    char value;

    ResolvedCharLiteral(SourceLocation location, char value)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 8)), value(value) {}

    bool isLiteral() const override { return true; }
    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedBoolLiteral : public ResolvedExpr {
    bool value;

    ResolvedBoolLiteral(SourceLocation location, bool value)
        : ResolvedExpr(location, makePtr<ResolvedTypeBool>(location)), value(value) {}

    bool isLiteral() const override { return true; }
    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedStringLiteral : public ResolvedExpr {
    std::string value;

    ResolvedStringLiteral(SourceLocation location, std::string_view value)
        : ResolvedExpr(location, makePtr<ResolvedTypePointer>(
                                     location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 8))),
          value(value) {}

    bool isLiteral() const override { return true; }
    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedNullLiteral : public ResolvedExpr {
    ResolvedNullLiteral(SourceLocation location)
        : ResolvedExpr(location, makePtr<ResolvedTypePointer>(location, makePtr<ResolvedTypeVoid>(location))) {}

    bool isLiteral() const override { return true; }
    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedSizeofExpr : public ResolvedExpr {
    ptr<ResolvedType> sizeofType;

    ResolvedSizeofExpr(SourceLocation location, ptr<ResolvedType> sizeofType)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 64, true)),
          sizeofType(std::move(sizeofType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeidExpr : public ResolvedExpr {
    ptr<ResolvedExpr> typeidExpr;

    ResolvedTypeidExpr(SourceLocation location, ptr<ResolvedExpr> typeidExpr)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::Int, 32)),
          typeidExpr(std::move(typeidExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeinfoExpr : public ResolvedExpr {
    ptr<ResolvedExpr> typeinfoExpr;

    ResolvedTypeinfoExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> typeinfoExpr)
        : ResolvedExpr(location, std::move(type)), typeinfoExpr(std::move(typeinfoExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedHasMethodExpr : public ResolvedExpr {
    ptr<ResolvedExpr> structTypeExpr;
    std::string methodName;

    ResolvedHasMethodExpr(SourceLocation location, ptr<ResolvedExpr> structTypeExpr, std::string methodName)
        : ResolvedExpr(location, makePtr<ResolvedTypeBool>(location)),
          structTypeExpr(std::move(structTypeExpr)),
          methodName(std::move(methodName)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedSimdSizeExpr : public ResolvedExpr {
    ptr<ResolvedExpr> typeExpr;

    ResolvedSimdSizeExpr(SourceLocation location, ptr<ResolvedExpr> typeExpr)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 64, true)),
          typeExpr(std::move(typeExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedSimdSplatExpr : public ResolvedExpr {
    ptr<ResolvedExpr> value;

    ResolvedSimdSplatExpr(SourceLocation location, ptr<ResolvedExpr> value, ptr<ResolvedType> type)
        : ResolvedExpr(location, std::move(type)), value(std::move(value)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedSimdIotaExpr : public ResolvedExpr {
    ResolvedSimdIotaExpr(SourceLocation location, ptr<ResolvedType> type) : ResolvedExpr(location, std::move(type)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeExpr : public ResolvedExpr {
    ptr<ResolvedType> resolvedType;

    ResolvedTypeExpr(SourceLocation location, ptr<ResolvedType> resolvedType)
        : ResolvedExpr(location, resolvedType->clone()), resolvedType(std::move(resolvedType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedAssignableExpr : public ResolvedExpr {
    ResolvedAssignableExpr(SourceLocation location, ptr<ResolvedType> type) : ResolvedExpr(location, std::move(type)) {}
};

struct ResolvedTypePointerExpr : public ResolvedExpr {
    ptr<ResolvedExpr> pointerType;
    ResolvedTypePointerExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> pointerType)
        : ResolvedExpr(location, std::move(type)), pointerType(std::move(pointerType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeSliceExpr : public ResolvedExpr {
    ptr<ResolvedExpr> sliceType;
    ResolvedTypeSliceExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> sliceType)
        : ResolvedExpr(location, std::move(type)), sliceType(std::move(sliceType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeOptionalExpr : public ResolvedExpr {
    ptr<ResolvedExpr> optionalType;
    ResolvedTypeOptionalExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> optionalType)
        : ResolvedExpr(location, std::move(type)), optionalType(std::move(optionalType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeArrayExpr : public ResolvedAssignableExpr {
    ptr<ResolvedExpr> arrayType;
    ptr<ResolvedExpr> sizeExpr;
    ResolvedTypeArrayExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> arrayType,
                          ptr<ResolvedExpr> sizeExpr)
        : ResolvedAssignableExpr(location, std::move(type)),
          arrayType(std::move(arrayType)),
          sizeExpr(std::move(sizeExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeSimdExpr : public ResolvedExpr {
    ptr<ResolvedExpr> simdType;
    ptr<ResolvedExpr> sizeExpr;
    ResolvedTypeSimdExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> simdType,
                         ptr<ResolvedExpr> sizeExpr)
        : ResolvedExpr(location, std::move(type)), simdType(std::move(simdType)), sizeExpr(std::move(sizeExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedCallExpr : public ResolvedExpr {
    ptr<ResolvedExpr> callee;
    std::vector<ptr<ResolvedExpr>> arguments;

    ResolvedCallExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> callee,
                     std::vector<ptr<ResolvedExpr>> arguments)
        : ResolvedExpr(location, std::move(type)), callee(std::move(callee)), arguments(std::move(arguments)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedLambdaExpr : public ResolvedExpr {
    ptr<ResolvedLambdaFunctionDecl> lambdaFunc;
    std::vector<ptr<ResolvedExpr>> captureInitializers;
    ptr<ResolvedScope> scope;

    ResolvedLambdaExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedLambdaFunctionDecl> lambdaFunc,
                       std::vector<ptr<ResolvedExpr>> captureInitializers, ptr<ResolvedScope> scope)
        : ResolvedExpr(location, std::move(type)),
          lambdaFunc(std::move(lambdaFunc)),
          captureInitializers(std::move(captureInitializers)),
          scope(std::move(scope)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedDeclRefExpr : public ResolvedAssignableExpr {
    const ResolvedDecl &decl;

    ResolvedDeclRefExpr(SourceLocation location, ResolvedDecl &decl, ptr<ResolvedType> type)
        : ResolvedAssignableExpr(location, std::move(type)), decl(decl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedMemberExpr : public ResolvedAssignableExpr {
    ptr<ResolvedExpr> base;
    const ResolvedDecl &member;

    ResolvedMemberExpr(SourceLocation location, ptr<ResolvedExpr> base, const ResolvedDecl &member)
        : ResolvedAssignableExpr(location, member.type->clone()), base(std::move(base)), member(member) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedGenericExpr : public ResolvedAssignableExpr {
    ptr<ResolvedExpr> base;
    const ResolvedDecl &decl;
    ptr<ResolvedTypeSpecialized> specializedTypes;

    ResolvedGenericExpr(SourceLocation location, ptr<ResolvedExpr> base, const ResolvedDecl &decl,
                        ptr<ResolvedTypeSpecialized> specializedTypes)
        : ResolvedAssignableExpr(location, decl.type->clone()),
          base(std::move(base)),
          decl(decl),
          specializedTypes(std::move(specializedTypes)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedArrayAtExpr : public ResolvedAssignableExpr {
    ptr<ResolvedExpr> array;
    ptr<ResolvedExpr> index;

    ResolvedArrayAtExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> array,
                        ptr<ResolvedExpr> index)
        : ResolvedAssignableExpr(location, std::move(type)), array(std::move(array)), index(std::move(index)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedGroupingExpr : public ResolvedExpr {
    ptr<ResolvedExpr> expr;

    ResolvedGroupingExpr(SourceLocation location, ptr<ResolvedExpr> expr)
        : ResolvedExpr(location, expr->type->clone()), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedBinaryOperator : public ResolvedExpr {
    TokenType op;
    ptr<ResolvedExpr> lhs;
    ptr<ResolvedExpr> rhs;

    ResolvedBinaryOperator(SourceLocation location, TokenType op, ptr<ResolvedExpr> lhs, ptr<ResolvedExpr> rhs)
        : ResolvedExpr(location, lhs->type->clone()), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedUnaryOperator : public ResolvedExpr {
    TokenType op;
    ptr<ResolvedExpr> operand;

    ResolvedUnaryOperator(SourceLocation location, ptr<ResolvedType> type, TokenType op, ptr<ResolvedExpr> operand)
        : ResolvedExpr(location, std::move(type)), op(op), operand(std::move(operand)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedRefPtrExpr : public ResolvedExpr {
    ptr<ResolvedExpr> expr;

    ResolvedRefPtrExpr(SourceLocation location, ptr<ResolvedExpr> expr)
        : ResolvedExpr(location, makePtr<ResolvedTypePointer>(location, expr->type->clone())), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedDerefPtrExpr : public ResolvedAssignableExpr {
    ptr<ResolvedExpr> expr;

    ResolvedDerefPtrExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> expr)
        : ResolvedAssignableExpr(location, std::move(type)), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedDeclStmt : public ResolvedDecl, public ResolvedStmt {
    SourceLocation location;
    ptr<ResolvedVarDecl> varDecl;
    bool initialized = false;

    ResolvedDeclStmt(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedVarDecl> varDecl)
        : ResolvedDecl(location, varDecl->identifier, std::move(type), varDecl->isMutable, varDecl->isPublic),
          ResolvedStmt(location),
          location(location),
          varDecl(std::move(varDecl)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedAssignment : public ResolvedStmt {
    ptr<ResolvedAssignableExpr> assignee;
    ptr<ResolvedExpr> expr;

    ResolvedAssignment(SourceLocation location, ptr<ResolvedAssignableExpr> assignee, ptr<ResolvedExpr> expr)
        : ResolvedStmt(location), assignee(std::move(assignee)), expr(std::move(expr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedReturnStmt : public ResolvedStmt {
    ptr<ResolvedExpr> expr;
    std::vector<ptr<ResolvedDeferRefStmt>> defers;

    ResolvedReturnStmt(SourceLocation location, ptr<ResolvedExpr> expr, std::vector<ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedStmt(location), expr(std::move(expr)), defers(std::move(defers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedFieldInitStmt : public ResolvedStmt {
    const ResolvedFieldDecl &field;
    ptr<ResolvedExpr> initializer;

    ResolvedFieldInitStmt(SourceLocation location, const ResolvedFieldDecl &field, ptr<ResolvedExpr> initializer)
        : ResolvedStmt(location), field(field), initializer(std::move(initializer)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedStructInstantiationExpr : public ResolvedExpr {
    ResolvedStructDecl &structDecl;
    std::vector<ptr<ResolvedFieldInitStmt>> fieldInitializers;
    bool isTuple;

    ResolvedStructInstantiationExpr(SourceLocation location, ResolvedStructDecl &structDecl,
                                    std::vector<ptr<ResolvedFieldInitStmt>> fieldInitializers, bool isTuple)
        : ResolvedExpr(location, makePtr<ResolvedTypeStruct>(structDecl.type->location, &structDecl)),
          structDecl(structDecl),
          fieldInitializers(std::move(fieldInitializers)),
          isTuple(isTuple) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedUnionInstantiationExpr : public ResolvedExpr {
    ResolvedUnionDecl &unionDecl;
    ptr<ResolvedFieldInitStmt> fieldInitializer;

    ResolvedUnionInstantiationExpr(SourceLocation location, ResolvedUnionDecl &unionDecl,
                                   ptr<ResolvedFieldInitStmt> fieldInitializer)
        : ResolvedExpr(location, makePtr<ResolvedTypeUnion>(unionDecl.location, &unionDecl)),
          unionDecl(unionDecl),
          fieldInitializer(std::move(fieldInitializer)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedArrayInstantiationExpr : public ResolvedExpr {
    std::vector<ptr<ResolvedExpr>> initializers;

    ResolvedArrayInstantiationExpr(SourceLocation location, ptr<ResolvedType> type,
                                   std::vector<ptr<ResolvedExpr>> initializers)
        : ResolvedExpr(location, std::move(type)), initializers(std::move(initializers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedRangeExpr : public ResolvedExpr {
    ptr<ResolvedExpr> startExpr;
    ptr<ResolvedExpr> endExpr;

    ResolvedRangeExpr(SourceLocation location, ptr<ResolvedExpr> startExpr, ptr<ResolvedExpr> endExpr)
        : ResolvedExpr(location, makePtr<ResolvedTypeRange>(location)),
          startExpr(std::move(startExpr)),
          endExpr(std::move(endExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedErrorDecl : public ResolvedDecl {
    ResolvedErrorDecl(SourceLocation location, std::string_view identifier)
        : ResolvedDecl(location, std::move(identifier), makePtr<ResolvedTypeError>(location), false, true) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedErrorInPlaceExpr : public ResolvedExpr {
    std::string identifier;
    ResolvedErrorInPlaceExpr(SourceLocation location, std::string_view identifier)
        : ResolvedExpr(location, makePtr<ResolvedTypeError>(location)), identifier(identifier) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedErrorGroupExprDecl : public ResolvedExpr, public ResolvedDecl {
    SourceLocation location;
    std::vector<ptr<ResolvedErrorDecl>> errors;

    ResolvedErrorGroupExprDecl(SourceLocation location, std::vector<ptr<ResolvedErrorDecl>> errors)
        : ResolvedExpr(location, makePtr<ResolvedTypeErrorGroup>(location, this)),
          ResolvedDecl(location, "", makePtr<ResolvedTypeErrorGroup>(location, this), false, true),
          location(location),
          errors(std::move(errors)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedCatchErrorExpr : public ResolvedExpr {
    ptr<ResolvedScope> scope;
    ptr<ResolvedExpr> errorToCatch;
    ptr<ResolvedVarDecl> errorVar;
    ptr<ResolvedStmt> handler;

    ResolvedCatchErrorExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedScope> scope,
                           ptr<ResolvedExpr> errorToCatch = nullptr, ptr<ResolvedVarDecl> errorVar = nullptr,
                           ptr<ResolvedStmt> handler = nullptr)
        : ResolvedExpr(location, std::move(type)),
          scope(std::move(scope)),
          errorToCatch(std::move(errorToCatch)),
          errorVar(std::move(errorVar)),
          handler(std::move(handler)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTryErrorExpr : public ResolvedExpr {
    ptr<ResolvedExpr> errorToTry;
    std::vector<ptr<ResolvedDeferRefStmt>> defers;

    ResolvedTryErrorExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> errorToTry,
                         std::vector<ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedExpr(location, std::move(type)), errorToTry(std::move(errorToTry)), defers(std::move(defers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
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
    DMZ_TYPE_NAME();
};

struct ResolvedModuleDecl : public ResolvedDecl {
    ptr<ModuleDecl> moduleDecl;
    std::filesystem::path module_path;
    std::vector<ptr<ResolvedDecl>> declarations;
    ptr<ResolvedScope> scope;
    int tuple_counter = 0;

    ResolvedModuleDecl(SourceLocation location, std::string_view identifier, ptr<ModuleDecl> moduleDecl,
                       std::filesystem::path module_path, std::vector<ptr<ResolvedDecl>> declarations,
                       ptr<ResolvedScope> scope)
        : ResolvedDecl(location, identifier, makePtr<ResolvedTypeModule>(location, this), false, true),
          moduleDecl(std::move(moduleDecl)),
          module_path(std::move(module_path)),
          declarations(std::move(declarations)),
          scope(std::move(scope)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
};

struct ResolvedImportExpr : public ResolvedExpr {
    ResolvedModuleDecl &moduleDecl;

    ResolvedImportExpr(SourceLocation location, ResolvedModuleDecl &moduleDecl)
        : ResolvedExpr(location, makePtr<ResolvedTypeModule>(location, &moduleDecl)), moduleDecl(moduleDecl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTestDecl : public ResolvedFunctionDecl {
    ResolvedTestDecl(SourceLocation location, std::string_view identifier, const FunctionDecl *functionDecl,
                     ptr<ResolvedScope> scope)
        : ResolvedFunctionDecl(location, true, identifier,
                               makePtr<ResolvedTypeFunction>(
                                   location, this, std::vector<ptr<ResolvedType>>{},
                                   makePtr<ResolvedTypeOptional>(location, makePtr<ResolvedTypeVoid>(location))),
                               {}, std::move(scope), functionDecl) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};
struct ResolvedAtomicLoadExpr : public ResolvedExpr {
    ptr<ResolvedExpr> ptrExpr;
    ResolvedAtomicLoadExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> ptrExpr)
        : ResolvedExpr(location, std::move(type)), ptrExpr(std::move(ptrExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedAtomicStoreExpr : public ResolvedExpr {
    ptr<ResolvedExpr> ptrExpr;
    ptr<ResolvedExpr> valExpr;
    ResolvedAtomicStoreExpr(SourceLocation location, ptr<ResolvedExpr> ptrExpr, ptr<ResolvedExpr> valExpr)
        : ResolvedExpr(location, makePtr<ResolvedTypeVoid>(location)),
          ptrExpr(std::move(ptrExpr)),
          valExpr(std::move(valExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedAtomicCmpExExpr : public ResolvedExpr {
    ptr<ResolvedExpr> ptrExpr;
    ptr<ResolvedExpr> expected;
    ptr<ResolvedExpr> replacement;
    bool isWeak;

    ResolvedAtomicCmpExExpr(SourceLocation location, ptr<ResolvedType> resultType, ptr<ResolvedExpr> ptrExpr,
                            ptr<ResolvedExpr> expected, ptr<ResolvedExpr> replacement, bool isWeak)
        : ResolvedExpr(location, std::move(resultType)),
          ptrExpr(std::move(ptrExpr)),
          expected(std::move(expected)),
          replacement(std::move(replacement)),
          isWeak(isWeak) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

struct ResolvedAtomicRmwExpr : public ResolvedExpr {
    ptr<ResolvedExpr> ptrExpr;
    TokenType op;
    ptr<ResolvedExpr> valExpr;
    ResolvedAtomicRmwExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> ptrExpr, TokenType op,
                          ptr<ResolvedExpr> valExpr)
        : ResolvedExpr(location, std::move(type)), ptrExpr(std::move(ptrExpr)), op(op), valExpr(std::move(valExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    DMZ_TYPE_NAME();
};

}  // namespace DMZ
