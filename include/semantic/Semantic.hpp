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
    std::vector<std::unique_ptr<ModuleDecl>> m_ast;
    std::vector<std::unordered_map<std::string, ResolvedDecl *>> m_scopes;
    std::unordered_map<std::string, ResolvedModuleDecl *> m_modules_for_import;

    void dump_scopes() const;
    void dump_modules_for_import() const;

    std::vector<std::vector<ResolvedDeferStmt *>> m_defers;
    ResolvedFuncDecl *m_currentFunction = nullptr;

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
    std::unique_ptr<ScopeRAII> m_globalScope;
    ResolvedModuleDecl *m_actualModule = nullptr;

    std::vector<const ResolvedTestDecl *> m_tests;

    std::unordered_set<ResolvedDecl *> m_removed_decls;

   public:
    explicit Sema(std::vector<std::unique_ptr<ModuleDecl>> ast)
        : m_ast(std::move(ast)), m_globalScope(std::make_unique<ScopeRAII>(*this)) {}
    // std::vector<std::unique_ptr<ResolvedDecl>> resolve_ast();
    std::vector<std::unique_ptr<ResolvedDecl>> resolve_ast_decl();
    bool resolve_ast_body(std::vector<std::unique_ptr<ResolvedDecl>> &decls);
    void fill_depends(ResolvedDependencies *parent, std::vector<std::unique_ptr<ResolvedDecl>> &decls);
    void remove_unused(std::vector<std::unique_ptr<ResolvedDecl>> &decls, bool buildTest);
    bool recurse_needed(ResolvedDependencies &deps, bool buildTest);

   private:
    std::pair<ResolvedDecl *, int> lookup(const std::string_view id, ResolvedDeclType type, bool needAddDeps = true);
    ResolvedDecl *lookup_in_module(const ResolvedModuleDecl &moduleDecl, const std::string_view id,
                                   ResolvedDeclType type);
    ResolvedDecl *lookup_in_struct(const ResolvedStructDecl &structDecl, const std::string_view id,
                                   ResolvedDeclType type);
