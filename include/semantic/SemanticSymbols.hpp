#pragma once

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "SemanticSymbolsTypes.hpp"
#include "lexer/Lexer.hpp"

// Forward declaration
namespace llvm {
class GlobalVariable;
}

namespace DMZ {

struct ResolvedCatchErrorExpr;

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

struct ResolvedDecl : public ConstantValueContainer<int> {
    SourceLocation location;
    std::string identifier;
    std::string symbolName;
    ptr<ResolvedType> type;
    bool isMutable;
    bool isPublic;

    ResolvedDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, bool isMutable,
                 bool isPublic)
        : location(location),
          identifier(std::move(identifier)),
          type(std::move(type)),
          isMutable(isMutable),
          isPublic(isPublic) {}
    virtual ~ResolvedDecl() = default;

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
    virtual void dump_dependencies(size_t level = 0, bool dot_format = false) const {};
    virtual std::string name() const {
        if (symbolName.empty()) return identifier;
        return symbolName;
    }

    bool is_needed();
};

struct ResolvedDependencies : public ResolvedDecl {
    bool isNeeded = true;
    std::unordered_set<ResolvedDependencies *> dependsOn;
    std::unordered_set<ResolvedDependencies *> isUsedBy;
    bool cachedIsNotNeeded = false;

    ResolvedDependencies(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, bool isMutable,
                         bool isPublic)
        : ResolvedDecl(location, identifier, std::move(type), isMutable, isPublic) {}
    virtual ~ResolvedDependencies();

    void clean_dependencies();

    virtual void dump(size_t level = 0, bool onlySelf = false) const = 0;
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
};

struct ResolvedGenericTypeDecl : public ResolvedDecl {
    ptr<ResolvedType> specializedType;

    ResolvedGenericTypeDecl(SourceLocation location, std::string_view identifier)
        : ResolvedDecl(location, identifier, makePtr<ResolvedTypeGeneric>(location, this), false, false) {}

    void dump(size_t level = 0, bool onlySelf = false) const;

    static std::string generic_types_to_str(const std::vector<ptr<ResolvedGenericTypeDecl>> &genericTypeDecls);
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
    bool isInline;

