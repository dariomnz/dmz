#include "codegen/Codegen.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Host.h>
#pragma GCC diagnostic pop

namespace DMZ {

llvm::Value *Codegen::generate_expr(const ResolvedExpr &expr, bool keepPointer) {
    if (auto val = expr.get_constant_value()) {
        if (std::holds_alternative<int>(*val)) {
            return m_builder.getInt32(std::get<int>(*val));
        } else if (std::holds_alternative<char>(*val)) {
            return m_builder.getInt8(std::get<char>(*val));
        } else if (std::holds_alternative<bool>(*val)) {
            return m_builder.getInt1(std::get<bool>(*val));
        }
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
        return m_builder.CreateGlobalStringPtr(str->value, "global.str");
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
    if (auto *err = dynamic_cast<const ResolvedErrDeclRefExpr *>(&expr)) {
        return generate_err_decl_ref_expr(*err);
    }
    if (auto *errUnwrap = dynamic_cast<const ResolvedErrUnwrapExpr *>(&expr)) {
        return generate_err_unwrap_expr(*errUnwrap);
    }
    if (auto *catchErr = dynamic_cast<const ResolvedCatchErrExpr *>(&expr)) {
        return generate_catch_err_expr(*catchErr);
    }
    if (auto *tryErr = dynamic_cast<const ResolvedTryErrExpr *>(&expr)) {
        return generate_try_err_expr(*tryErr);
    }
    if (auto *modDeclRef = dynamic_cast<const ResolvedModuleDeclRefExpr *>(&expr)) {
        return generate_expr(*modDeclRef->expr);
    }
    expr.dump();
    dmz_unreachable("unexpected expression");
}

llvm::Value *Codegen::generate_call_expr(const ResolvedCallExpr &call) {
    const ResolvedFuncDecl &calleeDecl = call.callee;
    std::string modIdentifier = dynamic_cast<const ResolvedExternFunctionDecl *>(&calleeDecl)
                                    ? std::string(calleeDecl.identifier)
                                    : calleeDecl.modIdentifier;
    auto symbolName = generate_symbol_name(modIdentifier);
    llvm::Function *callee = m_module.getFunction(generate_symbol_name(symbolName));
    if (!callee) {
        // println("symbolName " << symbolName);
        // call.dump();
        // println(&calleeDecl << " " << calleeDecl.identifier);
        // println(&calleeDecl << " " << calleeDecl.params.size());
        // println(&calleeDecl << " " << (void *)calleeDecl.params[0].get());

        // println(&calleeDecl << " " << calleeDecl.params[0]->identifier);

        // (&calleeDecl)->dump();
        generate_function_decl(calleeDecl);
        callee = m_module.getFunction(symbolName);
        // println("Cannot find " << symbolName);
        assert(callee && "Cannot generate declaration of extern function");
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

        if (arg->type.kind == Type::Kind::Struct && calleeDecl.params[argIdx]->isMutable && !arg->type.isRef) {
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

llvm::Value *Codegen::generate_unary_operator(const ResolvedUnaryOperator &unop) {
    bool keepPointer = unop.op == TokenType::amp;
    llvm::Value *rhs = generate_expr(*unop.operand, keepPointer);

    if (unop.op == TokenType::op_minus) return m_builder.CreateNeg(rhs);

    if (unop.op == TokenType::op_excla_mark) return bool_to_int(m_builder.CreateNot(int_to_bool(rhs)));

    if (unop.op == TokenType::amp) {
        return rhs;
    }

    unop.dump();
    dmz_unreachable("unknown unary op");
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

    binop.dump();
    dmz_unreachable("unexpected binary operator");
    return nullptr;
}

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

llvm::Value *Codegen::generate_err_decl_ref_expr(const ResolvedErrDeclRefExpr &errDeclRefExpr) {
    return m_declarations[&errDeclRefExpr.decl];
}

llvm::Value *Codegen::generate_err_unwrap_expr(const ResolvedErrUnwrapExpr &errUnwrapExpr) {
    llvm::Function *function = get_current_function();

    auto *trueBB = llvm::BasicBlock::Create(m_context, "if.true.unwrap");
    auto *exitBB = llvm::BasicBlock::Create(m_context, "if.exit.unwrap");

    llvm::BasicBlock *elseBB = exitBB;

    llvm::Value *err_struct = generate_expr(*errUnwrapExpr.errToUnwrap, true);

    llvm::Value *err_value_ptr =
        m_builder.CreateStructGEP(generate_type(errUnwrapExpr.errToUnwrap->type), err_struct, 1);
    llvm::Value *err_value = load_value(err_value_ptr, Type::builtinErr("err"));

    m_builder.CreateCondBr(ptr_to_bool(err_value), trueBB, elseBB);

    trueBB->insertInto(function);
    m_builder.SetInsertPoint(trueBB);
    for (auto &&d : errUnwrapExpr.defers)
    {
        generate_block(*d->resolvedDefer.block);
    }
    
    if (m_currentFunction->type.isOptional) {
        llvm::Value *dst = m_builder.CreateStructGEP(generate_type(m_currentFunction->type), retVal, 1);
        store_value(err_value, dst, Type::builtinErr("err"));

        assert(retBB && "function with return stmt doesn't have a return block");
        break_into_bb(retBB);
    } else {
        llvm::Function *trapIntrinsic = llvm::Intrinsic::getDeclaration(&m_module, llvm::Intrinsic::trap);
        m_builder.CreateCall(trapIntrinsic, {});
    }
    break_into_bb(exitBB);

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);

    auto unwrapedValue = m_builder.CreateStructGEP(generate_type(errUnwrapExpr.errToUnwrap->type), err_struct, 0);
    return load_value(unwrapedValue, errUnwrapExpr.type);
}

llvm::Value *Codegen::generate_catch_err_expr(const ResolvedCatchErrExpr &catchErrExpr) {
    llvm::Value *err_struct = nullptr;
    llvm::Value *decl_value = nullptr;
    Type err_type;
    if (catchErrExpr.errToCatch) {
        err_struct = generate_expr(*catchErrExpr.errToCatch, true);
        err_type = catchErrExpr.errToCatch->type;
    } else if (catchErrExpr.declaration) {
        err_struct = generate_expr(*catchErrExpr.declaration->varDecl->initializer, true);

        decl_value = allocate_stack_variable(catchErrExpr.declaration->varDecl->identifier,
                                             catchErrExpr.declaration->varDecl->type);

        m_declarations[catchErrExpr.declaration->varDecl.get()] = decl_value;

        err_type = catchErrExpr.declaration->varDecl->initializer->type;
    } else {
        dmz_unreachable("malformed ResolvedCatchErrExpr");
    }
    llvm::Value *err_value_ptr = m_builder.CreateStructGEP(generate_type(err_type), err_struct, 1);
    llvm::Value *err_value = load_value(err_value_ptr, Type::builtinErr("err"));
    llvm::Value *err_value_bool = ptr_to_bool(err_value);
    if (catchErrExpr.declaration) {
        llvm::Value *selectedError = m_builder.CreateSelect(err_value_bool, err_value, m_success, "select.err");
        store_value(selectedError, decl_value, Type::builtinErr("err"));
    }

    return m_builder.CreateSelect(err_value_bool, m_builder.getInt32(1), m_builder.getInt32(0), "catch.result");
}

llvm::Value *Codegen::generate_try_err_expr(const ResolvedTryErrExpr &tryErrExpr) {
    llvm::Value *err_struct = nullptr;
    llvm::Value *decl_value = nullptr;
    Type err_type;
    if (tryErrExpr.errToTry) {
        err_struct = generate_expr(*tryErrExpr.errToTry, true);
        err_type = tryErrExpr.errToTry->type;
    } else if (tryErrExpr.declaration) {
        err_struct = generate_expr(*tryErrExpr.declaration->varDecl->initializer, true);

        decl_value =
            allocate_stack_variable(tryErrExpr.declaration->varDecl->identifier, tryErrExpr.declaration->varDecl->type);

        m_declarations[tryErrExpr.declaration->varDecl.get()] = decl_value;

        err_type = tryErrExpr.declaration->varDecl->initializer->type;
    } else {
        dmz_unreachable("malformed ResolvedTryErrExpr");
    }
    llvm::Value *err_value_ptr = m_builder.CreateStructGEP(generate_type(err_type), err_struct, 1);
    llvm::Value *err_value = load_value(err_value_ptr, Type::builtinErr("err"));

    if (tryErrExpr.declaration) {
        llvm::Value *value_ptr = m_builder.CreateStructGEP(generate_type(err_type), err_struct, 0);
        llvm::Value *value = load_value(value_ptr, tryErrExpr.declaration->varDecl->type);
        store_value(value, decl_value, tryErrExpr.declaration->varDecl->type);
    }

    return m_builder.CreateSelect(ptr_to_bool(err_value), m_builder.getInt32(0), m_builder.getInt32(1), "try.result");
}
}  // namespace DMZ
