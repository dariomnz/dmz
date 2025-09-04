#pragma once

#include "DMZPCHLLVM.hpp"
#include "DMZPCHSymbols.hpp"
#include "Debug.hpp"

namespace DMZ {

class Codegen {
    static llvm::orc::ThreadSafeContext get_shared_context() {
        static llvm::orc::ThreadSafeContext tsc(makePtr<llvm::LLVMContext>());
        return tsc;
    }
    const std::vector<ptr<ResolvedDecl>> &m_resolvedTree;

    llvm::LLVMContext *m_context;
    llvm::IRBuilder<> m_builder;
    ptr<llvm::Module> m_module;

    std::map<const ResolvedDecl *, llvm::Value *> m_declarations;
    llvm::Instruction *m_allocaInsertPoint = nullptr;
    llvm::Instruction *m_memsetInsertPoint = nullptr;
    const ResolvedFuncDecl *m_currentFunction = nullptr;
    llvm::Value *m_success = nullptr;

    llvm::Value *retVal = nullptr;
    llvm::BasicBlock *retBB = nullptr;

   public:
    Codegen(const std::vector<ptr<ResolvedDecl>> &resolvedTree, std::string_view sourcePath);

    ptr<llvm::orc::ThreadSafeModule> generate_ir(bool runTest);
    llvm::Type *generate_type(const ResolvedType &type, bool noOpaque = false);
    std::string generate_decl_name(const ResolvedDecl &decl);
    void generate_function_decl(const ResolvedFuncDecl &functionDecl);
    void generate_function_body(const ResolvedFuncDecl &functionDecl);
    llvm::AllocaInst *allocate_stack_variable(const std::string_view identifier, const ResolvedType &type);
    void generate_block(const ResolvedBlock &block);
    llvm::Value *generate_stmt(const ResolvedStmt &stmt);
    llvm::Value *generate_return_stmt(const ResolvedReturnStmt &stmt);
    llvm::Value *generate_expr(const ResolvedExpr &expr, bool keepPointer = false);
    llvm::Value *generate_call_expr(const ResolvedCallExpr &call);
    void generate_main_wrapper(bool runTest);
    llvm::AttributeList construct_attr_list(const ResolvedFuncDecl &fn);
    llvm::Value *generate_unary_operator(const ResolvedUnaryOperator &unop);
    llvm::Value *generate_ref_ptr_expr(const ResolvedRefPtrExpr &expr);
    llvm::Value *generate_deref_ptr_expr(const ResolvedDerefPtrExpr &expr, bool keepPointer = false);
    llvm::Value *generate_binary_operator(const ResolvedBinaryOperator &binop);
    llvm::Value *cast_binary_operator(const ResolvedBinaryOperator &binop, llvm::Value *lhs, llvm::Value *rhs);
    llvm::Value *to_bool(llvm::Value *v, const ResolvedType &type);
    llvm::Value *cast_to(llvm::Value *v, const ResolvedType &from, const ResolvedType &to);
    void generate_conditional_operator(const ResolvedExpr &op, llvm::BasicBlock *trueBB, llvm::BasicBlock *falseBB);
    llvm::Function *get_current_function();
    llvm::Value *generate_if_stmt(const ResolvedIfStmt &stmt);
    llvm::Value *generate_while_stmt(const ResolvedWhileStmt &stmt);
    llvm::Value *generate_decl_stmt(const ResolvedDeclStmt &stmt);
    llvm::Value *generate_assignment(const ResolvedAssignment &stmt);
    llvm::Value *store_value(llvm::Value *val, llvm::Value *ptr, const ResolvedType &from, const ResolvedType &to);
    llvm::Value *load_value(llvm::Value *v, const ResolvedType &type);
    llvm::Value *generate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool keepPointer);
    llvm::Value *generate_member_expr(const ResolvedMemberExpr &memberExpr, bool keepPointer);
    llvm::Value *generate_self_member_expr(const ResolvedSelfMemberExpr &memberExpr, bool keepPointer);
    llvm::Value *generate_array_at_expr(const ResolvedArrayAtExpr &arrayAtExpr, bool keepPointer);
    llvm::Value *generate_temporary_struct(const ResolvedStructInstantiationExpr &sie);
    llvm::Value *generate_temporary_array(const ResolvedArrayInstantiationExpr &aie);
    llvm::Type *generate_struct_decl(const ResolvedStructDecl &structDecl);
    void generate_struct_fields(const ResolvedStructDecl &structDecl);
    void generate_struct_functions(const ResolvedStructDecl &structDecl);
    void break_into_bb(llvm::BasicBlock *targetBB);
    void generate_error_no_err();
    void generate_error_group_expr_decl(const ResolvedErrorGroupExprDecl &ErrorGroupExprDecl);
    llvm::Value *generate_catch_error_expr(const ResolvedCatchErrorExpr &catchErrorExpr);
    llvm::Value *generate_try_error_expr(const ResolvedTryErrorExpr &tryErrorExpr);
    llvm::Value *generate_orelse_error_expr(const ResolvedOrElseErrorExpr &orelseErrorExpr);
    void generate_module_decl(const ResolvedModuleDecl &moduleDecl);
    void generate_module_body(const ResolvedModuleDecl &moduleDecl);
    void generate_in_module_decl(const std::vector<ptr<ResolvedDecl>> &declarations);
    void generate_in_module_body(const std::vector<ptr<ResolvedDecl>> &declarations);
    llvm::Value *generate_switch_stmt(const ResolvedSwitchStmt &stmt);
    void generate_global_var_decl(const ResolvedDeclStmt &stmt);
    llvm::Value *generate_sizeof_expr(const ResolvedSizeofExpr &sizeofExpr);
};
}  // namespace DMZ