    ResolvedIfStmt(SourceLocation location, ptr<ResolvedExpr> condition, ptr<ResolvedBlock> trueBlock,
                   ptr<ResolvedBlock> falseBlock = nullptr, bool isInline = false)
        : ResolvedStmt(location),
          condition(std::move(condition)),
          trueBlock(std::move(trueBlock)),
          falseBlock(std::move(falseBlock)),
          isInline(isInline) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedWhileStmt : public ResolvedStmt {
    ptr<ResolvedExpr> condition;
    ptr<ResolvedBlock> body;

    ResolvedWhileStmt(SourceLocation location, ptr<ResolvedExpr> condition, ptr<ResolvedBlock> body)
        : ResolvedStmt(location), condition(std::move(condition)), body(std::move(body)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedBreakStmt : public ResolvedStmt {
    std::vector<ptr<ResolvedDeferRefStmt>> defers;
    ptr<ResolvedExpr> expr;
    ResolvedCatchErrorExpr* targetCatch;

    ResolvedBreakStmt(SourceLocation location, std::vector<ptr<ResolvedDeferRefStmt>> defers, 
                      ptr<ResolvedExpr> expr = nullptr, ResolvedCatchErrorExpr* targetCatch = nullptr)
        : ResolvedStmt(location), defers(std::move(defers)), expr(std::move(expr)), targetCatch(targetCatch) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedContinueStmt : public ResolvedStmt {
    std::vector<ptr<ResolvedDeferRefStmt>> defers;
    ResolvedContinueStmt(SourceLocation location, std::vector<ptr<ResolvedDeferRefStmt>> defers)
        : ResolvedStmt(location), defers(std::move(defers)) {}

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
    bool isInline;

    ResolvedForStmt(SourceLocation location, std::vector<ptr<ResolvedExpr>> conditions,
                    std::vector<ptr<ResolvedCaptureDecl>> captures, ptr<ResolvedBlock> body, bool isInline = false)
        : ResolvedStmt(location),
          conditions(std::move(conditions)),
          captures(std::move(captures)),
          body(std::move(body)),
          isInline(isInline) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedCaseStmt : public ResolvedStmt {
    std::vector<ptr<ResolvedExpr>> conditions;
    ptr<ResolvedBlock> block;

    ResolvedCaseStmt(SourceLocation location, std::vector<ptr<ResolvedExpr>> conditions, ptr<ResolvedBlock> block)
        : ResolvedStmt(location), conditions(std::move(conditions)), block(std::move(block)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
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
};

struct ResolvedParamDecl : public ResolvedDecl {
    bool isVararg = false;

    ptr<ResolvedExpr> resolvedTypeExpr = nullptr;
    ResolvedParamDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, bool isMutable,
                      bool isVararg = false)
        : ResolvedDecl(location, std::move(identifier), std::move(type), isMutable, false), isVararg(isVararg) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedFieldDecl : public ResolvedDecl {
    unsigned index;
    ptr<ResolvedExpr> default_initializer;

    ptr<ResolvedExpr> resolvedTypeExpr = nullptr;
    ResolvedFieldDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type, unsigned index,
                      ptr<ResolvedExpr> default_initializer)
        : ResolvedDecl(location, std::move(identifier), std::move(type), false, true),
          index(index),
          default_initializer(std::move(default_initializer)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedVarDecl : public ResolvedDependencies {
    ptr<ResolvedExpr> resolvedTypeExpr = nullptr;
    const VarDecl *varDecl;
    ptr<ResolvedExpr> initializer;
    bool isGlobal;

    ResolvedVarDecl(SourceLocation location, const VarDecl *varDecl, bool isPublic, std::string_view identifier,
                    ptr<ResolvedType> type, bool isMutable, ptr<ResolvedExpr> initializer = nullptr,
                    bool isGlobal = false)
        : ResolvedDependencies(location, std::move(identifier), std::move(type), isMutable, isPublic),
          varDecl(varDecl),
          initializer(std::move(initializer)),
          isGlobal(isGlobal) {}

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

struct ResolvedLambdaFunctionDecl : public ResolvedFunctionDecl {
    std::vector<ptr<ResolvedDecl>> captures;

    llvm::GlobalVariable *globalCaptureBuffer = nullptr;

    ResolvedLambdaFunctionDecl(SourceLocation location, std::string_view identifier, ptr<ResolvedType> type,
                               std::vector<ptr<ResolvedParamDecl>> params, ptr<ResolvedBlock> body,
                               std::vector<ptr<ResolvedDecl>> captures)
        : ResolvedFunctionDecl(location, false, identifier, std::move(type), std::move(params), nullptr,
                               std::move(body)),
          captures(std::move(captures)) {}

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
    std::string name() const override;
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
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
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
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
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
    bool isTuple = false;
    std::vector<ptr<ResolvedFieldDecl>> fields;
    std::vector<ptr<ResolvedMemberFunctionDecl>> functions;
    std::vector<std::string> fields_strs;
    std::vector<std::string> functions_strs;

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
                                  std::vector<ptr<ResolvedMemberFunctionDecl>> functions,
                                  ResolvedGenericStructDecl *genStruct, ptr<ResolvedTypeSpecialized> specializedTypes)
        : ResolvedStructDecl(location, isPublic, identifier, structDecl, isPacked, std::move(fields),
                             std::move(functions)),
          genStruct(genStruct),
          specializedTypes(std::move(specializedTypes)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    std::string name() const override;
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
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
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
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 64, true)),
          sizeofType(std::move(sizeofType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTypeidExpr : public ResolvedExpr {
    ptr<ResolvedExpr> typeidExpr;

    ResolvedTypeidExpr(SourceLocation location, ptr<ResolvedExpr> typeidExpr)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::Int, 32)),
          typeidExpr(std::move(typeidExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTypeinfoExpr : public ResolvedExpr {
    ptr<ResolvedExpr> typeinfoExpr;

    ResolvedTypeinfoExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> typeinfoExpr)
        : ResolvedExpr(location, std::move(type)), typeinfoExpr(std::move(typeinfoExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedHasMethodExpr : public ResolvedExpr {
    ptr<ResolvedExpr> structTypeExpr;
    std::string methodName;

    ResolvedHasMethodExpr(SourceLocation location, ptr<ResolvedExpr> structTypeExpr, std::string methodName)
        : ResolvedExpr(location, makePtr<ResolvedTypeBool>(location)),
          structTypeExpr(std::move(structTypeExpr)),
          methodName(std::move(methodName)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedSimdSizeExpr : public ResolvedExpr {
    ptr<ResolvedExpr> typeExpr;

    ResolvedSimdSizeExpr(SourceLocation location, ptr<ResolvedExpr> typeExpr)
        : ResolvedExpr(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 64, true)),
          typeExpr(std::move(typeExpr)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTypeExpr : public ResolvedExpr {
    ptr<ResolvedType> resolvedType;

    ResolvedTypeExpr(SourceLocation location, ptr<ResolvedType> resolvedType)
        : ResolvedExpr(location, resolvedType->clone()), resolvedType(std::move(resolvedType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedAssignableExpr : public ResolvedExpr {
    ResolvedAssignableExpr(SourceLocation location, ptr<ResolvedType> type) : ResolvedExpr(location, std::move(type)) {}
};

struct ResolvedTypePointerExpr : public ResolvedExpr {
    ptr<ResolvedExpr> pointerType;
    ResolvedTypePointerExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> pointerType)
        : ResolvedExpr(location, std::move(type)), pointerType(std::move(pointerType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTypeSliceExpr : public ResolvedExpr {
    ptr<ResolvedExpr> sliceType;
    ResolvedTypeSliceExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> sliceType)
        : ResolvedExpr(location, std::move(type)), sliceType(std::move(sliceType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
};

struct ResolvedTypeOptionalExpr : public ResolvedExpr {
    ptr<ResolvedExpr> optionalType;
    ResolvedTypeOptionalExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> optionalType)
        : ResolvedExpr(location, std::move(type)), optionalType(std::move(optionalType)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
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
};

struct ResolvedTypeSimdExpr : public ResolvedExpr {
    ptr<ResolvedExpr> simdType;
    ptr<ResolvedExpr> sizeExpr;
    ResolvedTypeSimdExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> simdType,
                         ptr<ResolvedExpr> sizeExpr)
        : ResolvedExpr(location, std::move(type)), simdType(std::move(simdType)), sizeExpr(std::move(sizeExpr)) {}

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

struct ResolvedLambdaExpr : public ResolvedExpr {
    ptr<ResolvedLambdaFunctionDecl> lambdaFunc;
    std::vector<ptr<ResolvedExpr>> captureInitializers;

    ResolvedLambdaExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedLambdaFunctionDecl> lambdaFunc,
                       std::vector<ptr<ResolvedExpr>> captureInitializers)
        : ResolvedExpr(location, std::move(type)),
          lambdaFunc(std::move(lambdaFunc)),
          captureInitializers(std::move(captureInitializers)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
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
    ResolvedModuleDecl *saveCurrentModule;
    ResolvedStructDecl *saveCurrentStruct;

    ResolvedDeclStmt(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedVarDecl> varDecl,
                     ResolvedModuleDecl *saveCurrentModule, ResolvedStructDecl *saveCurrentStruct)
        : ResolvedDependencies(location, varDecl->identifier, std::move(type), varDecl->isMutable, varDecl->isPublic),
          ResolvedStmt(location),
          location(location),
          varDecl(std::move(varDecl)),
          saveCurrentModule(saveCurrentModule),
          saveCurrentStruct(saveCurrentStruct) {}

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
    bool isTuple;

    ResolvedStructInstantiationExpr(SourceLocation location, ResolvedStructDecl &structDecl,
                                    std::vector<ptr<ResolvedFieldInitStmt>> fieldInitializers, bool isTuple)
        : ResolvedExpr(location, makePtr<ResolvedTypeStruct>(structDecl.type->location, &structDecl)),
          structDecl(structDecl),
          fieldInitializers(std::move(fieldInitializers)),
          isTuple(isTuple) {}

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
    ptr<ResolvedVarDecl> errorVar;
    ptr<ResolvedStmt> handler;

    ResolvedCatchErrorExpr(SourceLocation location, ptr<ResolvedType> type, ptr<ResolvedExpr> errorToCatch = nullptr,
                           ptr<ResolvedVarDecl> errorVar = nullptr, ptr<ResolvedStmt> handler = nullptr)
        : ResolvedExpr(location, std::move(type)),
          errorToCatch(std::move(errorToCatch)),
          errorVar(std::move(errorVar)),
          handler(std::move(handler)) {}

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
    int tuple_counter = 0;

    ResolvedModuleDecl(SourceLocation location, std::string_view identifier, const ModuleDecl &moduleDecl,
                       std::filesystem::path module_path, std::vector<ptr<ResolvedDecl>> declarations)
        : ResolvedDependencies(location, identifier, makePtr<ResolvedTypeModule>(location, this), false, true),
          moduleDecl(moduleDecl),
          module_path(std::move(module_path)),
          declarations(std::move(declarations)) {}

    void dump(size_t level = 0, bool onlySelf = false) const override;
    void dump_dependencies(size_t level = 0, bool dot_format = false) const override;
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
