#include "Codegen.hpp"

namespace C {
Codegen::Codegen(std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree, std::string_view sourcePath)
    : m_resolvedTree(std::move(resolvedTree)), m_builder(m_context), m_module("<translation_unit>", m_context) {
    m_module.setSourceFileName(sourcePath);
    m_module.setTargetTriple(llvm::sys::getDefaultTargetTriple());
}

llvm::Module *Codegen::generate_ir() {
    for (auto &&decl : m_resolvedTree) {
        if (const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get()))
            generate_function_decl(*fn);
        else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get()))
            generate_struct_decl(*sd);
        else
            llvm_unreachable("unexpected top level declaration");
    }

    for (auto &&decl : m_resolvedTree) {
        if (const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get()))
            generate_function_body(*fn);
        else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get()))
            generate_struct_definition(*sd);
        else
            llvm_unreachable("unexpected top level declaration");
    }

    generate_main_wrapper();

    return &m_module;
}

llvm::Type *Codegen::generate_type(const Type &type) {
    if (type.kind == Type::Kind::Int) {
        return m_builder.getInt32Ty();
    }

    if (type.kind == Type::Kind::Struct) {
        return llvm::StructType::getTypeByName(m_context, "struct." + type.name);
    }

    return m_builder.getVoidTy();
}

void Codegen::generate_function_decl(const ResolvedFunctionDecl &functionDecl) {
    llvm::Type *retType = generate_type(functionDecl.type);
    std::vector<llvm::Type *> paramTypes;

    if (functionDecl.type.kind == Type::Kind::Struct) {
        paramTypes.emplace_back(llvm::PointerType::get(retType, 0));
        retType = m_builder.getVoidTy();
    }

    for (auto &&param : functionDecl.params) {
        llvm::Type *paramType = generate_type(param->type);
        if (param->type.kind == Type::Kind::Struct) paramType = llvm::PointerType::get(paramType, 0);
        paramTypes.emplace_back(paramType);
    }

    auto *type = llvm::FunctionType::get(retType, paramTypes, false);
    auto *fn = llvm::Function::Create(type, llvm::Function::ExternalLinkage, functionDecl.identifier, m_module);
    fn->setAttributes(construct_attr_list(functionDecl));
}

llvm::AttributeList Codegen::construct_attr_list(const ResolvedFunctionDecl &fn) {
    bool isReturningStruct = fn.type.kind == Type::Kind::Struct;
    std::vector<llvm::AttributeSet> argsAttrSets;

    if (isReturningStruct) {
        llvm::AttrBuilder retAttrs(m_context);
        retAttrs.addStructRetAttr(generate_type(fn.type));
        argsAttrSets.emplace_back(llvm::AttributeSet::get(m_context, retAttrs));
    }

    for ([[maybe_unused]] auto &&param : fn.params) {
        llvm::AttrBuilder paramAttrs(m_context);
        if (param->type.kind == Type::Kind::Struct) {
            if (param->isMutable)
                paramAttrs.addByValAttr(generate_type(param->type));
            else
                paramAttrs.addAttribute(llvm::Attribute::ReadOnly);
        }
        argsAttrSets.emplace_back(llvm::AttributeSet::get(m_context, paramAttrs));
    }

    return llvm::AttributeList::get(m_context, llvm::AttributeSet{}, llvm::AttributeSet{}, argsAttrSets);
}

