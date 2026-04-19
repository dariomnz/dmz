#pragma once

#include "DMZPCH.hpp"
#include "UtilsPtr.hpp"
#include "semantic/CFG.hpp"
#include "semantic/Constexpr.hpp"
#include "semantic/ResolvedScope.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

class Driver;
class Sema {
   public:
    ConstantExpressionEvaluator cee;

   private:
    Driver &m_driver;
    ptr<ModuleDecl> m_ast;
    std::unordered_map<std::string, ResolvedModuleDecl *> m_modules_for_import;

    void dump_scopes() const;
    void dump_modules_for_import() const;

    ResolvedScope *m_currentScope = nullptr;
    ResolvedFuncDecl *m_currentFunction = nullptr;
    ResolvedStructDecl *m_currentStruct = nullptr;
    int m_loopDepth = 0;
    std::vector<ResolvedCatchErrorExpr *> m_catchStack;

    class ScopeRAII {
        Sema &m_sema;
        ResolvedScope *m_oldScope;
        std::unique_ptr<ResolvedScope> m_ownedScope;
        ResolvedScope *m_currentScopePtr;

       public:
        explicit ScopeRAII(Sema &sema, ResolvedScope *scopeToUse = nullptr)
            : m_sema(sema), m_oldScope(sema.m_currentScope) {
            if (scopeToUse) {
                m_currentScopePtr = scopeToUse;
            } else {
                m_ownedScope = makePtr<ResolvedScope>(m_oldScope);
                m_currentScopePtr = m_ownedScope.get();
            }
            m_sema.m_currentScope = m_currentScopePtr;
        }
        ~ScopeRAII() {
            m_sema.m_currentScope = m_oldScope;
            if (m_ownedScope) dmz_unreachable(SourceLocation{}, "ScopeRAII: Scope not taken");
        }

        ResolvedScope *getScope() { return m_currentScopePtr; }
        std::unique_ptr<ResolvedScope> takeScope() { return std::move(m_ownedScope); }
    };
    ResolvedModuleDecl *m_currentModule = nullptr;

    std::vector<ResolvedTestDecl *> m_tests;

    std::vector<ResolvedDecl *> m_pending_decls;

    std::unordered_map<std::string, ResolvedStructDecl *> m_instantiatedTuples;
    std::unordered_map<const StructDecl *, ResolvedStructDecl *> m_resolvedStructs;
    std::unordered_map<std::string, ResolvedBuiltinFunctionDecl *> m_vectorBuiltins;

    std::unordered_map<std::string, ResolvedBuiltinFunctionDecl *> m_funcBuiltins = {
        {"@call", nullptr},
    };

   public:
    explicit Sema(Driver &driver, ptr<ModuleDecl> ast) : m_driver(driver), m_ast(std::move(ast)) {}

    std::vector<ptr<ResolvedModuleDecl>> resolve_ast_decl(std::filesystem::path sourcePath, bool needMain);
    bool resolve_ast_body(std::vector<ptr<ResolvedModuleDecl>> &moduleDecls);
    void fill_depends(std::vector<ptr<ResolvedModuleDecl>> &decls);
    void fill_depends(ResolvedDependencies *parent, std::vector<ptr<ResolvedDecl>> &decls);
    void mark_needed(std::vector<ptr<ResolvedModuleDecl>> &decls, bool buildTest);

   private:
    std::string resolve_decl_name(std::string_view identifier);
    ResolvedDecl *lookup(const SourceLocation &loc, const std::string_view id, bool needAddDeps = true,
                         ResolvedScope *scope = nullptr);
    ResolvedDecl *lookup_in_module(const SourceLocation &loc, const ResolvedModuleDecl &moduleDecl,
                                   const std::string_view id, bool needAddDeps = true);
    ResolvedDecl *lookup_in_struct(const SourceLocation &loc, const ResolvedStructDecl &structDecl,
                                   const std::string_view id, bool needAddDeps = true);

    bool insert_decl_to_current_scope(ResolvedDecl &decl, bool ignoreIfFound = false);
    void remove_decl_to_current_scope(ResolvedDecl &decl);
    bool insert_decl_to_module(ResolvedModuleDecl &moduleDecl, ptr<ResolvedDecl> decl);

    ptr<ResolvedType> resolve_type(const Expr &parsedType);
    ptr<ResolvedType> resolve_simd_type(const TypeSimd &simdType);
    ptr<ResolvedTypeSpecialized> resolve_specialized_type(const GenericExpr &parsedType);
    ptr<ResolvedType> re_resolve_type(const ResolvedType &type);
    ptr<ResolvedGenericTypeDecl> resolve_generic_type_decl(const GenericTypeDecl &genericTypeDecl);
    std::vector<ptr<ResolvedGenericTypeDecl>> resolve_generic_types_decl(
        const std::vector<ptr<GenericTypeDecl>> &genericTypesDecl);
    ptr<ResolvedTypeSpecialized> infer_generic_types(const SourceLocation &location,
                                                     ResolvedGenericFunctionDecl &funcDecl,
                                                     const std::vector<ptr<ResolvedExpr>> &arguments);
    bool internal_infer_type(std::unordered_map<ResolvedGenericTypeDecl *, ptr<ResolvedType>> &inferredTypes,
                             const ResolvedType &paramType, const ResolvedType &argType);