#define cast_lookup(id, type)                 static_cast<type *>(lookup(id, ResolvedDeclType::type).first)
#define cast_lookup_in_module(decl, id, type) static_cast<type *>(lookup_in_module(decl, id, ResolvedDeclType::type))
#define cast_lookup_in_struct(decl, id, type) static_cast<type *>(lookup_in_struct(decl, id, ResolvedDeclType::type))
    // ResolvedDecl *lookup_in_modules(const ModuleID &moduleID, const std::string_view id, ResolvedDeclType type);
    bool insert_decl_to_current_scope(ResolvedDecl &decl);
    // bool insert_decl_to_modules(ResolvedDecl &decl);
    // std::unique_ptr<ResolvedFunctionDecl> create_builtin_println();
    std::optional<Type> resolve_type(Type parsedType);
    std::unique_ptr<ResolvedGenericTypeDecl> resolve_generic_type_decl(const GenericTypeDecl &genericTypeDecl);
    std::unique_ptr<ResolvedGenericTypesDecl> resolve_generic_types_decl(
        const GenericTypesDecl &genericTypesDecl, const GenericTypes &specifiedTypes = GenericTypes{{}});
    ResolvedFuncDecl *specialize_generic_function(const ResolvedFuncDecl &parentFunc, ResolvedFunctionDecl &funcDecl,
                                                  const GenericTypes &genericTypes);
    ResolvedStructDecl *specialize_generic_struct(ResolvedStructDecl &struDecl, const GenericTypes &genericTypes);
    std::unique_ptr<ResolvedFuncDecl> resolve_function_decl(const FuncDecl &function);
    std::unique_ptr<ResolvedMemberFunctionDecl> resolve_member_function_decl(const ResolvedStructDecl &structDecl,
                                                                             const MemberFunctionDecl &function);
    std::unique_ptr<ResolvedParamDecl> resolve_param_decl(const ParamDecl &param);
    std::unique_ptr<ResolvedBlock> resolve_block(const Block &block);
    std::unique_ptr<ResolvedStmt> resolve_stmt(const Stmt &stmt);
    std::unique_ptr<ResolvedReturnStmt> resolve_return_stmt(const ReturnStmt &returnStmt);
    std::unique_ptr<ResolvedExpr> resolve_expr(const Expr &expr);
    std::unique_ptr<ResolvedDeclRefExpr> resolve_decl_ref_expr(const DeclRefExpr &declRefExpr, bool isCallee = false);
    std::unique_ptr<ResolvedCallExpr> resolve_call_expr(const CallExpr &call);
    std::unique_ptr<ResolvedUnaryOperator> resolve_unary_operator(const UnaryOperator &unary);
    std::unique_ptr<ResolvedRefPtrExpr> resolve_ref_ptr_expr(const RefPtrExpr &refPtrExpr);
    std::unique_ptr<ResolvedDerefPtrExpr> resolve_deref_ptr_expr(const DerefPtrExpr &derefPtrExpr);
    std::unique_ptr<ResolvedBinaryOperator> resolve_binary_operator(const BinaryOperator &binop);
    std::unique_ptr<ResolvedGroupingExpr> resolve_grouping_expr(const GroupingExpr &grouping);
    std::unique_ptr<ResolvedIfStmt> resolve_if_stmt(const IfStmt &ifStmt);
    std::unique_ptr<ResolvedWhileStmt> resolve_while_stmt(const WhileStmt &whileStmt);
    bool run_flow_sensitive_checks(const ResolvedFuncDecl &fn);
    bool check_return_on_all_paths(const ResolvedFuncDecl &fn, const CFG &cfg);
    std::unique_ptr<ResolvedDeclStmt> resolve_decl_stmt(const DeclStmt &declStmt);
    std::unique_ptr<ResolvedVarDecl> resolve_var_decl(const VarDecl &varDecl);
    std::unique_ptr<ResolvedAssignment> resolve_assignment(const Assignment &assignment);
    bool check_variable_initialization(const CFG &cfg);
    std::unique_ptr<ResolvedAssignableExpr> resolve_assignable_expr(const AssignableExpr &assignableExpr);
    std::unique_ptr<ResolvedMemberExpr> resolve_member_expr(const MemberExpr &memberExpr);
    std::unique_ptr<ResolvedArrayAtExpr> resolve_array_at_expr(const ArrayAtExpr &arrayAtExpr);
    std::unique_ptr<ResolvedStructInstantiationExpr> resolve_struct_instantiation(
        const StructInstantiationExpr &structInstantiation);
    std::unique_ptr<ResolvedArrayInstantiationExpr> resolve_array_instantiation(
        const ArrayInstantiationExpr &arrayInstantiation);
    std::unique_ptr<ResolvedStructDecl> resolve_struct_decl(const StructDecl &structDecl);
    bool resolve_struct_decl_funcs(ResolvedStructDecl &resolvedStructDecl);
    bool resolve_struct_members(ResolvedStructDecl &resolvedStructDecl);
    std::unique_ptr<ResolvedDeferStmt> resolve_defer_stmt(const DeferStmt &deferStmt);
    std::vector<std::unique_ptr<ResolvedDeferRefStmt>> resolve_defer_ref_stmt(bool isScope, bool isError);
    std::unique_ptr<ResolvedErrorGroupExprDecl> resolve_error_group_expr_decl(
        const ErrorGroupExprDecl &ErrorGroupExprDecl);
    std::unique_ptr<ResolvedCatchErrorExpr> resolve_catch_error_expr(const CatchErrorExpr &catchErrorExpr);
    std::unique_ptr<ResolvedTryErrorExpr> resolve_try_error_expr(const TryErrorExpr &tryErrorExpr);
    std::unique_ptr<ResolvedOrElseErrorExpr> resolve_orelse_error_expr(const OrElseErrorExpr &orelseExpr);
    std::unique_ptr<ResolvedModuleDecl> resolve_module(const ModuleDecl &moduleDecl, int level);
    bool resolve_module_decl(const ModuleDecl &moduleDecl, ResolvedModuleDecl &resolvedModuleDecl);
    bool resolve_module_body(ResolvedModuleDecl &moduleDecl);
    std::vector<std::unique_ptr<ResolvedDecl>> resolve_in_module_decl(
        const std::vector<std::unique_ptr<Decl>> &decls,
        std::vector<std::unique_ptr<ResolvedDecl>> alreadyResolved = {});
    bool resolve_in_module_body(const std::vector<std::unique_ptr<ResolvedDecl>> &decls);
    std::unique_ptr<ResolvedImportExpr> resolve_import_expr(const ImportExpr &importExpr);
    std::unique_ptr<ResolvedSwitchStmt> resolve_switch_stmt(const SwitchStmt &switchStmt);
    std::unique_ptr<ResolvedCaseStmt> resolve_case_stmt(const CaseStmt &caseStmt);
    bool resolve_func_body(ResolvedFunctionDecl &function, const Block &body);
    void resolve_symbol_names(const std::vector<std::unique_ptr<ResolvedDecl>> &declarations);
    bool resolve_builtin_function(const ResolvedFunctionDecl &fnDecl);
    void resolve_builtin_test_num(const ResolvedFunctionDecl &fnDecl);
    void resolve_builtin_test_name(const ResolvedFunctionDecl &fnDecl);
    void resolve_builtin_test_run(const ResolvedFunctionDecl &fnDecl);
    void add_dependency(ResolvedDecl *decl);
};
}  // namespace DMZ