void Codegen::generate_function_body(const ResolvedFunctionDecl &functionDecl) {
    auto *function = m_module.getFunction(functionDecl.identifier);

    auto *entryBB = llvm::BasicBlock::Create(m_context, "entry", function);
    m_builder.SetInsertPoint(entryBB);

    // Note: llvm:Instruction has a protected destructor.
    llvm::Value *undef = llvm::UndefValue::get(m_builder.getInt32Ty());
    m_allocaInsertPoint = new llvm::BitCastInst(undef, undef->getType(), "alloca.placeholder", entryBB);

    bool returnsVoid = functionDecl.type.kind != Type::Kind::Int;
    if (!returnsVoid) retVal = allocate_stack_variable("retval", functionDecl.type);
    retBB = llvm::BasicBlock::Create(m_context, "return");

    int idx = 0;
    for (auto &&arg : function->args()) {
        if (arg.hasStructRetAttr()) {
            arg.setName("ret");
            retVal = &arg;
            continue;
        }

        const auto *paramDecl = functionDecl.params[idx].get();
        arg.setName(paramDecl->identifier);

        llvm::Value *declVal = &arg;
        if (paramDecl->type.kind != Type::Kind::Struct && paramDecl->isMutable) {
            declVal = allocate_stack_variable(paramDecl->identifier, paramDecl->type);
            store_value(&arg, declVal, paramDecl->type);
        }

        m_declarations[paramDecl] = declVal;
        ++idx;
    }

    if (functionDecl.identifier == "println")
        generate_builtin_println_body(functionDecl);
    else
        generate_block(*functionDecl.body);

    if (retBB->hasNPredecessorsOrMore(1)) {
        break_into_bb(retBB);
        retBB->insertInto(function);
        m_builder.SetInsertPoint(retBB);
    }

    m_allocaInsertPoint->eraseFromParent();
    m_allocaInsertPoint = nullptr;

    if (returnsVoid) {
        m_builder.CreateRetVoid();
        return;
    }

    m_builder.CreateRet(load_value(retVal, functionDecl.type));
}

llvm::AllocaInst *Codegen::allocate_stack_variable(const std::string_view identifier, const Type &type) {
    llvm::IRBuilder<> tmpBuilder(m_context);
    tmpBuilder.SetInsertPoint(m_allocaInsertPoint);

    return tmpBuilder.CreateAlloca(generate_type(type), nullptr, identifier);
}

void Codegen::generate_block(const ResolvedBlock &block) {
    for (auto &stmt : block.statements) {
        generate_stmt(*stmt);

        // We exited the current basic block for some reason, so there is
        // no need for generating the remaining instructions.
        if (!m_builder.GetInsertBlock()) break;
    }
}

llvm::Value *Codegen::generate_stmt(const ResolvedStmt &stmt) {
    if (auto *expr = dynamic_cast<const ResolvedExpr *>(&stmt)) {
        return generate_expr(*expr);
    }
    if (auto *returnStmt = dynamic_cast<const ResolvedReturnStmt *>(&stmt)) {
        return generate_return_stmt(*returnStmt);
    }
    if (auto *ifStmt = dynamic_cast<const ResolvedIfStmt *>(&stmt)) {
        return generate_if_stmt(*ifStmt);
    }
    if (auto *whileStmt = dynamic_cast<const ResolvedWhileStmt *>(&stmt)) {
        return generate_while_stmt(*whileStmt);
    }
    if (auto *declStmt = dynamic_cast<const ResolvedDeclStmt *>(&stmt)) {
        return generate_decl_stmt(*declStmt);
    }
    if (auto *assignment = dynamic_cast<const ResolvedAssignment *>(&stmt)) {
        return generate_assignment(*assignment);
    }
    llvm_unreachable("unknown statement");
}

llvm::Value *Codegen::generate_return_stmt(const ResolvedReturnStmt &stmt) {
    if (stmt.expr) store_value(generate_expr(*stmt.expr), retVal, stmt.expr->type);

    assert(retBB && "function with return stmt doesn't have a return block");
    break_into_bb(retBB);
    return nullptr;
}

