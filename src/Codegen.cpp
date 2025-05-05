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
    for (auto &&decl : m_resolvedTree) {
        if (const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get())) {
            generate_function_decl(*fn);
        }
    }
    for (auto &&decl : m_resolvedTree) {
        if (const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get())) {
            generate_function_body(*fn);
        }
    }

    generate_main_wrapper();

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

    if (auto *binop = dynamic_cast<const ResolvedBinaryOperator *>(&expr)) {
        return generate_binary_operator(*binop);
    }

    if (auto *unop = dynamic_cast<const ResolvedUnaryOperator *>(&expr)) {
        return generate_unary_operator(*unop);
    }

    if (auto *grouping = dynamic_cast<const ResolvedGroupingExpr *>(&expr)) {
        return generate_expr(*grouping->expr);
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

llvm::Value *Codegen::generate_unary_operator(const ResolvedUnaryOperator &unop) {
    llvm::Value *rhs = generate_expr(*unop.operand);

    if (unop.op == TokenType::op_minus) return m_builder.CreateNeg(rhs);

    if (unop.op == TokenType::op_not) return bool_to_int(m_builder.CreateNot(int_to_bool(rhs)));

    llvm_unreachable("unknown unary op");
    return nullptr;
}

llvm::Value *Codegen::generate_binary_operator(const ResolvedBinaryOperator &binop) {
    TokenType op = binop.op;

    if (op == TokenType::op_and || op == TokenType::op_or) {
        llvm::Function *function = get_current_function();
        bool isOr = op == TokenType::op_or;

        auto *rhsTag = isOr ? "or.rhs" : "and.rhs";
        auto *mergeTag = isOr ? "or.merge" : "and.merge";

        auto *rhsBB = llvm::BasicBlock::Create(m_context, rhsTag, function);
        auto *mergeBB = llvm::BasicBlock::Create(m_context, mergeTag, function);

        llvm::BasicBlock *trueBB = isOr ? mergeBB : rhsBB;
        llvm::BasicBlock *falseBB = isOr ? rhsBB : mergeBB;
        generate_conditional_operator(*binop.lhs, trueBB, falseBB);

        m_builder.SetInsertPoint(rhsBB);
        llvm::Value *rhs = int_to_bool(generate_expr(*binop.rhs));

        assert(!m_builder.GetInsertBlock()->getTerminator() && "a binop terminated the current block");
        m_builder.CreateBr(mergeBB);

        rhsBB = m_builder.GetInsertBlock();
        m_builder.SetInsertPoint(mergeBB);
        llvm::PHINode *phi = m_builder.CreatePHI(m_builder.getInt1Ty(), 2);

        for (auto it = pred_begin(mergeBB); it != pred_end(mergeBB); ++it) {
            if (*it == rhsBB)
                phi->addIncoming(rhs, rhsBB);
            else
                phi->addIncoming(m_builder.getInt1(isOr), *it);
        }

        return bool_to_int(phi);
    }

    llvm::Value *lhs = generate_expr(*binop.lhs);
    llvm::Value *rhs = generate_expr(*binop.rhs);

    if (op == TokenType::op_plus) return m_builder.CreateAdd(lhs, rhs);

    if (op == TokenType::op_minus) return m_builder.CreateSub(lhs, rhs);

    if (op == TokenType::op_mult) return m_builder.CreateMul(lhs, rhs);

    if (op == TokenType::op_div) return m_builder.CreateUDiv(lhs, rhs);

    if (op == TokenType::op_less) return bool_to_int(m_builder.CreateICmpSLT(lhs, rhs));
    if (op == TokenType::op_less_eq) return bool_to_int(m_builder.CreateICmpSLE(lhs, rhs));

    if (op == TokenType::op_more) return bool_to_int(m_builder.CreateICmpSGT(lhs, rhs));
    if (op == TokenType::op_more_eq) return bool_to_int(m_builder.CreateICmpSGE(lhs, rhs));

    if (op == TokenType::op_equal) return bool_to_int(m_builder.CreateICmpEQ(lhs, rhs));

    llvm_unreachable("unexpected binary operator");
    return nullptr;
}

llvm::Value *Codegen::int_to_bool(llvm::Value *v) {
    return m_builder.CreateICmpNE(v, llvm::ConstantInt::get(m_builder.getInt32Ty(), 0), "int.to.bool");
}

llvm::Value *Codegen::bool_to_int(llvm::Value *v) { return m_builder.CreateIntCast(v, m_builder.getInt32Ty(), false); }

llvm::Function *Codegen::get_current_function() { return m_builder.GetInsertBlock()->getParent(); };

void Codegen::generate_conditional_operator(const ResolvedExpr &op, llvm::BasicBlock *trueBB,
                                            llvm::BasicBlock *falseBB) {
    llvm::Function *function = get_current_function();
    const auto *binop = dynamic_cast<const ResolvedBinaryOperator *>(&op);

    if (binop && binop->op == TokenType::op_or) {
        llvm::BasicBlock *nextBB = llvm::BasicBlock::Create(m_context, "or.lhs.false", function);
        generate_conditional_operator(*binop->lhs, trueBB, nextBB);

        m_builder.SetInsertPoint(nextBB);
        generate_conditional_operator(*binop->rhs, trueBB, falseBB);
        return;
    }

    if (binop && binop->op == TokenType::op_and) {
        llvm::BasicBlock *nextBB = llvm::BasicBlock::Create(m_context, "and.lhs.true", function);
        generate_conditional_operator(*binop->lhs, nextBB, falseBB);

        m_builder.SetInsertPoint(nextBB);
        generate_conditional_operator(*binop->rhs, trueBB, falseBB);
        return;
    }

    llvm::Value *val = int_to_bool(generate_expr(op));
    m_builder.CreateCondBr(val, trueBB, falseBB);
};
}  // namespace C
