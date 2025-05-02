#include "Codegen.hpp"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Host.h>

namespace C {
Codegen::Codegen(std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree, std::string_view sourcePath)
    : m_resolvedTree(std::move(resolvedTree)), m_builder(m_context), m_module("<translation_unit>", m_context) {
    m_module.setSourceFileName(sourcePath);
    m_module.setTargetTriple(llvm::sys::getDefaultTargetTriple());
}

llvm::Module *Codegen::generate_ir() {
    println("Decl -------------------") for (auto &&decl : m_resolvedTree) {
        if (const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get())) {
            generate_function_decl(*fn);
        }
    }
    println("Body -------------------") for (auto &&decl : m_resolvedTree) {
        if (const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get())) {
            generate_function_body(*fn);
        }
    }

    println("Main wrapper -------------------") generate_main_wrapper();

    return &m_module;
}

llvm::Type *Codegen::generate_type(const Type &type) {
    if (type.kind == Type::Kind::Int) return m_builder.getInt32Ty();

    return m_builder.getVoidTy();
}

void Codegen::generate_function_decl(const ResolvedFunctionDecl &functionDecl) {
    auto *retType = generate_type(functionDecl.type);

    std::vector<llvm::Type *> paramTypes;
    for (auto &&param : functionDecl.params) {
        paramTypes.emplace_back(generate_type(param->type));
    }

    auto *type = llvm::FunctionType::get(retType, paramTypes, false);

    auto *fn = llvm::Function::Create(type, llvm::Function::ExternalLinkage, functionDecl.identifier, m_module);
    fn->setAttributes(construct_attr_list(&functionDecl));
}

llvm::AttributeList Codegen::construct_attr_list(const ResolvedFunctionDecl *fn) {
    // bool isReturningStruct = fn->type.kind == Type::Kind::Struct;
    bool isReturningStruct = false;
    std::vector<llvm::AttributeSet> argsAttrSets;

    if (isReturningStruct) {
        llvm::AttrBuilder retAttrs(m_context);
        retAttrs.addStructRetAttr(generate_type(fn->type));
        argsAttrSets.emplace_back(llvm::AttributeSet::get(m_context, retAttrs));
    }

    for ([[maybe_unused]] auto &&param : fn->params) {
        llvm::AttrBuilder paramAttrs(m_context);
        // if (param->type.kind == Type::Kind::Struct) {
        // if (param->isMutable)
        //     paramAttrs.addByValAttr(generate_type(param->type));
        // else
        // paramAttrs.addAttribute(llvm::Attribute::ReadOnly);
        // }
        argsAttrSets.emplace_back(llvm::AttributeSet::get(m_context, paramAttrs));
    }

    return llvm::AttributeList::get(m_context, llvm::AttributeSet{}, llvm::AttributeSet{}, argsAttrSets);
}

void Codegen::generate_function_body(const ResolvedFunctionDecl &functionDecl) {
    auto *function = m_module.getFunction(functionDecl.identifier);

    auto *entryBB = llvm::BasicBlock::Create(m_context, "entry", function);
    m_builder.SetInsertPoint(entryBB);

    llvm::Value *undef = llvm::UndefValue::get(m_builder.getInt32Ty());
    m_allocaInsertPoint = new llvm::BitCastInst(undef, undef->getType(), "alloca.placeholder", entryBB);

    bool isVoid = functionDecl.type.kind == Type::Kind::Void;
    if (!isVoid) retVal = allocate_stack_variable("retval", functionDecl.type);
    retBB = llvm::BasicBlock::Create(m_context, "return");

    int idx = 0;
    for (auto &&arg : function->args()) {
        const auto *paramDecl = functionDecl.params[idx].get();
        arg.setName(paramDecl->identifier);
        llvm::Value *var = allocate_stack_variable(paramDecl->identifier, paramDecl->type);
        m_builder.CreateStore(&arg, var);

        m_declarations[paramDecl] = var;
        ++idx;
    }

    if (functionDecl.identifier == "println")
        generate_builtin_println_body(functionDecl);
    else
        generate_block(*functionDecl.body);

    if (retBB->hasNPredecessorsOrMore(1)) {
        m_builder.CreateBr(retBB);
        retBB->insertInto(function);
        m_builder.SetInsertPoint(retBB);
    }

    m_allocaInsertPoint->eraseFromParent();
    m_allocaInsertPoint = nullptr;

    if (isVoid) {
        m_builder.CreateRetVoid();
        return;
    }

    m_builder.CreateRet(m_builder.CreateLoad(m_builder.getInt32Ty(), retVal));
}

