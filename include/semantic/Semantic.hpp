#pragma once

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "Debug.hpp"
#include "semantic/CFG.hpp"
#include "semantic/Constexpr.hpp"

namespace DMZ {

class Sema {
   private:
    ConstantExpressionEvaluator cee;
    std::vector<ptr<ModuleDecl>> m_ast;
    std::unordered_map<std::string, ResolvedModuleDecl *> m_modules_for_import;

    void dump_scopes() const;
    void dump_modules_for_import() const;

    std::vector<std::unordered_map<std::string, ResolvedDecl *>> m_scopes;
    std::vector<std::vector<ResolvedDeferStmt *>> m_defers;
    ResolvedFuncDecl *m_currentFunction = nullptr;
    ResolvedStructDecl *m_currentStruct = nullptr;

    class ScopeRAII {
        Sema &m_sema;

       public:
        explicit ScopeRAII(Sema &sema) : m_sema(sema) {
            m_sema.m_scopes.emplace_back();
            m_sema.m_defers.emplace_back();
        }
        ~ScopeRAII() {
            m_sema.m_scopes.pop_back();
            m_sema.m_defers.pop_back();
        }
    };
    ptr<ScopeRAII> m_globalScope;
    ResolvedModuleDecl *m_currentModule = nullptr;

    std::vector<ResolvedTestDecl *> m_tests;

    std::unordered_set<ResolvedDecl *> m_removed_decls;

   public:
    explicit Sema(std::vector<ptr<ModuleDecl>> ast) : m_ast(std::move(ast)), m_globalScope(makePtr<ScopeRAII>(*this)) {}
    // std::vector<ref<ResolvedDecl>> resolve_ast();
    std::vector<ptr<ResolvedModuleDecl>> resolve_ast_decl();
    bool resolve_import_modules(std::vector<ptr<ResolvedModuleDecl>> &out_resolvedModules);
    bool resolve_ast_body(std::vector<ptr<ResolvedModuleDecl>> &moduleDecls);
    void fill_depends(std::vector<ptr<ResolvedModuleDecl>> &decls);
    void fill_depends(ResolvedDependencies *parent, std::vector<ptr<ResolvedDecl>> &decls);
    void remove_unused(std::vector<ptr<ResolvedModuleDecl>> &decls, bool buildTest);
    void remove_unused(std::vector<ptr<ResolvedDecl>> &decls, bool buildTest);
    bool recurse_needed(ResolvedDependencies &deps, bool buildTest,
                        std::unordered_set<ResolvedDependencies *> &recurse_check);

