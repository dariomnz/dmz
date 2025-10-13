#pragma once

#include "DMZPCHLLVM.hpp"
#include "DMZPCHSymbols.hpp"
#include "Debug.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

class Codegen {
    std::vector<ptr<ResolvedDecl>> m_resolvedTree;

    ptr<llvm::LLVMContext> m_context;
    llvm::IRBuilder<> m_builder;
    ptr<llvm::Module> m_module;

    std::map<const ResolvedDecl *, llvm::Value *> m_declarations;
    llvm::Instruction *m_allocaInsertPoint = nullptr;
    llvm::Instruction *m_memsetInsertPoint = nullptr;
    const ResolvedFuncDecl *m_currentFunction = nullptr;
    llvm::Value *m_success = nullptr;

    llvm::Value *retVal = nullptr;
    llvm::BasicBlock *retBB = nullptr;

    // Debug
    llvm::DIBuilder m_debugBuilder;
    bool m_debugSymbols = false;
    llvm::DIFile *m_currentDebugFile = nullptr;
    llvm::DIScope *m_currentDebugScope = nullptr;
    std::vector<llvm::DIScope *> m_debugScopes = {};

    class DebugScopeRAII {
        Codegen &m_codegen;

       public:
        explicit DebugScopeRAII(Codegen &cod, llvm::DIScope *debugScope) : m_codegen(cod) {
            auto emplaced = m_codegen.m_debugScopes.emplace_back(debugScope);
            m_codegen.m_currentDebugScope = emplaced;
        }
        ~DebugScopeRAII() {
            m_codegen.m_debugScopes.pop_back();
            if (!m_codegen.m_debugScopes.empty()) {
                m_codegen.m_currentDebugScope = m_codegen.m_debugScopes.back();
            }
        }
    };

   public:
    Codegen(std::vector<ptr<ResolvedModuleDecl>> resolvedTree, std::string_view sourcePath, bool debugSymbols);

    std::pair<ptr<llvm::LLVMContext>, ptr<llvm::Module>> generate_ir(bool runTest);
    llvm::Type *generate_type(const ResolvedType &type, bool noOpaque = false);
    llvm::DIType *generate_debug_type(const ResolvedType &type);
    llvm::DIFile *generate_debug_file(const SourceLocation &location);
    void generate_debug_location(const SourceLocation &location);

    std::string generate_decl_name(const ResolvedDecl &decl);
    llvm::Function *generate_function_decl(const ResolvedFuncDecl &functionDecl);
    void generate_function_body(const ResolvedFuncDecl &functionDecl);
    llvm::AllocaInst *allocate_stack_variable(const SourceLocation &location, const std::string_view identifier,
                                              const ResolvedType &type);
    void generate_block(const ResolvedBlock &block);
    llvm::Value *generate_stmt(const ResolvedStmt &stmt);
    llvm::Value *generate_return_stmt(const ResolvedReturnStmt &stmt);
    llvm::Value *generate_expr(const ResolvedExpr &expr, bool keepPointer = false);
    llvm::Value *generate_call_expr(const ResolvedCallExpr &call);
    void generate_main_wrapper(bool runTest);
    llvm::AttributeList construct_attr_list(const ResolvedTypeFunction &fnType);
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
    llvm::Value *generate_for_stmt(const ResolvedForStmt &stmt);
    llvm::Value *generate_decl_stmt(const ResolvedDeclStmt &stmt);
    llvm::Value *generate_assignment(const ResolvedAssignment &stmt);
    llvm::Value *store_value(llvm::Value *val, llvm::Value *ptr, const ResolvedType &from, const ResolvedType &to);
    llvm::Value *load_value(llvm::Value *v, const ResolvedType &type);
    llvm::Value *generate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool keepPointer);
    llvm::Value *generate_member_expr(const ResolvedMemberExpr &memberExpr, bool keepPointer);
    llvm::Value *generate_array_at_expr(const ResolvedArrayAtExpr &arrayAtExpr, bool keepPointer);
    llvm::Value *generate_temporary_struct(const ResolvedStructInstantiationExpr &sie);
    llvm::Value *generate_temporary_array(const ResolvedArrayInstantiationExpr &aie);
    llvm::StructType *get_struct_decl(const ResolvedStructDecl &structDecl);
    llvm::StructType *generate_struct_decl(const ResolvedStructDecl &structDecl);
    void generate_struct_fields(const ResolvedStructDecl &structDecl);
    void generate_struct_functions(const ResolvedStructDecl &structDecl);
    void break_into_bb(llvm::BasicBlock *targetBB);
    void generate_error_no_err();
    void generate_error_group_expr_decl(const ResolvedErrorGroupExprDecl &ErrorGroupExprDecl);
    llvm::Value *generate_error_in_place_expr(const ResolvedErrorInPlaceExpr &errorInPlaceExpr);
    llvm::Value *generate_catch_error_expr(const ResolvedCatchErrorExpr &catchErrorExpr, bool keepPointer);
    llvm::Value *generate_try_error_expr(const ResolvedTryErrorExpr &tryErrorExpr, bool keepPointer);
    llvm::Value *generate_orelse_error_expr(const ResolvedOrElseErrorExpr &orelseErrorExpr, bool keepPointer);
    void generate_module_decl(const ResolvedModuleDecl &moduleDecl);
    void generate_module_body(const ResolvedModuleDecl &moduleDecl);
    void generate_in_module_decl(const std::vector<ptr<ResolvedDecl>> &declarations);
    void generate_in_module_body(const std::vector<ptr<ResolvedDecl>> &declarations);
    llvm::Value *generate_switch_stmt(const ResolvedSwitchStmt &stmt);
    void generate_global_var_decl(const ResolvedDeclStmt &stmt);
    llvm::Value *generate_sizeof_expr(const ResolvedSizeofExpr &sizeofExpr);
    llvm::Value *generate_slice_expr(const ResolvedType &sliceType, const ResolvedExpr &from,
                                     const ResolvedRangeExpr &range);
};
}  // namespace DMZ