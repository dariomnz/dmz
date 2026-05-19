#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#pragma GCC diagnostic pop
#include <stack>
#include <unordered_map>
#include <unordered_set>

#include "UtilsPtr.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

class Codegen {
    std::vector<ptr<ResolvedDecl>> m_resolvedTree;
    bool m_noRemoveUnused;
    bool m_isModule;
    std::filesystem::path m_testDir;

    ptr<llvm::LLVMContext> m_context;
    llvm::IRBuilder<> m_builder;

   public:
    ptr<llvm::Module> m_module;

   private:
    const ResolvedModuleDecl *m_currentModule = nullptr;
    std::unordered_map<const ResolvedDecl *, llvm::Value *> m_declarations;
    std::unordered_map<std::string, llvm::GlobalVariable *> m_globalStrings;
    llvm::Instruction *m_allocaInsertPoint = nullptr;
    llvm::Instruction *m_memsetInsertPoint = nullptr;
    const ResolvedFuncDecl *m_currentFunction = nullptr;
    llvm::Value *m_success = nullptr;
    llvm::Value *m_errorTraceGlobal = nullptr;
    llvm::StructType *m_errorTraceType = nullptr;
    llvm::StructType *m_errorTraceEntryType = nullptr;

    struct CatchBreakTarget {
        llvm::Value *valueAddr;
        llvm::BasicBlock *exitBB;
    };
    std::unordered_map<const ResolvedCatchErrorExpr *, CatchBreakTarget> m_catchBreakTargets;
    std::unordered_set<const ResolvedDecl *> m_resolvedDecls;
    std::unordered_set<const ResolvedDecl *> m_pendingDecls;

    void generate_decl(const ResolvedDecl &decl);
    void generate_body(const ResolvedDecl &decl);

    llvm::Value *retVal = nullptr;
    llvm::BasicBlock *retBB = nullptr;

    std::stack<llvm::BasicBlock *> m_loopExitStack;
    std::stack<llvm::BasicBlock *> m_loopContinueStack;

