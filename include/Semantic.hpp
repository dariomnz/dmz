#pragma once

#include "AST.hpp"
#include "Utils.hpp"

namespace C {

class Sema{
   private:
    std::vector<std::unique_ptr<FunctionDecl>> m_ast;
    std::vector<std::vector<std::unique_ptr<ResolvedDecl>>> m_scopes;
    ResolvedFunctionDecl *currentFunction;
    class ScopeRAII {
        Sema &m_sema;

       public:
        explicit ScopeRAII(Sema &sema) : m_sema(sema) { m_sema.m_scopes.emplace_back(); }
        ~ScopeRAII() { m_sema.m_scopes.pop_back(); }
    };

   public:
    explicit Sema(std::vector<std::unique_ptr<FunctionDecl>>& ast) : m_ast(std::move(ast)) {}
    std::vector<std::unique_ptr<ResolvedDecl>> resolve_ast();

   private:
    std::pair<ResolvedDecl *, int> lookup_decl(const std::string_view id);
    bool insert_decl_to_current_scope(ResolvedDecl &decl);
    std::unique_ptr<ResolvedFunctionDecl> create_builtin_println();
    std::vector<std::unique_ptr<ResolvedFunctionDecl>> resolve_source_file();
    std::optional<Type> resolve_type(Type parsedType);
    std::unique_ptr<ResolvedFunctionDecl> resolve_function_declaration(const FunctionDecl &function);
    std::unique_ptr<ResolvedParamDecl> resolve_param_decl(const ParamDecl &param);
    std::unique_ptr<ResolvedBlock> resolve_block(const Block &block);
    std::unique_ptr<ResolvedStmt> resolve_stmt(const Statement &stmt);
    std::unique_ptr<ResolvedReturnStmt> resolve_return_stmt(const ReturnStmt &returnStmt);
    std::unique_ptr<ResolvedExpr> resolve_expr(const Expr &expr);
    std::unique_ptr<ResolvedDeclRefExpr> resolve_decl_ref_expr(const DeclRefExpr &declRefExpr, bool isCallee = false);
    std::unique_ptr<ResolvedCallExpr> resolve_call_expr(const CallExpr &call);
};
}  // namespace C