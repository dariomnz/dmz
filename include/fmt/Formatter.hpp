#pragma once

#include "DMZPCH.hpp"
#include "fmt/Builder.hpp"
#include "fmt/FormatterSymbols.hpp"
#include "fmt/Generator.hpp"
#include "parser/ParserSymbols.hpp"

namespace DMZ {
namespace fmt {
class Formatter {
    Generator gen;
    Builder build;

   public:
    Formatter(int max_line) : gen(max_line) {};

    void print(ref<Node> node) {
        gen.generate(*node);
        gen.print();
    }

    ref<Node> fmt_ast(const ModuleDecl& modDecl);

    // Decorations
    ref<Node> fmt_decoration(const Decoration& decl);
    ref<Node> fmt_comment(const Comment& decl);
    ref<Node> fmt_empty_line(const EmptyLine& decl);
    ref<Node> fmt_block(const Block& block);
    ref<Node> fmt_case_block(const Block& block);

    // Decl
    ref<Node> fmt_decl(const Decl& decl);
    ref<Node> fmt_module_decl(const ModuleDecl& decl);
    ref<Node> fmt_function_decl(const FunctionDecl& decl);
    ref<Node> fmt_extern_function_decl(const ExternFunctionDecl& decl);
    ref<Node> fmt_struct_decl(const StructDecl& decl);
    ref<Node> fmt_field_decl(const FieldDecl& decl);
    ref<Node> fmt_param_decl(const ParamDecl& decl);
    ref<Node> fmt_error_decl(const ErrorDecl& decl);

    // Expr
    ref<Node> fmt_expr(const Expr& expr);
    ref<Node> fmt_decl_ref_expr(const DeclRefExpr& expr);
    ref<Node> fmt_call_expr(const CallExpr& expr);
    ref<Node> fmt_deref_ptr_expr(const DerefPtrExpr& expr);
    ref<Node> fmt_member_expr(const MemberExpr& expr);
    ref<Node> fmt_import_expr(const ImportExpr& expr);
    ref<Node> fmt_unary_operator(const UnaryOperator& expr);
    ref<Node> fmt_error_group_expr_decl(const ErrorGroupExprDecl& expr);

    // Stmt
    ref<Node> fmt_stmt(const Stmt& stmt);
    ref<Node> fmt_decl_stmt(const DeclStmt& stmt);
    ref<Node> fmt_assignment_stmt(const Assignment& stmt);
    ref<Node> fmt_return_stmt(const ReturnStmt& stmt);
    ref<Node> fmt_switch_stmt(const SwitchStmt& stmt);
    ref<Node> fmt_case_stmt(const CaseStmt& stmt);
};
}  // namespace fmt
}  // namespace DMZ