llvm::AllocaInst *Codegen::allocate_stack_variable(const std::string_view identifier, const Type &type) {
    llvm::IRBuilder<> tmpBuilder(m_context);
    tmpBuilder.SetInsertPoint(m_allocaInsertPoint);

    return tmpBuilder.CreateAlloca(generate_type(type), nullptr, identifier);
}

void Codegen::generate_block(const ResolvedBlock &block) {
    for (auto &&stmt : block.statements) {
        generate_stmt(*stmt);

        if (dynamic_cast<const ResolvedReturnStmt *>(stmt.get())) {
            m_builder.ClearInsertionPoint();
            break;
        }
    }
}

llvm::Value *Codegen::generate_stmt(const ResolvedStmt &stmt) {
    if (auto *expr = dynamic_cast<const ResolvedExpr *>(&stmt)) {
        return generate_expr(*expr);
    }

    if (auto *returnStmt = dynamic_cast<const ResolvedReturnStmt *>(&stmt)) {
        return generate_return_stmt(*returnStmt);
    }

    llvm_unreachable("unknown statement");
}

llvm::Value *Codegen::generate_return_stmt(const ResolvedReturnStmt &stmt) {
    if (stmt.expr) m_builder.CreateStore(generate_expr(*stmt.expr), retVal);

    return m_builder.CreateBr(retBB);
}

llvm::Value *Codegen::generate_expr(const ResolvedExpr &expr) {
    if (auto *number = dynamic_cast<const ResolvedNumberLiteral *>(&expr)) {
        return llvm::ConstantInt::get(m_builder.getInt32Ty(), number->value);
    }

    if (auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(&expr)) {
        return m_builder.CreateLoad(m_builder.getInt32Ty(), m_declarations[&dre->decl]);
    }

    if (auto *call = dynamic_cast<const ResolvedCallExpr *>(&expr)) {
        return generate_call_expr(*call);
    }

    llvm_unreachable("unexpected expression");
}

llvm::Value *Codegen::generate_call_expr(const ResolvedCallExpr &call) {
    llvm::Function *callee = m_module.getFunction(call.callee.identifier);

    std::vector<llvm::Value *> args;
    for (auto &&arg : call.arguments) args.emplace_back(generate_expr(*arg));

    return m_builder.CreateCall(callee, args);
}

void Codegen::generate_builtin_println_body(const ResolvedFunctionDecl &println) {
    auto *type = llvm::FunctionType::get(m_builder.getInt32Ty(), {m_builder.getInt8PtrTy()}, true);
    auto *printf = llvm::Function::Create(type, llvm::Function::ExternalLinkage, "printf", m_module);
    auto *format = m_builder.CreateGlobalStringPtr("%d\n");
    llvm::Value *param = m_builder.CreateLoad(m_builder.getInt32Ty(), m_declarations[println.params[0].get()]);

    m_builder.CreateCall(printf, {format, param});
}

void Codegen::generate_main_wrapper() {
    auto *builtinMain = m_module.getFunction("main");
    builtinMain->setName("__builtin_main");

    auto *main = llvm::Function::Create(llvm::FunctionType::get(m_builder.getInt32Ty(), {}, false),
                                        llvm::Function::ExternalLinkage, "main", m_module);

    auto *entry = llvm::BasicBlock::Create(m_context, "entry", main);
    m_builder.SetInsertPoint(entry);

    m_builder.CreateCall(builtinMain);
    m_builder.CreateRet(llvm::ConstantInt::getSigned(m_builder.getInt32Ty(), 0));
}
}  // namespace C