    // Debug
    llvm::DIBuilder m_debugBuilder;
    bool m_debugSymbols = false;
    llvm::DIFile *m_currentDebugFile = nullptr;
    llvm::DIScope *m_currentDebugScope = nullptr;
    std::vector<llvm::DIScope *> m_debugScopes = {};
    std::unordered_map<std::string, llvm::DIType *> m_debugTypes;
    std::stack<llvm::DILocation *> m_DebugScopeStack = {};

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
            } else {
                m_codegen.m_currentDebugScope = nullptr;
            }
        }
    };

   public:
    Codegen(std::vector<ptr<ResolvedModuleDecl>> resolvedTree, std::string_view sourcePath, bool debugSymbols,
            bool noRemoveUnused, bool isModule, std::filesystem::path testDir = {});

    std::pair<ptr<llvm::LLVMContext>, ptr<llvm::Module>> generate_ir(bool runTest,
                                                                     const std::string &optimizationLevel);
    llvm::Type *generate_type(const ResolvedType &type, bool noOpaque = false);
    llvm::DIType *generate_debug_type(const ResolvedType &type);
    llvm::DIFile *generate_debug_file(const std::string &location);
    void set_debug_location(const SourceLocation &location);
    void unset_debug_location();

    std::string generate_decl_name(const ResolvedDecl &decl);
    llvm::FunctionType *generate_function_type(const ResolvedTypeFunction &fnType);
    llvm::Function *generate_function_decl(const ResolvedFuncDecl &functionDecl);
    void generate_function_body(const ResolvedFuncDecl &functionDecl);
    llvm::AllocaInst *allocate_stack_variable(const SourceLocation &location, const std::string_view identifier,
                                              const ResolvedType &type);
    void generate_block(const ResolvedBlock &block);
    llvm::Value *generate_stmt(const ResolvedStmt &stmt);
    llvm::Value *generate_return_stmt(const ResolvedReturnStmt &stmt);
    llvm::Value *generate_break_stmt(const ResolvedBreakStmt &stmt);
    llvm::Value *generate_continue_stmt(const ResolvedContinueStmt &stmt);
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

    constexpr static const size_t INLINE_SIZE_THRESHOLD = 64;
    bool store_value_generate_memcpy(const ResolvedType &from);
    llvm::Value *store_value(llvm::Value *val, llvm::Value *ptr, const ResolvedType &from, const ResolvedType &to);
    llvm::Value *load_value(llvm::Value *v, const ResolvedType &type);

    llvm::Value *generate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool keepPointer);
    llvm::Value *generate_member_expr(const ResolvedMemberExpr &memberExpr, bool keepPointer);
    llvm::Value *generate_array_at_expr(const ResolvedArrayAtExpr &arrayAtExpr, bool keepPointer);
    llvm::Value *generate_temporary_struct(const ResolvedStructInstantiationExpr &sie);
    llvm::Value *generate_temporary_union(const ResolvedUnionInstantiationExpr &uie);
    llvm::Value *generate_temporary_array(const ResolvedArrayInstantiationExpr &aie);
    llvm::StructType *get_struct_decl(const ResolvedStructDecl &structDecl);
    llvm::StructType *generate_struct_decl(const ResolvedStructDecl &structDecl);
    void generate_struct_fields(const ResolvedStructDecl &structDecl);
    void generate_struct_functions(const ResolvedStructDecl &structDecl);
    llvm::StructType *get_union_decl(const ResolvedUnionDecl &unionDecl);
    llvm::StructType *generate_union_decl(const ResolvedUnionDecl &unionDecl);
    void generate_union_fields(const ResolvedUnionDecl &unionDecl);
    void generate_union_functions(const ResolvedUnionDecl &unionDecl);
    void break_into_bb(llvm::BasicBlock *targetBB);
    void generate_error_no_err();
    void generate_error_group_expr_decl(const ResolvedErrorGroupExprDecl &ErrorGroupExprDecl);
    llvm::Value *generate_error_decl(const ResolvedErrorDecl &errorDecl);
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
    void generate_pending_decls();
    llvm::Value *generate_slice_expr(const ResolvedType &sliceType, const ResolvedExpr &from,
                                     const ResolvedRangeExpr &range);
    void generate_error_trace_push(const SourceLocation &location);
    llvm::Value *generate_error_trace_get_idx();
    void generate_error_trace_clear(llvm::Value *idx = nullptr);
    llvm::Value *generate_builtin_error_trace();

    llvm::Value *generate_comptimeValue(const SourceLocation &location, const ComptimeValue &comptimeValue,
                                        const ResolvedType &type);
    llvm::Value *generate_aggregate_initialization(const SourceLocation &location, const ResolvedType &type,
                                                   std::string_view tmpName,
                                                   const std::vector<std::pair<int, llvm::Value *>> &initializers);

    vec<const ResolvedTestDecl *> cached_tests;
    vec<const ResolvedTestDecl *> get_tests();

    llvm::Value *generate_builtin_function(const ResolvedBuiltinFunctionDecl &builtin, const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_call(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_atomicLoad(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_atomicStore(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_atomicCmpEx(const ResolvedCallExpr &call, bool isWeak);
    llvm::Value *generate_builtin_atomicRmw(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_sizeof(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_typeid(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_typeinfo(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_hasmethod(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_simdsize(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_simdsplat(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_simdiota(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_testnum(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_testname(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_testrun(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_simdLoad(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_simdStore(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_simdSelect(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_simdReduce(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_simdShuffle(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_asm(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_ptrCast(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_intCast(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_floatCast(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_bitCast(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_sqrt(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_abs(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_min(const ResolvedCallExpr &call);
    llvm::Value *generate_builtin_max(const ResolvedCallExpr &call);

    llvm::GlobalVariable *create_global_string(const std::string &str, const std::string &name = "global.str");
};
}  // namespace DMZ