    ResolvedSpecializedFunctionDecl *specialize_generic_function(const SourceLocation &location,
                                                                 ResolvedGenericFunctionDecl &funcDecl,
                                                                 const ResolvedTypeSpecialized &specializedTypes);
    ResolvedSpecializedStructDecl *specialize_generic_struct(const SourceLocation &location,
                                                             ResolvedGenericStructDecl &struDecl,
                                                             const ResolvedTypeSpecialized &specializedTypes);
    ptr<ResolvedFuncDecl> resolve_function_decl(const FuncDecl &function);
    ptr<ResolvedMemberFunctionDecl> resolve_member_function_decl(const ResolvedDecl &parentDecl,
                                                                 const MemberFunctionDecl &function);
    ptr<ResolvedParamDecl> resolve_param_decl(const ParamDecl &param);
    ptr<ResolvedBlock> resolve_block(const Block &block);
    ptr<ResolvedStmt> resolve_stmt(const Stmt &stmt);
    ptr<ResolvedReturnStmt> resolve_return_stmt(const ReturnStmt &returnStmt);
    ptr<ResolvedExpr> resolve_expr(const Expr &expr);
    ptr<ResolvedGenericExpr> resolve_generic_expr(const GenericExpr &genericExpr);
    ptr<ResolvedLambdaExpr> resolve_lambda_expr(const LambdaExpr &expr);
    ptr<ResolvedDeclRefExpr> resolve_decl_ref_expr(const DeclRefExpr &declRefExpr);
    ptr<ResolvedCallExpr> resolve_call_expr(const CallExpr &call);
    ptr<ResolvedUnaryOperator> resolve_unary_operator(const UnaryOperator &unary);
    ptr<ResolvedRefPtrExpr> resolve_ref_ptr_expr(const RefPtrExpr &refPtrExpr);
    ptr<ResolvedDerefPtrExpr> resolve_deref_ptr_expr(const DerefPtrExpr &derefPtrExpr);
    ptr<ResolvedBinaryOperator> resolve_binary_operator(const BinaryOperator &binop);
    ptr<ResolvedGroupingExpr> resolve_grouping_expr(const GroupingExpr &grouping);
    ptr<ResolvedIfStmt> resolve_if_stmt(const IfStmt &ifStmt);
    ptr<ResolvedWhileStmt> resolve_while_stmt(const WhileStmt &whileStmt);
    ptr<ResolvedBreakStmt> resolve_break_stmt(const BreakStmt &breakStmt);
    ptr<ResolvedContinueStmt> resolve_continue_stmt(const ContinueStmt &continueStmt);
    ptr<ResolvedStmt> resolve_for_stmt(const ForStmt &forStmt);
    bool run_flow_sensitive_checks(const ResolvedFuncDecl &fn);
    bool check_return_on_all_paths(const ResolvedFuncDecl &fn, const CFG &cfg);
    ptr<ResolvedDeclStmt> resolve_decl_stmt(const DeclStmt &declStmt);
    bool resolve_decl_stmt_initialize(ResolvedDeclStmt &declStmt);
    ptr<ResolvedVarDecl> resolve_var_decl(const VarDecl &varDecl);
    bool resolve_var_decl_initialize(ResolvedVarDecl &varDecl);
    ptr<ResolvedAssignment> resolve_assignment(const Assignment &assignment);
    bool check_variable_initialization(const CFG &cfg);
    ptr<ResolvedAssignableExpr> resolve_assignable_expr(const AssignableExpr &assignableExpr);
    ptr<ResolvedMemberExpr> resolve_member_expr(const MemberExpr &memberExpr);
    ptr<ResolvedAssignableExpr> resolve_array_at_expr(const ArrayAtExpr &arrayAtExpr);
    ptr<ResolvedExpr> resolve_struct_instantiation(const StructInstantiationExpr &structInstantiation);
    ptr<ResolvedExpr> resolve_tuple_instantiation(const TupleInstantiationExpr &tupleInstantiation);
    ptr<ResolvedExpr> resolve_array_instantiation(const ArrayInstantiationExpr &arrayInstantiation);
    ptr<ResolvedStructDecl> resolve_struct_decl(const StructDecl &structDecl);
    bool resolve_struct_decl_funcs(ResolvedStructDecl &resolvedStructDecl);
    bool resolve_struct_members(ResolvedStructDecl &resolvedStructDecl);
    bool resolve_struct_body_funcs(ResolvedStructDecl &resolvedStructDecl);
    bool resolve_union_members(ResolvedUnionDecl &resolvedUnionDecl);
    ptr<ResolvedDeferStmt> resolve_defer_stmt(const DeferStmt &deferStmt);
    std::vector<ptr<ResolvedDeferRefStmt>> resolve_defer_ref_stmt(bool isScope, bool isError);
    ptr<ResolvedErrorGroupExprDecl> resolve_error_group_expr_decl(const ErrorGroupExprDecl &ErrorGroupExprDecl);
    ptr<ResolvedCatchErrorExpr> resolve_catch_error_expr(const CatchErrorExpr &catchErrorExpr);
    ptr<ResolvedTryErrorExpr> resolve_try_error_expr(const TryErrorExpr &tryErrorExpr);
    ptr<ResolvedOrElseErrorExpr> resolve_orelse_error_expr(const OrElseErrorExpr &orelseExpr);

