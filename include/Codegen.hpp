#pragma once

#include "PH.hpp"

#include "AST.hpp"

namespace C {

class Codegen {
    std::vector<std::unique_ptr<ResolvedDecl>> m_resolvedTree;

    llvm::LLVMContext m_context;
    llvm::IRBuilder<> m_builder;
    llvm::Module m_module;

    std::map<const ResolvedDecl *, llvm::Value *> m_declarations;
    llvm::Instruction *m_allocaInsertPoint;

    llvm::Value *retVal = nullptr;
    llvm::BasicBlock *retBB = nullptr;

   public:
    Codegen(std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree, std::string_view sourcePath);

    llvm::Module *generate_ir();
    llvm::Type *generate_type(const Type &type);
    void generate_function_decl(const ResolvedFunctionDecl &functionDecl);
    void generate_function_body(const ResolvedFunctionDecl &functionDecl);
    llvm::AllocaInst *allocate_stack_variable(const std::string_view identifier, const Type &type);
    void generate_block(const ResolvedBlock &block);
    llvm::Value *generate_stmt(const ResolvedStmt &stmt);
    llvm::Value *generate_return_stmt(const ResolvedReturnStmt &stmt);
    llvm::Value *generate_expr(const ResolvedExpr &expr);
    llvm::Value *generate_call_expr(const ResolvedCallExpr &call);
    void generate_builtin_println_body(const ResolvedFunctionDecl &println);
    void generate_main_wrapper();
    llvm::AttributeList construct_attr_list(const ResolvedFunctionDecl *fn);
    llvm::Value *generate_unary_operator(const ResolvedUnaryOperator &unop);
    llvm::Value *generate_binary_operator(const ResolvedBinaryOperator &binop);
    llvm::Value *int_to_bool(llvm::Value *v);
    llvm::Value *bool_to_int(llvm::Value *v);
    void generate_conditional_operator(const ResolvedExpr &op, llvm::BasicBlock *trueBB, llvm::BasicBlock *falseBB);
    llvm::Function *get_current_function();
};
}  // namespace C