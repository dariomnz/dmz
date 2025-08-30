#include "codegen/Codegen.hpp"

namespace DMZ {

llvm::Value *Codegen::generate_expr(const ResolvedExpr &expr, bool keepPointer) {
    debug_func("");
    if (auto val = expr.get_constant_value()) {
        if (expr.type == Type::builtinBool()) {
            return m_builder.getInt1(*val != 0);
        }
        return m_builder.getInt32(*val);
    }
    if (auto *number = dynamic_cast<const ResolvedFloatLiteral *>(&expr)) {
        return llvm::ConstantFP::get(m_builder.getDoubleTy(), number->value);
    }
    if (auto *number = dynamic_cast<const ResolvedIntLiteral *>(&expr)) {
        return m_builder.getInt32(number->value);
    }
    if (auto *number = dynamic_cast<const ResolvedCharLiteral *>(&expr)) {
        return m_builder.getInt8(number->value);
    }
    if (auto *number = dynamic_cast<const ResolvedBoolLiteral *>(&expr)) {
        return m_builder.getInt1(number->value);
    }
    if (auto *str = dynamic_cast<const ResolvedStringLiteral *>(&expr)) {
        return m_builder.CreateGlobalString(str->value, "global.str");
    }
    if (auto *str = dynamic_cast<const ResolvedNullLiteral *>(&expr)) {
        return llvm::Constant::getNullValue(m_builder.getPtrTy());
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
    if (auto *ptrExpr = dynamic_cast<const ResolvedRefPtrExpr *>(&expr)) {
        return generate_ref_ptr_expr(*ptrExpr);
    }
    if (auto *ptrExpr = dynamic_cast<const ResolvedDerefPtrExpr *>(&expr)) {
        return generate_deref_ptr_expr(*ptrExpr, keepPointer);
    }
    if (auto *grouping = dynamic_cast<const ResolvedGroupingExpr *>(&expr)) {
        return generate_expr(*grouping->expr);
    }
    if (auto *me = dynamic_cast<const ResolvedMemberExpr *>(&expr)) {
        return generate_member_expr(*me, keepPointer);
    }
    if (auto *me = dynamic_cast<const ResolvedSelfMemberExpr *>(&expr)) {
        return generate_self_member_expr(*me, keepPointer);
    }
    if (auto *arrayAtExpr = dynamic_cast<const ResolvedArrayAtExpr *>(&expr)) {
        return generate_array_at_expr(*arrayAtExpr, keepPointer);
    }
    if (auto *sie = dynamic_cast<const ResolvedStructInstantiationExpr *>(&expr)) {
        return generate_temporary_struct(*sie);
    }
    if (auto *aie = dynamic_cast<const ResolvedArrayInstantiationExpr *>(&expr)) {
        return generate_temporary_array(*aie);
    }
    if (auto *catchErr = dynamic_cast<const ResolvedCatchErrorExpr *>(&expr)) {
        return generate_catch_error_expr(*catchErr);
    }
    if (auto *tryErr = dynamic_cast<const ResolvedTryErrorExpr *>(&expr)) {
        return generate_try_error_expr(*tryErr);
    }
    if (auto *orelseErr = dynamic_cast<const ResolvedOrElseErrorExpr *>(&expr)) {
        return generate_orelse_error_expr(*orelseErr);
    }
    if (auto *sizeofExpr = dynamic_cast<const ResolvedSizeofExpr *>(&expr)) {
        return generate_sizeof_expr(*sizeofExpr);
    }
    expr.dump();
    dmz_unreachable("unexpected expression");
}

llvm::Value *Codegen::generate_call_expr(const ResolvedCallExpr &call) {
    debug_func("");
    const ResolvedFuncDecl &calleeDecl = call.callee;
    auto symbolName = generate_decl_name(call.callee);
    llvm::Function *callee = m_module->getFunction(symbolName);
    if (!callee) {
        generate_function_decl(call.callee);
        callee = m_module->getFunction(symbolName);
        if (!callee) {
            println("Cannot generate declaration of " << symbolName);
            dmz_unreachable("Cannot generate declaration of func");
        }
    }

    bool isReturningStruct = calleeDecl.type.kind == Type::Kind::Struct || calleeDecl.type.isOptional;
    llvm::Value *retVal = nullptr;
    std::vector<llvm::Value *> args;

    if (isReturningStruct) {
        retVal = args.emplace_back(allocate_stack_variable("struct.ret.tmp", calleeDecl.type));
    }

    size_t argIdx = 0;
    for (auto &&arg : call.arguments) {
        llvm::Value *val = generate_expr(*arg);

        if (arg->type.kind == Type::Kind::Struct && !arg->type.isPointer && calleeDecl.params[argIdx]->isMutable) {
            llvm::Value *tmpVar = allocate_stack_variable("struct.arg.tmp", arg->type);
            store_value(val, tmpVar, arg->type, arg->type);
            val = tmpVar;
        }

        args.emplace_back(val);
        ++argIdx;
    }

    llvm::CallInst *callInst = m_builder.CreateCall(callee, args);
    callInst->setAttributes(construct_attr_list(calleeDecl));

    return isReturningStruct ? retVal : callInst;
}

llvm::Value *Codegen::generate_unary_operator(const ResolvedUnaryOperator &unop) {
    debug_func("");
    llvm::Value *rhs = generate_expr(*unop.operand);

    if (unop.op == TokenType::op_minus) {
        if (unop.operand->type.kind == Type::Kind::Int || unop.operand->type.kind == Type::Kind::UInt)
            return m_builder.CreateNeg(rhs);
        else if (unop.operand->type.kind == Type::Kind::Float)
            return m_builder.CreateFNeg(rhs);
        else
            dmz_unreachable("not expected type in op_minus");
    }

    if (unop.op == TokenType::op_excla_mark) return m_builder.CreateNot(to_bool(rhs, unop.operand->type));

    unop.dump();
    dmz_unreachable("unknown unary op");
    return nullptr;
}

llvm::Value *Codegen::generate_ref_ptr_expr(const ResolvedRefPtrExpr &expr) {
    debug_func("");
    auto v = generate_expr(*expr.expr, true);
    return v;
}

llvm::Value *Codegen::generate_deref_ptr_expr(const ResolvedDerefPtrExpr &expr, bool keepPointer) {
    debug_func("");
    auto v = generate_expr(*expr.expr);
    return keepPointer ? v : load_value(v, expr.type);
}

llvm::Value *Codegen::generate_binary_operator(const ResolvedBinaryOperator &binop) {
    debug_func("");
    TokenType op = binop.op;

    if (op == TokenType::ampamp || op == TokenType::pipepipe) {
        llvm::Function *function = get_current_function();
        bool isOr = op == TokenType::pipepipe;

        auto *rhsTag = isOr ? "or.rhs" : "and.rhs";
        auto *mergeTag = isOr ? "or.merge" : "and.merge";

        auto *rhsBB = llvm::BasicBlock::Create(*m_context, rhsTag, function);
        auto *mergeBB = llvm::BasicBlock::Create(*m_context, mergeTag, function);

        llvm::BasicBlock *trueBB = isOr ? mergeBB : rhsBB;
        llvm::BasicBlock *falseBB = isOr ? rhsBB : mergeBB;
        generate_conditional_operator(*binop.lhs, trueBB, falseBB);

        m_builder.SetInsertPoint(rhsBB);
        llvm::Value *rhs = to_bool(generate_expr(*binop.rhs), binop.rhs->type);

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

        return phi;
    }

    llvm::Value *lhs = generate_expr(*binop.lhs);
    llvm::Value *rhs = generate_expr(*binop.rhs);

    return cast_binary_operator(binop, lhs, rhs);
}

llvm::Value *Codegen::cast_binary_operator(const ResolvedBinaryOperator &binop, llvm::Value *lhs, llvm::Value *rhs) {
    debug_func("");
    rhs = cast_to(rhs, binop.rhs->type, binop.lhs->type);

    if (binop.op == TokenType::op_plus) {
        if (binop.lhs->type.kind == Type::Kind::Int || binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateAdd(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFAdd(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_plus");
    }
    if (binop.op == TokenType::op_minus) {
        if (binop.lhs->type.kind == Type::Kind::Int || binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateSub(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFSub(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_minus");
    }
    if (binop.op == TokenType::asterisk) {
        if (binop.lhs->type.kind == Type::Kind::Int || binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateMul(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFMul(lhs, rhs);
        else
            dmz_unreachable("not expected type in asterisk");
    }
    if (binop.op == TokenType::op_div) {
        if (binop.lhs->type.kind == Type::Kind::Int)
            return m_builder.CreateSDiv(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateUDiv(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFDiv(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_div");
    }
    if (binop.op == TokenType::op_percent) {
        if (binop.lhs->type.kind == Type::Kind::Int)
            return m_builder.CreateSRem(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateURem(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFRem(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_percent");
    }
    if (binop.op == TokenType::op_less) {
        if (binop.lhs->type.kind == Type::Kind::Int)
            return m_builder.CreateICmpSLT(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateICmpULT(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFCmpULT(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_less");
    }
    if (binop.op == TokenType::op_less_eq) {
        if (binop.lhs->type.kind == Type::Kind::Int)
            return m_builder.CreateICmpSLE(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateICmpULE(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFCmpULE(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_less");
    }
    if (binop.op == TokenType::op_more) {
        if (binop.lhs->type.kind == Type::Kind::Int)
            return m_builder.CreateICmpSGT(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateICmpUGT(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFCmpUGT(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_more");
    }
    if (binop.op == TokenType::op_more_eq) {
        if (binop.lhs->type.kind == Type::Kind::Int)
            return m_builder.CreateICmpSGE(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateICmpUGE(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFCmpUGE(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_more_eq");
    }
    if (binop.op == TokenType::op_equal) {
        if (binop.lhs->type.kind == Type::Kind::Int || binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateICmpEQ(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFCmpUEQ(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_equal");
    }
    if (binop.op == TokenType::op_not_equal) {
        if (binop.lhs->type.kind == Type::Kind::Int || binop.lhs->type.kind == Type::Kind::UInt)
            return m_builder.CreateICmpNE(lhs, rhs);
        else if (binop.lhs->type.kind == Type::Kind::Float)
            return m_builder.CreateFCmpUNE(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_not_equal");
    }

    binop.dump();
    dmz_unreachable("unexpected binary operator");
    return nullptr;
}

void Codegen::generate_conditional_operator(const ResolvedExpr &op, llvm::BasicBlock *trueBB,
                                            llvm::BasicBlock *falseBB) {
    debug_func("");
    llvm::Function *function = get_current_function();
    const auto *binop = dynamic_cast<const ResolvedBinaryOperator *>(&op);

    if (binop && binop->op == TokenType::pipepipe) {
        llvm::BasicBlock *nextBB = llvm::BasicBlock::Create(*m_context, "or.lhs.false", function);
        generate_conditional_operator(*binop->lhs, trueBB, nextBB);

        m_builder.SetInsertPoint(nextBB);
        generate_conditional_operator(*binop->rhs, trueBB, falseBB);
        return;
    }

    if (binop && binop->op == TokenType::ampamp) {
        llvm::BasicBlock *nextBB = llvm::BasicBlock::Create(*m_context, "and.lhs.true", function);
        generate_conditional_operator(*binop->lhs, nextBB, falseBB);

        m_builder.SetInsertPoint(nextBB);
        generate_conditional_operator(*binop->rhs, trueBB, falseBB);
        return;
    }

    llvm::Value *val = to_bool(generate_expr(op), op.type);
    m_builder.CreateCondBr(val, trueBB, falseBB);
}

llvm::Value *Codegen::generate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool keepPointer) {
    debug_func("");
    const ResolvedDecl &decl = dre.decl;
    llvm::Value *val = m_declarations[&decl];

    keepPointer |= dynamic_cast<const ResolvedParamDecl *>(&decl) && !decl.isMutable;
    keepPointer |= dre.type.kind == Type::Kind::Struct;
    keepPointer |= dre.type.isArray.has_value();

    return keepPointer ? val : load_value(val, dre.type);
}

llvm::Value *Codegen::generate_member_expr(const ResolvedMemberExpr &memberExpr, bool keepPointer) {
    debug_func("");
    if (auto member = dynamic_cast<const ResolvedFieldDecl *>(&memberExpr.member)) {
        llvm::Value *base = generate_expr(*memberExpr.base, true);
        llvm::Value *field =
            m_builder.CreateStructGEP(generate_type(memberExpr.base->type.remove_pointer()), base, member->index);
        return keepPointer ? field : load_value(field, member->type);
    } else if (auto errDecl = dynamic_cast<const ResolvedErrorDecl *>(&memberExpr.member)) {
        return m_declarations[errDecl];
    } else {
        memberExpr.member.dump();
        dmz_unreachable("Unexpected member expresion");
    }
    return nullptr;
}

llvm::Value *Codegen::generate_self_member_expr(const ResolvedSelfMemberExpr &memberExpr, bool keepPointer) {
    debug_func("");
    if (auto member = dynamic_cast<const ResolvedFieldDecl *>(&memberExpr.member)) {
        llvm::Value *base = generate_expr(*memberExpr.base, true);
        llvm::Value *field =
            m_builder.CreateStructGEP(generate_type(memberExpr.base->type.remove_pointer()), base, member->index);
        return keepPointer ? field : load_value(field, member->type);
    } else if (auto errDecl = dynamic_cast<const ResolvedErrorDecl *>(&memberExpr.member)) {
        return m_declarations[errDecl];
    } else {
        memberExpr.member.dump();
        dmz_unreachable("Unexpected member expresion");
    }
    return nullptr;
}

llvm::Value *Codegen::generate_array_at_expr(const ResolvedArrayAtExpr &arrayAtExpr, bool keepPointer) {
    debug_func("");
    llvm::Value *base = generate_expr(*arrayAtExpr.array, !arrayAtExpr.array->type.isPointer);
    llvm::Type *type = nullptr;
    std::vector<llvm::Value *> idxs;
    if (arrayAtExpr.array->type.isPointer) {
        type = generate_type(arrayAtExpr.type);
        idxs = {generate_expr(*arrayAtExpr.index)};
    } else {
        type = generate_type(arrayAtExpr.array->type);
        idxs = {m_builder.getInt32(0), generate_expr(*arrayAtExpr.index)};
    }
    llvm::Value *field = m_builder.CreateGEP(type, base, idxs);

    return keepPointer ? field : load_value(field, arrayAtExpr.type);
}

llvm::Value *Codegen::generate_temporary_struct(const ResolvedStructInstantiationExpr &sie) {
    debug_func("");
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
        store_value(initializerVals[field.get()], dst, field->type, field->type);
    }

    return tmp;
}

llvm::Value *Codegen::generate_temporary_array(const ResolvedArrayInstantiationExpr &aie) {
    debug_func("");
    Type arrayType = aie.type;
    std::string varName = "array." + std::string(arrayType.name) + ".tmp";
    llvm::Value *tmp = allocate_stack_variable(varName, arrayType);

    size_t idx = 0;
    for (auto &&initExpr : aie.initializers) {
        auto var = generate_expr(*initExpr);
        llvm::Value *dst =
            m_builder.CreateGEP(generate_type(arrayType), tmp, {m_builder.getInt32(0), m_builder.getInt32(idx++)});
        store_value(var, dst, arrayType.withoutArray(), arrayType.withoutArray());
    }

    return tmp;
}

llvm::Value *Codegen::generate_catch_error_expr(const ResolvedCatchErrorExpr &catchErrorExpr) {
    debug_func("");
    llvm::Value *error_struct = nullptr;
    llvm::Value *decl_value = nullptr;
    Type error_type;
    if (catchErrorExpr.errorToCatch) {
        error_struct = generate_expr(*catchErrorExpr.errorToCatch, true);
        error_type = catchErrorExpr.errorToCatch->type;
    } else if (catchErrorExpr.declaration) {
        error_struct = generate_expr(*catchErrorExpr.declaration->varDecl->initializer, true);

        decl_value = allocate_stack_variable(catchErrorExpr.declaration->varDecl->identifier,
                                             catchErrorExpr.declaration->varDecl->type);

        m_declarations[catchErrorExpr.declaration->varDecl.get()] = decl_value;

        error_type = catchErrorExpr.declaration->varDecl->initializer->type;
    } else {
        dmz_unreachable("malformed ResolvedCatchErrorExpr");
    }
    llvm::Value *error_value_ptr = m_builder.CreateStructGEP(generate_type(error_type), error_struct, 1);
    llvm::Value *error_value = load_value(error_value_ptr, Type::builtinError("err"));
    llvm::Value *error_value_bool = to_bool(error_value, Type::builtinError("err"));
    if (catchErrorExpr.declaration) {
        llvm::Value *selectedError = m_builder.CreateSelect(error_value_bool, error_value, m_success, "select.err");
        store_value(selectedError, decl_value, Type::builtinError("err"), Type::builtinError("err"));
    }

    return m_builder.CreateSelect(error_value_bool, m_builder.getInt1(true), m_builder.getInt1(false), "catch.result");
}

llvm::Value *Codegen::generate_try_error_expr(const ResolvedTryErrorExpr &tryErrorExpr) {
    debug_func("");
    llvm::Function *function = get_current_function();

    auto *trueBB = llvm::BasicBlock::Create(*m_context, "if.true.try");
    auto *exitBB = llvm::BasicBlock::Create(*m_context, "if.exit.try");

    llvm::BasicBlock *elseBB = exitBB;

    llvm::Value *error_struct = generate_expr(*tryErrorExpr.errorToTry, true);

    llvm::Value *error_value_ptr =
        m_builder.CreateStructGEP(generate_type(tryErrorExpr.errorToTry->type), error_struct, 1);
    llvm::Value *error_value = load_value(error_value_ptr, Type::builtinError("err"));

    m_builder.CreateCondBr(to_bool(error_value, Type::builtinError("err")), trueBB, elseBB);

    trueBB->insertInto(function);
    m_builder.SetInsertPoint(trueBB);
    for (auto &&d : tryErrorExpr.defers) {
        generate_block(*d->resolvedDefer.block);
    }

    if (m_currentFunction->type.isOptional) {
        llvm::Value *dst = m_builder.CreateStructGEP(generate_type(m_currentFunction->type), retVal, 1);
        store_value(error_value, dst, Type::builtinError("err"), Type::builtinError("err"));

        assert(retBB && "function with return stmt doesn't have a return block");
        break_into_bb(retBB);
    } else {
        auto fmt = m_builder.CreateGlobalString(
            tryErrorExpr.location.to_string() + ": Aborted: Try catch an error value of '%s' in the function '" +
            std::string(m_currentFunction->identifier) + "' that not return an optional\n");
        auto printf_func = m_module->getOrInsertFunction(
            "printf", llvm::FunctionType::get(m_builder.getInt32Ty(), m_builder.getInt8Ty()->getPointerTo(), true));
        m_builder.CreateCall(printf_func, {fmt, error_value});
        llvm::Function *trapIntrinsic = llvm::Intrinsic::getDeclaration(m_module.get(), llvm::Intrinsic::trap);
        m_builder.CreateCall(trapIntrinsic, {});
    }
    break_into_bb(exitBB);

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);

    if (tryErrorExpr.type == Type::builtinVoid()) return nullptr;

    auto tryValue = m_builder.CreateStructGEP(generate_type(tryErrorExpr.errorToTry->type), error_struct, 0);
    return load_value(tryValue, tryErrorExpr.type);
}

llvm::Value *Codegen::generate_orelse_error_expr(const ResolvedOrElseErrorExpr &orelseErrorExpr) {
    debug_func("");
    llvm::Function *function = get_current_function();

    auto *trueBB = llvm::BasicBlock::Create(*m_context, "if.true.orelse");
    auto *exitBB = llvm::BasicBlock::Create(*m_context, "if.exit.orelse");

    llvm::BasicBlock *elseBB = exitBB;

    llvm::Value *error_struct = generate_expr(*orelseErrorExpr.errorToOrElse, true);

    llvm::Value *error_value_ptr =
        m_builder.CreateStructGEP(generate_type(orelseErrorExpr.errorToOrElse->type), error_struct, 1);
    llvm::Value *error_value = load_value(error_value_ptr, Type::builtinError("err"));

    llvm::Value *return_value = allocate_stack_variable("tmp.orelse", orelseErrorExpr.type);

    auto error_expr_value_ptr =
        m_builder.CreateStructGEP(generate_type(orelseErrorExpr.errorToOrElse->type), error_struct, 0);
    auto error_expr_value = load_value(error_expr_value_ptr, orelseErrorExpr.errorToOrElse->type.withoutOptional());
    store_value(error_expr_value, return_value, orelseErrorExpr.errorToOrElse->type.withoutOptional(),
                orelseErrorExpr.type);

    llvm::Value *error_value_bool = to_bool(error_value, Type::builtinError("err"));
    m_builder.CreateCondBr(error_value_bool, trueBB, elseBB);

    trueBB->insertInto(function);
    m_builder.SetInsertPoint(trueBB);

    llvm::Value *orelse_value = generate_expr(*orelseErrorExpr.orElseExpr, false);

    store_value(orelse_value, return_value, orelseErrorExpr.orElseExpr->type, orelseErrorExpr.type);
    break_into_bb(exitBB);

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);
    return load_value(return_value, orelseErrorExpr.type);
}

llvm::Value *Codegen::generate_sizeof_expr(const ResolvedSizeofExpr &sizeofExpr) {
    auto type = generate_type(sizeofExpr.sizeofType);
    auto size = llvm::ConstantExpr::getSizeOf(type);
    return size;
}
}  // namespace DMZ