llvm::Value *Codegen::generate_expr(const ResolvedExpr &expr, bool keepPointer) {
    if (auto val = expr.get_constant_value()) {
        return llvm::ConstantInt::get(m_builder.getInt32Ty(), *val);
    }
    if (auto *number = dynamic_cast<const ResolvedNumberLiteral *>(&expr)) {
        return llvm::ConstantInt::get(m_builder.getInt32Ty(), number->value);
    }
    if (auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(&expr)) {
        return generate_decl_ref_expr(*dre, keepPointer);
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
    if (auto *me = dynamic_cast<const ResolvedMemberExpr *>(&expr)) {
        return generate_member_expr(*me, keepPointer);
    }
    if (auto *sie = dynamic_cast<const ResolvedStructInstantiationExpr *>(&expr)) {
        return generate_temporary_struct(*sie);
    }
    llvm_unreachable("unexpected expression");
}

llvm::Value *Codegen::generate_call_expr(const ResolvedCallExpr &call) {
    const ResolvedFunctionDecl &calleeDecl = call.callee;
    llvm::Function *callee = m_module.getFunction(calleeDecl.identifier);

    bool isReturningStruct = calleeDecl.type.kind == Type::Kind::Struct;
    llvm::Value *retVal = nullptr;
    std::vector<llvm::Value *> args;

    if (isReturningStruct) retVal = args.emplace_back(allocate_stack_variable("struct.ret.tmp", calleeDecl.type));

    size_t argIdx = 0;
    for (auto &&arg : call.arguments) {
        llvm::Value *val = generate_expr(*arg);

        if (arg->type.kind == Type::Kind::Struct && calleeDecl.params[argIdx]->isMutable) {
            llvm::Value *tmpVar = allocate_stack_variable("struct.arg.tmp", arg->type);
            store_value(val, tmpVar, arg->type);
            val = tmpVar;
        }

        args.emplace_back(val);
        ++argIdx;
    }

    llvm::CallInst *callInst = m_builder.CreateCall(callee, args);
    callInst->setAttributes(construct_attr_list(calleeDecl));

    return isReturningStruct ? retVal : callInst;
}

void Codegen::generate_builtin_println_body(const ResolvedFunctionDecl &println) {
    auto *type = llvm::FunctionType::get(m_builder.getInt32Ty(), {m_builder.getInt8Ty()->getPointerTo()}, true);
    auto *printf = llvm::Function::Create(type, llvm::Function::ExternalLinkage, "printf", m_module);
    auto *format = m_builder.CreateGlobalStringPtr("%d\n");

    llvm::Value *param = m_declarations[println.params[0].get()];

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
    if (op == TokenType::op_not_equal) return bool_to_int(m_builder.CreateICmpNE(lhs, rhs));

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
}

llvm::Value *Codegen::generate_if_stmt(const ResolvedIfStmt &stmt) {
    llvm::Function *function = get_current_function();

    auto *trueBB = llvm::BasicBlock::Create(m_context, "if.true");
    auto *exitBB = llvm::BasicBlock::Create(m_context, "if.exit");

    llvm::BasicBlock *elseBB = exitBB;
    if (stmt.falseBlock) elseBB = llvm::BasicBlock::Create(m_context, "if.false");

    llvm::Value *cond = generate_expr(*stmt.condition);
    m_builder.CreateCondBr(int_to_bool(cond), trueBB, elseBB);

    trueBB->insertInto(function);
    m_builder.SetInsertPoint(trueBB);
    generate_block(*stmt.trueBlock);
    break_into_bb(exitBB);

    if (stmt.falseBlock) {
        elseBB->insertInto(function);
        m_builder.SetInsertPoint(elseBB);
        generate_block(*stmt.falseBlock);
        break_into_bb(exitBB);
    }

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);
    return nullptr;
}

llvm::Value *Codegen::generate_while_stmt(const ResolvedWhileStmt &stmt) {
    llvm::Function *function = get_current_function();

    auto *header = llvm::BasicBlock::Create(m_context, "while.cond", function);
    auto *body = llvm::BasicBlock::Create(m_context, "while.body", function);
    auto *exit = llvm::BasicBlock::Create(m_context, "while.exit", function);

    m_builder.CreateBr(header);

    m_builder.SetInsertPoint(header);
    llvm::Value *cond = generate_expr(*stmt.condition);
    m_builder.CreateCondBr(int_to_bool(cond), body, exit);

    m_builder.SetInsertPoint(body);
    generate_block(*stmt.body);
    break_into_bb(header);

    m_builder.SetInsertPoint(exit);
    return nullptr;
}

