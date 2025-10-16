#include <llvm-20/llvm/IR/Value.h>

#include "DMZPCH.hpp"
#include "Debug.hpp"
#include "Utils.hpp"
#include "codegen/Codegen.hpp"
#include "semantic/SemanticSymbols.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

llvm::Value *Codegen::generate_expr(const ResolvedExpr &expr, bool keepPointer) {
    debug_func(expr.location << " keepPointer " << (keepPointer ? "true" : "false"));
    set_debug_location(expr.location);
    defer([&]() { unset_debug_location(); });

    if (auto val = expr.get_constant_value()) {
        if (auto nt = dynamic_cast<ResolvedTypeNumber *>(expr.type.get())) {
            return m_builder.getIntN(nt->bitSize, *val);
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
    if (dynamic_cast<const ResolvedNullLiteral *>(&expr)) {
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
        return generate_expr(*grouping->expr, keepPointer);
    }
    if (auto *me = dynamic_cast<const ResolvedMemberExpr *>(&expr)) {
        return generate_member_expr(*me, keepPointer);
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
    if (auto *errorInPlace = dynamic_cast<const ResolvedErrorInPlaceExpr *>(&expr)) {
        return generate_error_in_place_expr(*errorInPlace);
    }
    if (auto *catchErr = dynamic_cast<const ResolvedCatchErrorExpr *>(&expr)) {
        return generate_catch_error_expr(*catchErr, keepPointer);
    }
    if (auto *tryErr = dynamic_cast<const ResolvedTryErrorExpr *>(&expr)) {
        return generate_try_error_expr(*tryErr, keepPointer);
    }
    if (auto *orelseErr = dynamic_cast<const ResolvedOrElseErrorExpr *>(&expr)) {
        return generate_orelse_error_expr(*orelseErr, keepPointer);
    }
    if (auto *sizeofExpr = dynamic_cast<const ResolvedSizeofExpr *>(&expr)) {
        return generate_sizeof_expr(*sizeofExpr);
    }
    expr.dump();
    dmz_unreachable("unexpected expression");
}

llvm::Value *Codegen::generate_call_expr(const ResolvedCallExpr &call) {
    debug_func("");
    llvm::Value *callee = generate_expr(*call.callee);
    ResolvedTypeFunction *fnType = dynamic_cast<ResolvedTypeFunction *>(call.callee->type.get());
    if (!fnType) {
        if (auto ptrType = dynamic_cast<ResolvedTypePointer *>(call.callee->type.get())) {
            if (auto funcType = dynamic_cast<ResolvedTypeFunction *>(ptrType->pointerType.get())) {
                fnType = funcType;
            } else {
                dmz_unreachable("unexpected type '" + ptrType->pointerType->to_str() + "', expected function");
            }
        } else {
            dmz_unreachable("unexpected type '" + call.callee->type->to_str() + "', expected pointer to function");
        }
    }

    if (!fnType) {
        dmz_unreachable("unexpected type in callee '" + call.callee->type->to_str() + "'");
    }

    bool isReturningStruct = fnType->returnType->generate_struct();
    llvm::Value *callRetVal = nullptr;
    std::vector<llvm::Value *> args;

    if (isReturningStruct) {
        callRetVal = args.emplace_back(allocate_stack_variable(call.location, "struct.ret.tmp", *fnType->returnType));
    }

    for (auto &&arg : call.arguments) {
        llvm::Value *val = generate_expr(*arg);

        args.emplace_back(val);
    }

    llvm::FunctionType *llvmType = static_cast<llvm::FunctionType *>(generate_type(*fnType));
    llvm::CallInst *callInst = m_builder.CreateCall(llvmType, callee, args);
    callInst->setAttributes(construct_attr_list(*fnType));

    return isReturningStruct ? callRetVal : callInst;
}

llvm::Value *Codegen::generate_unary_operator(const ResolvedUnaryOperator &unop) {
    debug_func("");
    bool keepPointer = unop.op == TokenType::op_plusplus || unop.op == TokenType::op_minusminus;
    llvm::Value *rhs = generate_expr(*unop.operand, keepPointer);

    if (unop.op == TokenType::op_minus) {
        if (auto typeNum = dynamic_cast<const ResolvedTypeNumber *>(unop.operand->type.get())) {
            if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
                return m_builder.CreateNeg(rhs);
            else if (typeNum->numberKind == ResolvedNumberKind::Float)
                return m_builder.CreateFNeg(rhs);
            else
                dmz_unreachable("not expected type in op_minus");
        } else {
            dmz_unreachable("not expected type in op_minus");
        }
    }

    if (unop.op == TokenType::op_plusplus) {
        if (auto typeNum = dynamic_cast<const ResolvedTypeNumber *>(unop.operand->type.get())) {
            llvm::Value *ret = nullptr;
            auto rhs_value = load_value(rhs, *typeNum);
            if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt) {
                ret = m_builder.CreateAdd(rhs_value, m_builder.getIntN(typeNum->bitSize, 1));
            } else if (typeNum->numberKind == ResolvedNumberKind::Float) {
                ret = m_builder.CreateFAdd(rhs_value, llvm::ConstantFP::get(generate_type(*typeNum), 1));
            } else {
                dmz_unreachable("not expected type in op_plusplus");
            }
            store_value(ret, rhs, *typeNum, *typeNum);
            return ret;
        } else {
            dmz_unreachable("not expected type in op_plusplus");
        }
    }
    if (unop.op == TokenType::op_minusminus) {
        if (auto typeNum = dynamic_cast<const ResolvedTypeNumber *>(unop.operand->type.get())) {
            llvm::Value *ret = nullptr;
            auto rhs_value = load_value(rhs, *typeNum);
            if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt) {
                ret = m_builder.CreateSub(rhs_value, m_builder.getIntN(typeNum->bitSize, 1));
            } else if (typeNum->numberKind == ResolvedNumberKind::Float) {
                ret = m_builder.CreateFSub(rhs_value, llvm::ConstantFP::get(generate_type(*typeNum), 1));
            } else {
                dmz_unreachable("not expected type in op_minusminus");
            }
            store_value(ret, rhs, *typeNum, *typeNum);
            return ret;
        } else {
            dmz_unreachable("not expected type in op_minusminus");
        }
    }
    if (unop.op == TokenType::op_excla_mark) return m_builder.CreateNot(to_bool(rhs, *unop.operand->type));

    unop.dump();
    dmz_unreachable("unknown unary op");
    return nullptr;
}

llvm::Value *Codegen::generate_ref_ptr_expr(const ResolvedRefPtrExpr &expr) {
    llvm::Value *v = nullptr;
    debug_func(Dumper([&]() {
        if (v)
            v->print(llvm::errs());
        else
            std::cerr << "nullptr";
    }));
    v = generate_expr(*expr.expr, true);
    return v;
}

llvm::Value *Codegen::generate_deref_ptr_expr(const ResolvedDerefPtrExpr &expr, bool keepPointer) {
    debug_func("");
    auto v = generate_expr(*expr.expr);
    return keepPointer ? v : load_value(v, *expr.type);
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
        llvm::Value *rhs = to_bool(generate_expr(*binop.rhs), *binop.rhs->type);

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
    rhs = cast_to(rhs, *binop.rhs->type, *binop.lhs->type);
    auto typeNum = dynamic_cast<const ResolvedTypeNumber *>(binop.lhs->type.get());
    if (!typeNum) {
        binop.lhs->type->dump();
        println(binop.location);
        dmz_unreachable("not expected type in binop");
    }
    if (binop.op == TokenType::op_plus || binop.op == TokenType::op_plus_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateAdd(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFAdd(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_plus");
    }
    if (binop.op == TokenType::op_minus || binop.op == TokenType::op_minus_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateSub(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFSub(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_minus");
    }
    if (binop.op == TokenType::asterisk || binop.op == TokenType::op_asterisk_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateMul(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFMul(lhs, rhs);
        else
            dmz_unreachable("not expected type in asterisk");
    }
    if (binop.op == TokenType::op_div || binop.op == TokenType::op_div_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateSDiv(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateUDiv(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFDiv(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_div");
    }
    if (binop.op == TokenType::op_percent) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateSRem(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateURem(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFRem(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_percent");
    }
    if (binop.op == TokenType::op_less) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateICmpSLT(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpULT(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpULT(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_less");
    }
    if (binop.op == TokenType::op_less_eq) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateICmpSLE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpULE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpULE(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_less");
    }
    if (binop.op == TokenType::op_more) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateICmpSGT(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpUGT(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpUGT(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_more");
    }
    if (binop.op == TokenType::op_more_eq) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateICmpSGE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpUGE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpUGE(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_more_eq");
    }
    if (binop.op == TokenType::op_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpEQ(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpUEQ(lhs, rhs);
        else
            dmz_unreachable("not expected type in op_equal");
    }
    if (binop.op == TokenType::op_not_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpNE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
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

    llvm::Value *val = to_bool(generate_expr(op), *op.type);
    m_builder.CreateCondBr(val, trueBB, falseBB);
}

llvm::Value *Codegen::generate_decl_ref_expr(const ResolvedDeclRefExpr &dre, bool keepPointer) {
    debug_func(dre.location << " keepPointer " << (keepPointer ? "true" : "false"));

    llvm::Value *val = nullptr;
    if (auto funcDecl = dynamic_cast<const ResolvedFuncDecl *>(&dre.decl)) {
        std::string funcName = generate_decl_name(*funcDecl);
        val = m_module->getFunction(funcName);
        if (!val) dmz_unreachable("internal error no function '" + funcName + "'");
        return val;
    } else {
        val = m_declarations[&dre.decl];
    }

    // keepPointer |= dynamic_cast<const ResolvedParamDecl *>(&dre.decl) && !dre.decl.isMutable;
    keepPointer |= dre.type->generate_struct();
    keepPointer |= dre.type->kind == ResolvedTypeKind::Array;

    return keepPointer ? val : load_value(val, *dre.type);
}

llvm::Value *Codegen::generate_member_expr(const ResolvedMemberExpr &memberExpr, bool keepPointer) {
    debug_func(memberExpr.location);
    if (auto member = dynamic_cast<const ResolvedFieldDecl *>(&memberExpr.member)) {
        llvm::Value *base = generate_expr(*memberExpr.base, true);
        ResolvedType *typeToGenerate = memberExpr.base->type.get();
        if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(typeToGenerate)) {
            typeToGenerate = ptrType->pointerType.get();
        }
        llvm::Type *type = generate_type(*typeToGenerate);
        llvm::Value *field = m_builder.CreateStructGEP(type, base, member->index);
        return keepPointer ? field : load_value(field, *member->type);
    } else if (auto errDecl = dynamic_cast<const ResolvedErrorDecl *>(&memberExpr.member)) {
        return m_declarations[errDecl];
    } else if (auto fnDecl = dynamic_cast<const ResolvedFuncDecl *>(&memberExpr.member)) {
        return generate_function_decl(*fnDecl);
    } else if (auto declStmt = dynamic_cast<const ResolvedDeclStmt *>(&memberExpr.member)) {
        if (auto fnType = dynamic_cast<ResolvedTypeFunction *>(declStmt->type.get())) {
            if (fnType->fnDecl) {
                return generate_function_decl(*fnType->fnDecl);
            } else {
                dmz_unreachable("TODO");
            }
        } else {
            dmz_unreachable("TODO");
        }
    } else {
        memberExpr.member.dump();
        report(memberExpr.location, "unexpected member expresion");
        dmz_unreachable("Unexpected member expresion");
    }
    return nullptr;
}

llvm::Value *Codegen::generate_array_at_expr(const ResolvedArrayAtExpr &arrayAtExpr, bool keepPointer) {
    llvm::Value *ret = nullptr;
    debug_func(Dumper([&]() {
        if (ret) ret->print(llvm::errs());
    }));
    if (auto rangeExpr = dynamic_cast<ResolvedRangeExpr *>(arrayAtExpr.index.get())) {
        return generate_slice_expr(*arrayAtExpr.type, *arrayAtExpr.array, *rangeExpr);
    }
    bool isPointer = arrayAtExpr.array->type->kind == ResolvedTypeKind::Pointer;
    llvm::Value *base = generate_expr(*arrayAtExpr.array, !isPointer);
    llvm::Type *type = nullptr;
    std::vector<llvm::Value *> idxs;
    if (arrayAtExpr.array->type->kind == ResolvedTypeKind::Pointer) {
        type = generate_type(*arrayAtExpr.type);
        idxs = {generate_expr(*arrayAtExpr.index)};
    } else if (arrayAtExpr.array->type->kind == ResolvedTypeKind::Array) {
        type = generate_type(*arrayAtExpr.array->type);
        idxs = {m_builder.getInt32(0), generate_expr(*arrayAtExpr.index)};
    } else if (arrayAtExpr.array->type->kind == ResolvedTypeKind::Slice) {
        auto slicetype = generate_type(*arrayAtExpr.array->type);
        base = m_builder.CreateStructGEP(slicetype, base, 0);
        base = load_value(base, *ResolvedTypePointer::opaquePtr(arrayAtExpr.location));

        type = generate_type(*arrayAtExpr.type);
        idxs = {generate_expr(*arrayAtExpr.index)};
    } else {
        dmz_unreachable("TODO");
    }
    llvm::Value *field = m_builder.CreateGEP(type, base, idxs);
    keepPointer |= arrayAtExpr.type->generate_struct();
    ret = keepPointer ? field : load_value(field, *arrayAtExpr.type);
    return ret;
}

llvm::Value *Codegen::generate_temporary_struct(const ResolvedStructInstantiationExpr &sie) {
    debug_func("");
    if (sie.type->kind == ResolvedTypeKind::DefaultInit) return nullptr;

    std::string tmpName = "tmp.struct.";
    if (auto struType = dynamic_cast<const ResolvedTypeStruct *>(sie.type.get())) {
        tmpName += struType->decl->type->to_str();
    } else {
        tmpName += sie.type->to_str();
    }
    llvm::Value *tmp = allocate_stack_variable(sie.location, tmpName, *sie.type);

    std::map<const ResolvedFieldDecl *, llvm::Value *> initializerVals;
    for (auto &&initStmt : sie.fieldInitializers) {
        if (initStmt->initializer->type->kind == ResolvedTypeKind::DefaultInit) continue;
        initializerVals[&initStmt->field] = generate_expr(*initStmt->initializer);
    }

    for (size_t i = 0; i < sie.structDecl.fields.size(); i++) {
        auto &field = sie.structDecl.fields[i];
        if (sie.fieldInitializers[i]->initializer->type->kind == ResolvedTypeKind::DefaultInit) continue;
        llvm::Value *dst = m_builder.CreateStructGEP(generate_type(*sie.type), tmp, i);
        store_value(initializerVals[field.get()], dst, *field->type, *field->type);
    }

    return tmp;
}

llvm::Value *Codegen::generate_temporary_array(const ResolvedArrayInstantiationExpr &aie) {
    debug_func("");
    if (aie.type->kind == ResolvedTypeKind::DefaultInit) return nullptr;
    auto typeArray = dynamic_cast<const ResolvedTypeArray *>(aie.type.get());
    if (!typeArray) {
        aie.dump();
        dmz_unreachable("unexpected type in array instantiation");
    }
    std::string varName = "array." + typeArray->to_str() + ".tmp";
    llvm::Value *tmp = allocate_stack_variable(aie.location, varName, *typeArray);

    size_t idx = 0;
    for (auto &&initExpr : aie.initializers) {
        auto var = generate_expr(*initExpr);
        llvm::Value *dst =
            m_builder.CreateGEP(generate_type(*typeArray), tmp, {m_builder.getInt32(0), m_builder.getInt32(idx++)});
        store_value(var, dst, *typeArray->arrayType, *typeArray->arrayType);
    }

    return tmp;
}

llvm::Value *Codegen::generate_error_in_place_expr(const ResolvedErrorInPlaceExpr &errorInPlaceExpr) {
    std::string errName = "error.str." + errorInPlaceExpr.identifier;
    auto global = m_module->getGlobalVariable(errName);
    if (global) {
        return global;
    } else {
        llvm::Constant *stringConst = llvm::ConstantDataArray::getString(*m_context, errorInPlaceExpr.identifier, true);
        return new llvm::GlobalVariable(*m_module, stringConst->getType(), true,
                                        llvm::GlobalVariable::LinkageTypes::PrivateLinkage, stringConst, errName);
    }
}

llvm::Value *Codegen::generate_catch_error_expr(const ResolvedCatchErrorExpr &catchErrorExpr, bool keepPointer) {
    llvm::Value *ret = nullptr;
    debug_func(Dumper([&]() {
        if (ret) ret->print(llvm::errs());
    }));
    llvm::Value *error_struct = generate_expr(*catchErrorExpr.errorToCatch, true);
    ResolvedType *error_type = catchErrorExpr.errorToCatch->type.get();
    llvm::Value *error_value_ptr = m_builder.CreateStructGEP(generate_type(*error_type), error_struct, 1);

    ret = keepPointer ? error_value_ptr : load_value(error_value_ptr, ResolvedTypeError{SourceLocation{}});
    return ret;
}

llvm::Value *Codegen::generate_try_error_expr(const ResolvedTryErrorExpr &tryErrorExpr, bool keepPointer) {
    llvm::Value *ret = nullptr;
    debug_func(Dumper([&]() {
        if (ret) ret->print(llvm::errs());
    }));
    llvm::Function *function = get_current_function();

    auto *trueBB = llvm::BasicBlock::Create(*m_context, "if.true.try");
    auto *exitBB = llvm::BasicBlock::Create(*m_context, "if.exit.try");

    llvm::BasicBlock *elseBB = exitBB;

    llvm::Value *error_struct = generate_expr(*tryErrorExpr.errorToTry, true);

    llvm::Value *error_value_ptr =
        m_builder.CreateStructGEP(generate_type(*tryErrorExpr.errorToTry->type), error_struct, 1);
    llvm::Value *error_value = load_value(error_value_ptr, ResolvedTypeError{SourceLocation{}});

    m_builder.CreateCondBr(to_bool(error_value, ResolvedTypeError{SourceLocation{}}), trueBB, elseBB);

    trueBB->insertInto(function);
    m_builder.SetInsertPoint(trueBB);
    for (auto &&d : tryErrorExpr.defers) {
        generate_block(*d->resolvedDefer.block);
    }

    auto retType = m_currentFunction->getFnType()->returnType.get();
    if (retType->kind == ResolvedTypeKind::Optional) {
        llvm::Value *dst = m_builder.CreateStructGEP(generate_type(*retType), retVal, 1);
        store_value(error_value, dst, ResolvedTypeError{SourceLocation{}}, ResolvedTypeError{SourceLocation{}});

        assert(retBB && "function with return stmt doesn't have a return block");
        break_into_bb(retBB);
    } else {
        auto fmt = m_builder.CreateGlobalString(tryErrorExpr.location.to_string() +
                                                ": Aborted: Try catch an error value of '%s' in the function '" +
                                                m_currentFunction->identifier + "' that not return an optional\n");
        auto printf_func = m_module->getOrInsertFunction(
            "printf", llvm::FunctionType::get(m_builder.getInt32Ty(), m_builder.getPtrTy(), true));
        m_builder.CreateCall(printf_func, {fmt, error_value});
        llvm::Function *trapIntrinsic = llvm::Intrinsic::getOrInsertDeclaration(m_module.get(), llvm::Intrinsic::trap);
        m_builder.CreateCall(trapIntrinsic, {});
    }
    break_into_bb(exitBB);

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);

    if (tryErrorExpr.type->kind == ResolvedTypeKind::Void) return nullptr;

    auto tryValue = m_builder.CreateStructGEP(generate_type(*tryErrorExpr.errorToTry->type), error_struct, 0);
    keepPointer |= tryErrorExpr.type->generate_struct();
    ret = keepPointer ? tryValue : load_value(tryValue, *tryErrorExpr.type);
    return ret;
}

llvm::Value *Codegen::generate_orelse_error_expr(const ResolvedOrElseErrorExpr &orelseErrorExpr, bool keepPointer) {
    llvm::Value *ret = nullptr;
    debug_func(Dumper([&]() {
        if (ret) ret->print(llvm::errs());
    }));
    llvm::Function *function = get_current_function();

    auto *trueBB = llvm::BasicBlock::Create(*m_context, "if.true.orelse");
    auto *exitBB = llvm::BasicBlock::Create(*m_context, "if.exit.orelse");

    llvm::BasicBlock *elseBB = exitBB;

    llvm::Value *error_struct = generate_expr(*orelseErrorExpr.errorToOrElse, true);

    llvm::Value *error_value_ptr =
        m_builder.CreateStructGEP(generate_type(*orelseErrorExpr.errorToOrElse->type), error_struct, 1);
    llvm::Value *error_value = load_value(error_value_ptr, ResolvedTypeError{SourceLocation{}});

    llvm::Value *return_value = allocate_stack_variable(orelseErrorExpr.location, "tmp.orelse", *orelseErrorExpr.type);

    auto typeOptional = dynamic_cast<const ResolvedTypeOptional *>(orelseErrorExpr.errorToOrElse->type.get());
    if (!typeOptional) dmz_unreachable("unexpected type");
    auto error_expr_value_ptr =
        m_builder.CreateStructGEP(generate_type(*orelseErrorExpr.errorToOrElse->type), error_struct, 0);
    auto error_expr_value = load_value(error_expr_value_ptr, *typeOptional->optionalType);
    store_value(error_expr_value, return_value, *typeOptional->optionalType, *orelseErrorExpr.type);

    llvm::Value *error_value_bool = to_bool(error_value, ResolvedTypeError{SourceLocation{}});
    m_builder.CreateCondBr(error_value_bool, trueBB, elseBB);

    trueBB->insertInto(function);
    m_builder.SetInsertPoint(trueBB);

    llvm::Value *orelse_value = generate_expr(*orelseErrorExpr.orElseExpr, false);

    store_value(orelse_value, return_value, *orelseErrorExpr.orElseExpr->type, *orelseErrorExpr.type);
    break_into_bb(exitBB);

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);
    keepPointer |= orelseErrorExpr.type->generate_struct();
    ret = keepPointer ? return_value : load_value(return_value, *orelseErrorExpr.type);
    return ret;
}

llvm::Value *Codegen::generate_sizeof_expr(const ResolvedSizeofExpr &sizeofExpr) {
    auto type = generate_type(*sizeofExpr.sizeofType);
    auto size = llvm::ConstantExpr::getSizeOf(type);
    return size;
}

llvm::Value *Codegen::generate_slice_expr(const ResolvedType &type, const ResolvedExpr &from,
                                          const ResolvedRangeExpr &range) {
    const ResolvedTypeSlice *sliceType = dynamic_cast<const ResolvedTypeSlice *>(&type);
    if (!sliceType) dmz_unreachable("unexpected type " + type.to_str());
    llvm::Value *ptr = generate_expr(from, true);
    if (from.type->kind == ResolvedTypeKind::Array) {
        // ptr = ptr;
    } else if (from.type->kind == ResolvedTypeKind::Pointer) {
        ptr = load_value(ptr, *from.type);
    } else if (from.type->kind == ResolvedTypeKind::Slice) {
        ptr = m_builder.CreateStructGEP(generate_type(*from.type), ptr, 0);
        ptr = load_value(ptr, ResolvedTypePointer{type.location, makePtr<ResolvedTypeVoid>(type.location)});
    } else {
        return report(from.location, "unexpected type used in generate of slice '" + from.type->to_str() + "'");
    }
    auto tmpSlice = allocate_stack_variable(from.location, "tmp.slice", *sliceType);

    // ptr + sizeof(type)
    if (!range.startExpr->type->compare(*range.endExpr->type)) {
        dmz_unreachable("unexpected types in range '" + range.startExpr->type->to_str() + "' '" +
                        range.endExpr->type->to_str() + "'");
    }
    auto startRange = generate_expr(*range.startExpr);
    auto endRange = generate_expr(*range.endExpr);
    ptr = m_builder.CreateGEP(generate_type(*sliceType->sliceType), ptr, startRange);
    auto size = m_builder.CreateSub(cast_to(endRange, *range.endExpr->type, *range.startExpr->type), startRange);

    auto structSliceType = generate_type(*sliceType);
    auto slicePtr = m_builder.CreateStructGEP(structSliceType, tmpSlice, 0);
    auto sliceSize = m_builder.CreateStructGEP(structSliceType, tmpSlice, 1);

    auto ptrType = ResolvedTypePointer{from.location, makePtr<ResolvedTypeVoid>(from.location)};
    store_value(ptr, slicePtr, ptrType, ptrType);
    auto sizeType = ResolvedTypeNumber{from.location, ResolvedNumberKind::UInt,
                                       static_cast<int>(m_module->getDataLayout().getPointerSizeInBits())};
    store_value(size, sliceSize, *range.startExpr->type, sizeType);

    return tmpSlice;
}
}  // namespace DMZ