   private:
    ResolvedDecl *lookup(const SourceLocation &loc, const std::string_view id, bool needAddDeps = true);
    ResolvedDecl *lookup_in_module(const SourceLocation &loc, const ResolvedModuleDecl &moduleDecl,
                                   const std::string_view id);
    ResolvedDecl *lookup_in_struct(const SourceLocation &loc, const ResolvedStructDecl &structDecl,
                                   const std::string_view id);
    // ResolvedDecl *lookup_in_modules(const ModuleID &moduleID, const std::string_view id, ResolvedDeclType type);
    bool insert_decl_to_current_scope(ResolvedDecl &decl);
    bool insert_decl_to_module(ResolvedModuleDecl &moduleDecl, ptr<ResolvedDecl> decl);
    std::vector<ResolvedDecl *> collect_scope();
    // bool insert_decl_to_modules(ResolvedDecl &decl);
    // ref<ResolvedFunctionDecl> create_builtin_println();
    ptr<ResolvedType> resolve_type(const Expr &parsedType);
    ptr<ResolvedTypeSpecialized> resolve_specialized_type(const GenericExpr &parsedType);
    ptr<ResolvedType> re_resolve_type(const ResolvedType &type);
    ptr<ResolvedGenericTypeDecl> resolve_generic_type_decl(const GenericTypeDecl &genericTypeDecl);
    std::vector<ptr<ResolvedGenericTypeDecl>> resolve_generic_types_decl(
        const std::vector<ptr<GenericTypeDecl>> &genericTypesDecl);
    ResolvedSpecializedFunctionDecl *specialize_generic_function(const SourceLocation &location,
                                                                 ResolvedGenericFunctionDecl &funcDecl,
                                                                 const ResolvedTypeSpecialized &specializedTypes);
    ResolvedSpecializedStructDecl *specialize_generic_struct(const SourceLocation &location,
                                                             ResolvedGenericStructDecl &struDecl,
                                                             const ResolvedTypeSpecialized &specializedTypes);
    ptr<ResolvedFuncDecl> resolve_function_decl(const FuncDecl &function);
    ptr<ResolvedMemberFunctionDecl> resolve_member_function_decl(const ResolvedStructDecl &structDecl,
                                                                 const MemberFunctionDecl &function);
    ptr<ResolvedParamDecl> resolve_param_decl(const ParamDecl &param);
    ptr<ResolvedBlock> resolve_block(const Block &block);
    ptr<ResolvedStmt> resolve_stmt(const Stmt &stmt);
    ptr<ResolvedReturnStmt> resolve_return_stmt(const ReturnStmt &returnStmt);
    ptr<ResolvedExpr> resolve_expr(const Expr &expr);
    ptr<ResolvedDeclRefExpr> resolve_generic_expr(const GenericExpr &genericExpr);
    ptr<ResolvedDeclRefExpr> resolve_decl_ref_expr(const DeclRefExpr &declRefExpr);
    ptr<ResolvedCallExpr> resolve_call_expr(const CallExpr &call);
    ptr<ResolvedUnaryOperator> resolve_unary_operator(const UnaryOperator &unary);
    ptr<ResolvedRefPtrExpr> resolve_ref_ptr_expr(const RefPtrExpr &refPtrExpr);
    ptr<ResolvedDerefPtrExpr> resolve_deref_ptr_expr(const DerefPtrExpr &derefPtrExpr);
    ptr<ResolvedBinaryOperator> resolve_binary_operator(const BinaryOperator &binop);
    ptr<ResolvedGroupingExpr> resolve_grouping_expr(const GroupingExpr &grouping);
    ptr<ResolvedIfStmt> resolve_if_stmt(const IfStmt &ifStmt);
    ptr<ResolvedWhileStmt> resolve_while_stmt(const WhileStmt &whileStmt);
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
    ptr<ResolvedSelfMemberExpr> resolve_self_member_expr(const SelfMemberExpr &memberExpr);
    ptr<ResolvedArrayAtExpr> resolve_array_at_expr(const ArrayAtExpr &arrayAtExpr);
    ptr<ResolvedStructInstantiationExpr> resolve_struct_instantiation(
        const StructInstantiationExpr &structInstantiation);
    ptr<ResolvedArrayInstantiationExpr> resolve_array_instantiation(const ArrayInstantiationExpr &arrayInstantiation);
    ptr<ResolvedStructDecl> resolve_struct_decl(const StructDecl &structDecl);
    bool resolve_struct_decl_funcs(ResolvedStructDecl &resolvedStructDecl);
    bool resolve_struct_members(ResolvedStructDecl &resolvedStructDecl);
    ptr<ResolvedDeferStmt> resolve_defer_stmt(const DeferStmt &deferStmt);
    std::vector<ptr<ResolvedDeferRefStmt>> resolve_defer_ref_stmt(bool isScope, bool isError);
    ptr<ResolvedErrorGroupExprDecl> resolve_error_group_expr_decl(const ErrorGroupExprDecl &ErrorGroupExprDecl);
    ptr<ResolvedCatchErrorExpr> resolve_catch_error_expr(const CatchErrorExpr &catchErrorExpr);
    ptr<ResolvedTryErrorExpr> resolve_try_error_expr(const TryErrorExpr &tryErrorExpr);
    ptr<ResolvedOrElseErrorExpr> resolve_orelse_error_expr(const OrElseErrorExpr &orelseExpr);

    std::vector<ptr<ResolvedModuleDecl>> resolve_modules_decls(const std::vector<ptr<ModuleDecl>> &modules);
    ptr<ResolvedModuleDecl> resolve_module_decl(const ModuleDecl &moduleDecl);
    bool resolve_module_struct_decls(ResolvedModuleDecl &resolvedModuleDecl);
    bool resolve_module_decl_stmts(ResolvedModuleDecl &resolvedModuleDecl);
    bool resolve_module_struct_decl_funcs(ResolvedModuleDecl &resolvedModuleDecl);
    bool resolve_module_function_decls(ResolvedModuleDecl &resolvedModuleDecl);

    // bool resolve_module_decl(const ModuleDecl &moduleDecl, ResolvedModuleDecl &resolvedModuleDecl);
    bool resolve_module_body(ResolvedModuleDecl &moduleDecl);
    // std::vector<ptr<ResolvedDecl>> resolve_in_module_decl(const std::vector<ptr<Decl>> &decls,
    //                                                       std::vector<ptr<ResolvedDecl>> alreadyResolved = {});
    // bool resolve_in_module_body(const std::vector<ptr<ResolvedDecl>> &decls);
    ptr<ResolvedImportExpr> resolve_import_expr(const ImportExpr &importExpr);
    ptr<ResolvedSwitchStmt> resolve_switch_stmt(const SwitchStmt &switchStmt);
    ptr<ResolvedCaseStmt> resolve_case_stmt(const CaseStmt &caseStmt);
    bool resolve_func_body(ResolvedFunctionDecl &function, const Block &body);
    void resolve_symbol_names(const std::vector<ptr<ResolvedModuleDecl>> &declarations);
    bool resolve_builtin_function(const ResolvedFunctionDecl &fnDecl);
    void resolve_builtin_test_num(const ResolvedFunctionDecl &fnDecl);
    void resolve_builtin_test_name(const ResolvedFunctionDecl &fnDecl);
    void resolve_builtin_test_run(const ResolvedFunctionDecl &fnDecl);
    void add_dependency(ResolvedDecl *decl);
    ptr<ResolvedSizeofExpr> resolve_sizeof_expr(const SizeofExpr &sizeofExpr);
    ptr<ResolvedRangeExpr> resolve_range_expr(const RangeExpr &rangeExpr);
};
}  // namespace DMZ