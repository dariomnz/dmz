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

    void print(ptr<Node>& node) {
        gen.generate(*node);
        gen.print();
    }

    ptr<Node> fmt_ast(const ModuleDecl& modDecl);

    // Decorations
    ptr<Node> fmt_decoration(const Decoration& decl);
    ptr<Node> fmt_comment(const Comment& decl);
    ptr<Node> fmt_empty_line(const EmptyLine& decl);
    ptr<Node> fmt_block(const Block& block, bool wantWrap = true, bool needBracket = true);
    // ptr<Node> fmt_case_block(const Block& block);

    // Decl
    ptr<Node> fmt_decl(const Decl& decl);
    ptr<Node> fmt_module_decl(const ModuleDecl& decl);
    ptr<Node> fmt_func_decl(const FuncDecl& decl);
    ptr<Node> fmt_struct_decl(const StructDecl& decl);
    ptr<Node> fmt_field_decl(const FieldDecl& decl);
    ptr<Node> fmt_param_decl(const ParamDecl& decl);
    ptr<Node> fmt_error_decl(const ErrorDecl& decl);

    // Expr
    ptr<Node> fmt_expr(const Expr& expr);
    ptr<Node> fmt_decl_ref_expr(const DeclRefExpr& expr);
    ptr<Node> fmt_call_expr(const CallExpr& expr);
    ptr<Node> fmt_deref_ptr_expr(const DerefPtrExpr& expr);
    ptr<Node> fmt_member_expr(const MemberExpr& expr);
    ptr<Node> fmt_import_expr(const ImportExpr& expr);
    ptr<Node> fmt_unary_operator(const UnaryOperator& expr);
    ptr<Node> fmt_binary_operator(const BinaryOperator& expr);
    ptr<Node> fmt_grouping_expr(const GroupingExpr& expr);
    ptr<Node> fmt_error_group_expr_decl(const ErrorGroupExprDecl& expr);
    ptr<Node> fmt_catch_error_expr(const CatchErrorExpr& expr);
    ptr<Node> fmt_struct_instantiation_expr(const StructInstantiationExpr& expr);
    ptr<Node> fmt_array_instantiation_expr(const ArrayInstantiationExpr& expr);
    ptr<Node> fmt_sizeof_expr(const SizeofExpr& expr);
    ptr<Node> fmt_array_at_expr(const ArrayAtExpr& expr);
    ptr<Node> fmt_ref_ptr_expr(const RefPtrExpr& expr);
    ptr<Node> fmt_range_expr(const RangeExpr& expr);
    ptr<Node> fmt_error_in_place_expr(const ErrorInPlaceExpr& expr);
    ptr<Node> fmt_try_error_expr(const TryErrorExpr& expr);
    ptr<Node> fmt_orelse_error_expr(const OrElseErrorExpr& expr);
    ptr<Node> fmt_generic_expr(const GenericExpr& expr);

    // Stmt
    ptr<Node> fmt_stmt(const Stmt& stmt);
    ptr<Node> fmt_decl_stmt(const DeclStmt& stmt);
    ptr<Node> fmt_assignment_stmt(const Assignment& stmt);
    ptr<Node> fmt_return_stmt(const ReturnStmt& stmt);
    ptr<Node> fmt_switch_stmt(const SwitchStmt& stmt);
    ptr<Node> fmt_case_stmt(const CaseStmt& stmt);
    ptr<Node> fmt_while_stmt(const WhileStmt& stmt);
    ptr<Node> fmt_for_stmt(const ForStmt& stmt);
    ptr<Node> fmt_if_stmt(const IfStmt& stmt);
    ptr<Node> fmt_field_init_stmt(const FieldInitStmt& stmt);
};
}  // namespace fmt
}  // namespace DMZ