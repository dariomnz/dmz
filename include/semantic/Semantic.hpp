#pragma once

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "semantic/CFG.hpp"
#include "semantic/Constexpr.hpp"

namespace DMZ {

class Sema {
   public:
    class ModuleScope {
        std::unordered_map<std::string, ModuleScope> m_modules;
        std::unordered_map<std::string, ResolvedDecl *> m_decls;

       public:
        ResolvedDecl *lookup_decl(const std::string_view id);
        ModuleScope *lookup_module(const std::string_view id);
        bool insert(ResolvedDecl &decl);
        bool insert(const std::string_view id);
        void dump(size_t level = 0) const;
    };

   private:
    ConstantExpressionEvaluator cee;
    std::vector<std::unique_ptr<Decl>> m_ast;
    std::vector<std::unique_ptr<Decl>> m_ast_withoutModules;
    std::vector<std::vector<ResolvedDecl *>> m_scopes;

    static std::mutex m_moduleScopesMutex;
    static ModuleScope m_globalmodule;

    void dump_module_scopes() const;
    void dump_scopes() const;

    std::vector<std::vector<ResolvedDeferStmt *>> m_defers;
    ResolvedFuncDecl *m_currentFunction;

    std::unordered_map<const ResolvedFuncDecl *, Block *> m_functionsToResolveMap;
    ModuleID m_currentModuleID;
    ModuleID m_currentModuleIDRef;

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

   public:
    explicit Sema(std::vector<std::unique_ptr<Decl>> ast)
        : m_ast(std::move(ast)), m_globalScope(std::make_unique<ScopeRAII>(*this)) {}
    // std::vector<std::unique_ptr<ResolvedDecl>> resolve_ast();
    std::vector<std::unique_ptr<ResolvedDecl>> resolve_ast_decl();
    bool resolve_ast_body(std::vector<std::unique_ptr<ResolvedDecl>> &decls);

   private:
    template <typename T>
    std::pair<T *, int> lookup_decl(const std::string_view id);
    template <typename T>
    T *lookup_decl_in_modules(const ModuleID &moduleID, const std::string_view id);
    bool lookup_in_modules(const ModuleID &moduleID, const std::string_view id);
    bool insert_decl_to_current_scope(ResolvedDecl &decl);
    bool insert_decl_to_modules(ResolvedDecl &decl);
    // std::unique_ptr<ResolvedFunctionDecl> create_builtin_println();
    std::optional<Type> resolve_type(Type parsedType);
    std::unique_ptr<ResolvedGenericTypeDecl> resolve_generic_type_decl(const GenericTypeDecl &genericTypeDecl);
    std::unique_ptr<ResolvedGenericTypesDecl> resolve_generic_types_decl(const GenericTypesDecl &genericTypesDecl, const GenericTypes& specifiedTypes = GenericTypes{{}});
    ResolvedFuncDecl *specialize_generic_function(ResolvedFunctionDecl &funcDecl,
                                                  const GenericTypes &genericTypes);
    std::unique_ptr<ResolvedFuncDecl> resolve_function_decl(const FuncDecl &function);
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
    bool run_flow_sensitive_checks(const ResolvedFunctionDecl &fn);
    bool check_return_on_all_paths(const ResolvedFunctionDecl &fn, const CFG &cfg);
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
    bool resolve_struct_fields(ResolvedStructDecl &resolvedStructDecl);
    std::unique_ptr<ResolvedDeferStmt> resolve_defer_stmt(const DeferStmt &deferStmt);
    std::vector<std::unique_ptr<ResolvedDeferRefStmt>> resolve_defer_ref_stmt(bool isScope);
    std::unique_ptr<ResolvedErrGroupDecl> resolve_err_group_decl(const ErrGroupDecl &errGroupDecl);
    std::unique_ptr<ResolvedErrDeclRefExpr> resolve_err_decl_ref_expr(const ErrDeclRefExpr &errDeclRef);
    std::unique_ptr<ResolvedErrUnwrapExpr> resolve_err_unwrap_expr(const ErrUnwrapExpr &errUnwrapExpr);
    std::unique_ptr<ResolvedCatchErrExpr> resolve_catch_err_expr(const CatchErrExpr &catchErrExpr);
    std::unique_ptr<ResolvedTryErrExpr> resolve_try_err_expr(const TryErrExpr &tryErrExpr);
    std::unique_ptr<ResolvedModuleDecl> resolve_module_decl(const ModuleDecl &moduleDecl, const ModuleID &prevModuleID);
    bool resolve_module_body(ResolvedModuleDecl &moduleDecl);
    std::vector<std::unique_ptr<ResolvedDecl>> resolve_in_module_decl(const std::vector<std::unique_ptr<Decl>> &decls);
    bool resolve_in_module_body(const std::vector<std::unique_ptr<ResolvedDecl>> &decls);
    std::unique_ptr<ResolvedImportDecl> resolve_import_decl(const ImportDecl &importDecl, const ModuleID &prevModuleID);
    bool resolve_import_check(ResolvedImportDecl &importDecl);
    std::unique_ptr<ResolvedModuleDeclRefExpr> resolve_module_decl_ref_expr(const ModuleDeclRefExpr &moduleDeclRef,
                                                                            const ModuleID &prevModuleID);
    std::unique_ptr<ResolvedSwitchStmt> resolve_switch_stmt(const SwitchStmt &switchStmt);
    std::unique_ptr<ResolvedCaseStmt> resolve_case_stmt(const CaseStmt &caseStmt);
};
}  // namespace DMZ