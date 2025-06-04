#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/IRBuilder.h>

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#pragma GCC diagnostic pop

#include <map>

#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

class Codegen {
    static llvm::orc::ThreadSafeContext get_shared_context() {
        static llvm::orc::ThreadSafeContext tsc(std::make_unique<llvm::LLVMContext>());
        return tsc;
    }
    const std::vector<std::unique_ptr<ResolvedDecl>> &m_resolvedTree;

    llvm::LLVMContext *m_context;
    llvm::IRBuilder<> m_builder;
    std::unique_ptr<llvm::Module> m_module;

    std::map<const ResolvedDecl *, llvm::Value *> m_declarations;
    llvm::Instruction *m_allocaInsertPoint;
    llvm::Instruction *m_memsetInsertPoint;
    const ResolvedFunctionDecl *m_currentFunction;
    llvm::Value *m_success = nullptr;

    llvm::Value *retVal = nullptr;
    llvm::BasicBlock *retBB = nullptr;

   public:
    Codegen(const std::vector<std::unique_ptr<ResolvedDecl>> &resolvedTree, std::string_view sourcePath);

    std::unique_ptr<llvm::orc::ThreadSafeModule> generate_ir();
    llvm::Type *generate_type(const Type &type);
    llvm::Type *generate_optional_type(const Type &type, llvm::Type *llvmType);
    void generate_function_decl(const ResolvedFuncDecl &functionDecl);
    void generate_function_body(const ResolvedFunctionDecl &functionDecl);
    llvm::AllocaInst *allocate_stack_variable(const std::string_view identifier, const Type &type);
    void generate_block(const ResolvedBlock &block);
    llvm::Value *generate_stmt(const ResolvedStmt &stmt);
    llvm::Value *generate_return_stmt(const ResolvedReturnStmt &stmt);
    llvm::Value *generate_expr(const ResolvedExpr &expr, bool keepPointer = false);
    llvm::Value *generate_call_expr(const ResolvedCallExpr &call);
    void generate_builtin_get_errno();
    // void generate_builtin_println_body(const ResolvedFunctionDecl &println);
    void generate_main_wrapper();
    llvm::AttributeList construct_attr_list(const ResolvedFuncDecl &fn);
    llvm::Value *generate_unary_operator(const ResolvedUnaryOperator &unop);
    llvm::Value *generate_binary_operator(const ResolvedBinaryOperator &binop);
    llvm::Value *cast_binary_operator(const ResolvedBinaryOperator &binop, llvm::Value *lhs, llvm::Value *rhs);
    llvm::Value *to_bool(llvm::Value *v, const Type &type);
    llvm::Value *cast_to(llvm::Value *v, const Type &from, const Type &to);
    void generate_conditional_operator(const ResolvedExpr &op, llvm::BasicBlock *trueBB, llvm::BasicBlock *falseBB);
    llvm::Function *get_current_function();
    llvm::Value *generate_if_stmt(const ResolvedIfStmt &stmt);
    llvm::Value *generate_while_stmt(const ResolvedWhileStmt &stmt);
    llvm::Value *generate_decl_stmt(const ResolvedDeclStmt &stmt);
    llvm::Value *generate_assignment(const ResolvedAssignment &stmt);
    llvm::Value *store_value(llvm::Value *val, llvm::Value *ptr, const Type &from, const Type &to);
    llvm::Value *load_value(llvm::Value *v, Type type);
    llvm::Value *generate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool keepPointer);
    llvm::Value *generate_member_expr(const ResolvedMemberExpr &memberExpr, bool keepPointer);
    llvm::Value *generate_temporary_struct(const ResolvedStructInstantiationExpr &sie);
    void generate_struct_decl(const ResolvedStructDecl &structDecl);
    void generate_struct_definition(const ResolvedStructDecl &structDecl);
    void break_into_bb(llvm::BasicBlock *targetBB);
    void generate_err_no_err();
    void generate_err_group_decl(const ResolvedErrGroupDecl &errGroupDecl);
    llvm::Value *generate_err_decl_ref_expr(const ResolvedErrDeclRefExpr &errDeclRefExpr);
    llvm::Value *generate_err_unwrap_expr(const ResolvedErrUnwrapExpr &errUnwrapExpr);
    llvm::Value *generate_catch_err_expr(const ResolvedCatchErrExpr &catchErrExpr);
    llvm::Value *generate_try_err_expr(const ResolvedTryErrExpr &tryErrExpr);
    void generate_module_decl(const ResolvedModuleDecl &moduleDecl);
    void generate_in_module_decl(const std::vector<std::unique_ptr<ResolvedDecl>> &declarations, bool isGlobal = false);
    std::string generate_symbol_name(std::string modIdentifier);
    llvm::Value *generate_switch_stmt(const ResolvedSwitchStmt &stmt);
};
}  // namespace DMZ