llvm::Value *Codegen::generate_decl_stmt(const ResolvedDeclStmt &stmt) {
    const auto *decl = stmt.varDecl.get();
    llvm::AllocaInst *var = allocate_stack_variable(decl->identifier, decl->type);

    if (const auto &init = decl->initializer) {
        store_value(generate_expr(*init), var, init->type);
    }

    m_declarations[decl] = var;
    return nullptr;
}

void Codegen::break_into_bb(llvm::BasicBlock *targetBB) {
    llvm::BasicBlock *currentBB = m_builder.GetInsertBlock();

    if (currentBB && !currentBB->getTerminator()) m_builder.CreateBr(targetBB);

    m_builder.ClearInsertionPoint();
}

llvm::Value *Codegen::generate_assignment(const ResolvedAssignment &stmt) {
    llvm::Value *val = generate_expr(*stmt.expr);
    return store_value(val, generate_expr(*stmt.assignee, true), stmt.assignee->type);
}

llvm::Value *Codegen::store_value(llvm::Value *val, llvm::Value *ptr, const Type &type) {
    if (type.kind != Type::Kind::Struct) return m_builder.CreateStore(val, ptr);

    const llvm::DataLayout &dl = m_module.getDataLayout();
    const llvm::StructLayout *sl = dl.getStructLayout(static_cast<llvm::StructType *>(generate_type(type)));

    return m_builder.CreateMemCpy(ptr, sl->getAlignment(), val, sl->getAlignment(), sl->getSizeInBytes());
}

llvm::Value *Codegen::load_value(llvm::Value *v, const Type &type) {
    if (type.kind == Type::Kind::Int) {
        return m_builder.CreateLoad(m_builder.getInt32Ty(), v);
    }

    return v;
}

llvm::Value *Codegen::generate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool keepPointer) {
    const ResolvedDecl &decl = dre.decl;
    llvm::Value *val = m_declarations[&decl];

    keepPointer |= dynamic_cast<const ResolvedParamDecl *>(&decl) && !decl.isMutable;
    keepPointer |= dre.type.kind == Type::Kind::Struct;

    return keepPointer ? val : load_value(val, dre.type);
}

llvm::Value *Codegen::generate_member_expr(const ResolvedMemberExpr &memberExpr, bool keepPointer) {
    llvm::Value *base = generate_expr(*memberExpr.base, true);
    llvm::Value *field = m_builder.CreateStructGEP(generate_type(memberExpr.base->type), base, memberExpr.field.index);

    return keepPointer ? field : load_value(field, memberExpr.field.type);
}

llvm::Value *Codegen::generate_temporary_struct(const ResolvedStructInstantiationExpr &sie) {
    Type structType = sie.type;
    std::string varName(structType.name);
    varName += ".tmp";
    llvm::Value *tmp = allocate_stack_variable(varName, structType);

    std::map<const ResolvedFieldDecl *, llvm::Value *> initializerVals;
    for (auto &&initStmt : sie.fieldInitializers)
        initializerVals[&initStmt->field] = generate_expr(*initStmt->initializer);

    size_t idx = 0;
    for (auto &&field : sie.structDecl.fields) {
        llvm::Value *dst = m_builder.CreateStructGEP(generate_type(structType), tmp, idx++);
        store_value(initializerVals[field.get()], dst, field->type);
    }

    return tmp;
}

void Codegen::generate_struct_decl(const ResolvedStructDecl &structDecl) {
    std::string structName("struct.");
    structName += structDecl.identifier;
    llvm::StructType::create(m_context, structName);
}

void Codegen::generate_struct_definition(const ResolvedStructDecl &structDecl) {
    auto *type = static_cast<llvm::StructType *>(generate_type(structDecl.type));

    std::vector<llvm::Type *> fieldTypes;
    for (auto &&field : structDecl.fields) {
        llvm::Type *t = generate_type(field->type);
        fieldTypes.emplace_back(t);
    }

    type->setBody(fieldTypes);
}
}  // namespace C