    std::vector<ptr<ResolvedModuleDecl>> resolve_modules_decls(const std::vector<ptr<ModuleDecl>> &modules,
                                                               const std::filesystem::path &sourcePath);
    ptr<ResolvedModuleDecl> resolve_module_decl(const ModuleDecl &moduleDecl);

    // Lazy resolution: single discovery pass + on-demand resolution
    bool discover_module_decls(ResolvedModuleDecl &resolvedModuleDecl, const std::filesystem::path &sourcePath);

    // Lazy ensure methods — resolve only when needed
    bool ensure_struct_members_resolved(ResolvedStructDecl &st);
    bool ensure_struct_funcs_resolved(ResolvedStructDecl &st);
    bool ensure_struct_bodies_resolved(ResolvedStructDecl &st);
    bool ensure_fully_resolved(ResolvedDecl &decl);

    bool resolve_module_body(ResolvedModuleDecl &moduleDecl);
    bool resolve_pending_body();
    ptr<ResolvedImportExpr> resolve_import_expr(const ImportExpr &importExpr);
    ptr<ResolvedSwitchStmt> resolve_switch_stmt(const SwitchStmt &switchStmt);
    ptr<ResolvedCaseStmt> resolve_case_stmt(const CaseStmt &caseStmt, std::optional<int> constant_value, bool isInline);
    bool resolve_func_body(ResolvedFunctionDecl &function, const Block &body);
    void resolve_symbol_names(const std::vector<ptr<ResolvedModuleDecl>> &declarations);
    ResolvedBuiltinFunctionDecl *resolve_builtin_function_symbol(const std::string &fnName);
    ptr<ResolvedTypeFunction> resolve_builtin_function_expr(ResolvedExpr &callee,
                                                            ResolvedBuiltinFunctionDecl &resolvedCallee,
                                                            std::vector<ptr<ResolvedExpr>> &resolvedArguments);
    bool resolve_builtin_function(const ResolvedFunctionDecl &fnDecl);
    void resolve_builtin_test_num(const ResolvedFunctionDecl &fnDecl);
    void resolve_builtin_test_name(const ResolvedFunctionDecl &fnDecl);
    void resolve_builtin_test_run(const ResolvedFunctionDecl &fnDecl);
    void add_dependency(ResolvedDecl *decl);
    ptr<ResolvedSizeofExpr> resolve_sizeof_expr(const SizeofExpr &sizeofExpr);
    ptr<ResolvedTypeidExpr> resolve_typeid_expr(const TypeidExpr &typeidExpr);
    ptr<ResolvedTypeinfoExpr> resolve_typeinfo_expr(const TypeinfoExpr &typeinfoExpr);
    ptr<ResolvedHasMethodExpr> resolve_has_method_expr(const HasMethodExpr &hasMethodExpr);
    ResolvedBuiltinFunctionDecl *resolve_simd_buildin(const MemberExpr &memberExpr, const ResolvedExpr &resolvedBase,
                                                      const ResolvedTypeSimd &vecType);
    ptr<ResolvedSimdSizeExpr> resolve_simd_size_expr(const SimdSizeExpr &simdSizeExpr);
    ptr<ResolvedSimdSplatExpr> resolve_simdsplat_expr(const SimdSplatExpr &simdSplatExpr);
    ptr<ResolvedSimdIotaExpr> resolve_simdiota_expr(const SimdIotaExpr &simdiotaExpr);
    ptr<ResolvedRangeExpr> resolve_range_expr(const RangeExpr &rangeExpr);
    ptr<ResolvedAtomicLoadExpr> resolve_atomic_load_expr(const AtomicLoadExpr &expr);
    ptr<ResolvedAtomicStoreExpr> resolve_atomic_store_expr(const AtomicStoreExpr &expr);
    ptr<ResolvedAtomicCmpExExpr> resolve_atomic_cmpex_expr(const AtomicCmpExExpr &expr);
    ptr<ResolvedAtomicRmwExpr> resolve_atomic_rmw_expr(const AtomicRmwExpr &expr);
    void perform_implicit_cast(ptr<ResolvedExpr> &expr, const ResolvedType &expectedType);
};
}  // namespace DMZ