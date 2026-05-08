#ifdef DEBUG_CODEGEN
#ifndef DEBUG
#define DEBUG
#endif
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/Module.h>
#pragma GCC diagnostic pop

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
        if (auto value = val->toInt()) {
            return llvm::ConstantInt::get(generate_type(*expr.type), *value);
        }
    }
    if (auto *number = dynamic_cast<const ResolvedFloatLiteral *>(&expr)) {
        return llvm::ConstantFP::get(generate_type(*number->type), number->value);
    }
    if (auto *number = dynamic_cast<const ResolvedIntLiteral *>(&expr)) {
        return llvm::ConstantInt::get(generate_type(*number->type), number->value);
    }
    if (auto *number = dynamic_cast<const ResolvedCharLiteral *>(&expr)) {
        return m_builder.getInt8(number->value);
    }
    if (auto *number = dynamic_cast<const ResolvedBoolLiteral *>(&expr)) {
        return m_builder.getInt1(number->value);
    }
    if (auto *str = dynamic_cast<const ResolvedStringLiteral *>(&expr)) {
        auto ptr = create_global_string(str->value, "string.literal");
        if (str->type->kind == ResolvedTypeKind::Slice) {
            auto slice = allocate_stack_variable(str->location, "string.literal.slice", *str->type);
            auto sliceType = generate_type(*str->type);
            llvm::Value *slice_value = llvm::UndefValue::get(sliceType);
            slice_value = m_builder.CreateInsertValue(slice_value, ptr, 0);
            auto length = llvm::ConstantInt::get(m_builder.getIntPtrTy(m_module->getDataLayout()), str->value.size());
            slice_value = m_builder.CreateInsertValue(slice_value, length, 1);
            m_builder.CreateStore(slice_value, slice);
            return slice;
        }
        return ptr;
    }
    if (dynamic_cast<const ResolvedNullLiteral *>(&expr)) {
        return llvm::Constant::getNullValue(m_builder.getPtrTy());
    }
    if (auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(&expr)) {
        return generate_decl_ref_expr(*dre, keepPointer);
    }
    if (auto *ge = dynamic_cast<const ResolvedGenericExpr *>(&expr)) {
        if (auto fnDecl = dynamic_cast<const ResolvedFuncDecl *>(&ge->decl)) {
            return generate_function_decl(*fnDecl);
        }
        auto val = m_declarations[&ge->decl];
        bool kp = keepPointer;
        kp |= ge->type->generate_struct();
        kp |= ge->type->kind == ResolvedTypeKind::Array;
        return kp ? val : load_value(val, *ge->type);
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
    if (auto *uie = dynamic_cast<const ResolvedUnionInstantiationExpr *>(&expr)) {
        return generate_temporary_union(*uie);
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
    if (auto *comptimeExpr = dynamic_cast<const ResolvedComptimeExpr *>(&expr)) {
        if (auto val = comptimeExpr->get_constant_value()) {
            if (val->isInt()) {
                return llvm::ConstantInt::get(generate_type(*comptimeExpr->type), val->getInt());
            } else if (val->isBool()) {
                return m_builder.getInt1(val->getBool());
            } else if (val->isFloat()) {
                return llvm::ConstantFP::get(generate_type(*comptimeExpr->type), val->getFloat());
            } else if (val->isString()) {
                return create_global_string(val->getString(), "comptime.string");
            }
        }
        // If it's a statement or void, it might not have a value we care about for codegen
        return nullptr;
    }
    expr.dump();
    dmz_unreachable(expr.location, "unexpected expression");
}

llvm::Value *Codegen::generate_call_expr(const ResolvedCallExpr &call) {
    debug_func("");
    if (auto *declRef = dynamic_cast<const ResolvedDeclRefExpr *>(call.callee.get())) {
        if (auto *builtin = dynamic_cast<const ResolvedBuiltinFunctionDecl *>(&declRef->decl)) {
            return generate_builtin_function(*builtin, call);
        }
    }
    llvm::Value *callee = generate_expr(*call.callee);
    ResolvedTypeFunction *fnType = dynamic_cast<ResolvedTypeFunction *>(call.callee->type.get());
    if (!fnType) {
        if (auto ptrType = dynamic_cast<ResolvedTypePointer *>(call.callee->type.get())) {
            if (auto funcType = dynamic_cast<ResolvedTypeFunction *>(ptrType->pointerType.get())) {
                fnType = funcType;
            } else {
                dmz_unreachable(call.callee->location,
                                "unexpected type '" + ptrType->pointerType->to_str() + "', expected function");
            }
        } else {
            dmz_unreachable(call.callee->location,
                            "unexpected type '" + call.callee->type->to_str() + "', expected pointer to function");
        }
    }

    if (!fnType) {
        dmz_unreachable(call.callee->location, "unexpected type in callee '" + call.callee->type->to_str() + "'");
    }

    bool isReturningStruct = fnType->returnType->generate_struct();
    llvm::Value *callRetVal = nullptr;
    std::vector<llvm::Value *> args;

    if (isReturningStruct) {
        callRetVal = args.emplace_back(allocate_stack_variable(call.location, "struct.ret.tmp", *fnType->returnType));
    }

    bool isVarArg = false;
    for (size_t i = 0; i < call.arguments.size(); i++) {
        auto argExpr = generate_expr(*call.arguments[i], call.arguments[i]->type->generate_struct());
        // Only cast if is not vararg
        if (!isVarArg && fnType->paramsTypes[i]->kind != ResolvedTypeKind::VarArg) {
            argExpr = cast_to(argExpr, *call.arguments[i]->type, *fnType->paramsTypes[i]);
        } else {
            isVarArg = true;
        }
        args.emplace_back(argExpr);
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
                dmz_unreachable(unop.location, "not expected type in op_minus");
        } else {
            dmz_unreachable(unop.location, "not expected type in op_minus");
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
                dmz_unreachable(unop.location, "not expected type in op_plusplus");
            }
            store_value(ret, rhs, *typeNum, *typeNum);
            return rhs_value;
        } else {
            dmz_unreachable(unop.location, "not expected type in op_plusplus");
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
                dmz_unreachable(unop.location, "not expected type in op_minusminus");
            }
            store_value(ret, rhs, *typeNum, *typeNum);
            return rhs_value;
        } else {
            dmz_unreachable(unop.location, "not expected type in op_minusminus");
        }
    }
    if (unop.op == TokenType::op_excla_mark) return m_builder.CreateNot(to_bool(rhs, *unop.operand->type));
    if (unop.op == TokenType::op_tilde) return m_builder.CreateXor(rhs, llvm::ConstantInt::get(rhs->getType(), -1));

    unop.dump();
    dmz_unreachable(unop.location, "unknown unary op");
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

    keepPointer |= expr.type->generate_struct();
    keepPointer |= expr.type->kind == ResolvedTypeKind::Array;

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
    ptr<ResolvedTypeNumber> usizeType = castPtr<ResolvedTypeNumber>(ResolvedTypeNumber::usize(binop.lhs->location));
    auto typeNum = dynamic_cast<const ResolvedTypeNumber *>(binop.lhs->type.get());
    if (!typeNum) {
        if (auto simdType = dynamic_cast<const ResolvedTypeSimd *>(binop.lhs->type.get())) {
            typeNum = dynamic_cast<const ResolvedTypeNumber *>(simdType->simdType.get());
        }
        if (dynamic_cast<const ResolvedTypeError *>(binop.lhs->type.get())) {
            typeNum = usizeType.get();
        }
        if (dynamic_cast<const ResolvedTypePointer *>(binop.lhs->type.get())) {
            typeNum = usizeType.get();
        }
        if (!typeNum) {
            binop.lhs->type->dump();
            dmz_unreachable(binop.location, "not expected type in binop");
        }
    }
    if (binop.op == TokenType::op_plus || binop.op == TokenType::op_plus_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateAdd(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFAdd(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_plus");
    }
    if (binop.op == TokenType::op_minus || binop.op == TokenType::op_minus_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateSub(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFSub(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_minus");
    }
    if (binop.op == TokenType::asterisk || binop.op == TokenType::op_asterisk_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateMul(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFMul(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in asterisk");
    }
    if (binop.op == TokenType::op_div || binop.op == TokenType::op_div_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateSDiv(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateUDiv(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFDiv(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_div");
    }
    if (binop.op == TokenType::op_percent) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateSRem(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateURem(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFRem(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_percent");
    }
    if (binop.op == TokenType::op_less) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateICmpSLT(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpULT(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpULT(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_less");
    }
    if (binop.op == TokenType::op_less_eq) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateICmpSLE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpULE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpULE(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_less");
    }
    if (binop.op == TokenType::op_more) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateICmpSGT(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpUGT(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpUGT(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_more");
    }
    if (binop.op == TokenType::op_more_eq) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateICmpSGE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpUGE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpUGE(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_more_eq");
    }
    if (binop.op == TokenType::op_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpEQ(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpUEQ(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_equal");
    }
    if (binop.op == TokenType::op_not_equal) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt)
            return m_builder.CreateICmpNE(lhs, rhs);
        else if (typeNum->numberKind == ResolvedNumberKind::Float)
            return m_builder.CreateFCmpUNE(lhs, rhs);
        else
            dmz_unreachable(binop.location, "not expected type in op_not_equal");
    }
    if (binop.op == TokenType::amp) {
        return m_builder.CreateAnd(lhs, rhs);
    }
    if (binop.op == TokenType::pipe) {
        return m_builder.CreateOr(lhs, rhs);
    }
    if (binop.op == TokenType::caret) {
        return m_builder.CreateXor(lhs, rhs);
    }
    if (binop.op == TokenType::op_shl) {
        return m_builder.CreateShl(lhs, rhs);
    }
    if (binop.op == TokenType::op_shr) {
        if (typeNum->numberKind == ResolvedNumberKind::Int)
            return m_builder.CreateAShr(lhs, rhs);
        else
            return m_builder.CreateLShr(lhs, rhs);
    }

    binop.dump();
    dmz_unreachable(binop.location, "unexpected binary operator");
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
    if (auto fnDecl = dynamic_cast<const ResolvedFuncDecl *>(&dre.decl)) {
        return generate_function_decl(*fnDecl);
    } else if (dynamic_cast<const ResolvedDeclStmt *>(&dre.decl) || dynamic_cast<const ResolvedVarDecl *>(&dre.decl)) {
        const ResolvedVarDecl *varDecl = nullptr;
        if (auto decl = dynamic_cast<const ResolvedDeclStmt *>(&dre.decl)) {
            varDecl = decl->varDecl.get();
        } else {
            varDecl = dynamic_cast<const ResolvedVarDecl *>(&dre.decl);
        }

        if (auto fnType = dynamic_cast<ResolvedTypeFunction *>(varDecl->type.get())) {
            if (fnType->fnDecl) {
                return generate_function_decl(*fnType->fnDecl);
            } else {
                dmz_unreachable(dre.location, "TODO");
            }
        } else {
            auto ret = m_declarations[varDecl];
            if (!ret) {
                ret = m_declarations[varDecl->parentDeclStmt];
                if (!ret && varDecl->isGlobal && varDecl->parentDeclStmt) {
                    generate_global_var_decl(*varDecl->parentDeclStmt);
                    ret = m_declarations[varDecl];
                    if (!ret) ret = m_declarations[varDecl->parentDeclStmt];
                }

                if (!ret) {
                    if (varDecl->parentDeclStmt)
                        varDecl->parentDeclStmt->dump();
                    else
                        varDecl->dump();
                    dmz_unreachable(dre.location, "TODO");
                }
            }
            return keepPointer ? ret : load_value(ret, *varDecl->type);
        }
    } else {
        val = m_declarations[&dre.decl];
        if (!val) {
            dmz_unreachable(dre.decl.location, "not in declarations");
        }
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
        llvm::Type *type = generate_type(*typeToGenerate, true);
        if (typeToGenerate->kind == ResolvedTypeKind::Union || typeToGenerate->kind == ResolvedTypeKind::UnionDecl) {
            if (member->index == -1) {
                llvm::Value *field = m_builder.CreateStructGEP(type, base, 0);
                return keepPointer ? field : load_value(field, *member->type);
            }
            llvm::Value *payloadArrayPtr = m_builder.CreateStructGEP(type, base, 1);
            llvm::Type *fieldType = generate_type(*member->type);
            llvm::Value *fieldPtr = m_builder.CreateBitCast(payloadArrayPtr, llvm::PointerType::get(fieldType, 0));

            keepPointer |= member->type->generate_struct();
            keepPointer |= member->type->kind == ResolvedTypeKind::Array;

            return keepPointer ? fieldPtr : load_value(fieldPtr, *member->type);
        }
        llvm::Value *field = m_builder.CreateStructGEP(type, base, member->index);

        keepPointer |= member->type->generate_struct();
        keepPointer |= member->type->kind == ResolvedTypeKind::Array;

        return keepPointer ? field : load_value(field, *member->type);
    } else if (auto errDecl = dynamic_cast<const ResolvedErrorDecl *>(&memberExpr.member)) {
        auto ret = m_declarations[errDecl];
        if (!ret) {
            ret = generate_error_decl(*errDecl);
            if (!ret) {
                dmz_unreachable(errDecl->location, "TODO");
            }
        }
        return ret;
    } else if (auto fnDecl = dynamic_cast<const ResolvedFuncDecl *>(&memberExpr.member)) {
        return generate_function_decl(*fnDecl);
    } else if (dynamic_cast<const ResolvedDeclStmt *>(&memberExpr.member) ||
               dynamic_cast<const ResolvedVarDecl *>(&memberExpr.member)) {
        const ResolvedVarDecl *varDecl = nullptr;
        if (auto decl = dynamic_cast<const ResolvedDeclStmt *>(&memberExpr.member)) {
            varDecl = decl->varDecl.get();
        } else if (auto decl = dynamic_cast<const ResolvedVarDecl *>(&memberExpr.member)) {
            varDecl = decl;
        } else {
            dmz_unreachable(memberExpr.member.location, "TODO");
        }
        if (auto fnType = dynamic_cast<ResolvedTypeFunction *>(varDecl->type.get())) {
            if (fnType->fnDecl) {
                return generate_function_decl(*fnType->fnDecl);
            } else {
                dmz_unreachable(memberExpr.location, "TODO");
            }
        } else {
            auto ret = m_declarations[varDecl];
            if (!ret) {
                ret = m_declarations[varDecl->parentDeclStmt];
                if (!ret && varDecl->isGlobal && varDecl->parentDeclStmt) {
                    generate_global_var_decl(*varDecl->parentDeclStmt);
                    ret = m_declarations[varDecl];
                    if (!ret) ret = m_declarations[varDecl->parentDeclStmt];
                }

                if (!ret) {
                    if (varDecl->parentDeclStmt)
                        varDecl->parentDeclStmt->dump();
                    else
                        varDecl->dump();
                    dmz_unreachable(memberExpr.location, "TODO");
                }
            }
            return keepPointer ? ret : load_value(ret, *varDecl->type);
        }
    } else {
        memberExpr.member.dump();
        report(memberExpr.location, "unexpected member expresion");
        dmz_unreachable(memberExpr.location, "Unexpected member expresion");
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
    } else if (arrayAtExpr.array->type->kind == ResolvedTypeKind::Simd) {
        if (keepPointer)
            dmz_unreachable(
                arrayAtExpr.location,
                "This should not happend, simd types are not meant to be used as base of array at expressions");
        return m_builder.CreateExtractElement(generate_expr(*arrayAtExpr.array), generate_expr(*arrayAtExpr.index));
        // type = generate_type(*arrayAtExpr.array->type);
        // idxs = {m_builder.getInt32(0), generate_expr(*arrayAtExpr.index)};
    } else if (arrayAtExpr.array->type->kind == ResolvedTypeKind::Slice) {
        auto slicetype = generate_type(*arrayAtExpr.array->type);
        base = m_builder.CreateStructGEP(slicetype, base, 0);
        base = load_value(base, *ResolvedTypePointer::opaquePtr(arrayAtExpr.location));

        type = generate_type(*arrayAtExpr.type);
        idxs = {generate_expr(*arrayAtExpr.index)};
    } else {
        dmz_unreachable(arrayAtExpr.location, "TODO");
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

    llvm::Type *structType = generate_type(*sie.type, true);
    llvm::Value *structVal = llvm::ConstantAggregateZero::get(structType);
    bool canOptimize = true;
    if (sie.fieldInitializers.size() == 0) canOptimize = false;
    for (auto &&initStmt : sie.fieldInitializers) {
        if (store_value_generate_memcpy(*initStmt->initializer->type) || !initStmt->initializer->isLiteral()) {
            canOptimize = false;
            break;
        }
    }

    std::unordered_map<const ResolvedFieldDecl *, llvm::Value *> initializerVals;
    for (auto &&initStmt : sie.fieldInitializers) {
        if (initStmt->initializer->type->kind == ResolvedTypeKind::DefaultInit) continue;
        initializerVals[&initStmt->field] = generate_expr(*initStmt->initializer);
        if (canOptimize) {
            structVal = m_builder.CreateInsertValue(structVal, initializerVals[&initStmt->field],
                                                    (unsigned)initStmt->field.index);
        }
    }

    if (canOptimize) {
        m_builder.CreateStore(structVal, tmp);
    } else {
        for (size_t i = 0; i < sie.structDecl.fields.size(); i++) {
            auto &field = sie.structDecl.fields[i];
            if (sie.fieldInitializers[i]->initializer->type->kind == ResolvedTypeKind::DefaultInit) continue;
            llvm::Value *dst = m_builder.CreateStructGEP(generate_type(*sie.type), tmp, i);
            store_value(initializerVals[field.get()], dst, *field->type, *field->type);
        }
    }

    return tmp;
}

llvm::Value *Codegen::generate_temporary_union(const ResolvedUnionInstantiationExpr &uie) {
    debug_func("");
    std::string tmpName = "tmp.union." + uie.type->to_str();
    llvm::Value *tmp = allocate_stack_variable(uie.location, tmpName, *uie.type);

    auto unionLLVMType = generate_type(*uie.type);

    // 1. Store the tag
    llvm::Value *tagPtr = m_builder.CreateStructGEP(unionLLVMType, tmp, 0);
    llvm::Type *tagTy = static_cast<llvm::StructType *>(unionLLVMType)->getElementType(0);
    const llvm::DataLayout &dl = m_module->getDataLayout();
    m_builder.CreateStore(m_builder.getIntN(dl.getTypeSizeInBits(tagTy), uie.fieldInitializer->field.index), tagPtr);

    // 2. Store the payload
    if (uie.fieldInitializer->field.type->kind != ResolvedTypeKind::Void) {
        llvm::Value *payloadArrayPtr = m_builder.CreateStructGEP(unionLLVMType, tmp, 1);

        auto &initExpr = *uie.fieldInitializer->initializer;
        llvm::Value *initVal = generate_expr(initExpr, initExpr.type->generate_struct());

        llvm::Type *fieldType = generate_type(*uie.fieldInitializer->field.type);
        llvm::Value *fieldPtr = m_builder.CreateBitCast(payloadArrayPtr, llvm::PointerType::get(fieldType, 0));

        store_value(initVal, fieldPtr, *initExpr.type, *uie.fieldInitializer->field.type);
    }
    return tmp;
}

llvm::Value *Codegen::generate_temporary_array(const ResolvedArrayInstantiationExpr &aie) {
    debug_func("");
    if (aie.type->kind == ResolvedTypeKind::DefaultInit) return nullptr;
    auto typeArray = dynamic_cast<const ResolvedTypeArray *>(aie.type.get());
    if (!typeArray) {
        aie.dump();
        dmz_unreachable(aie.location, "unexpected type in array instantiation");
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
    auto global = m_module->getNamedGlobal(errName);
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
        if (ret) ret->print(llvm::errs(), true);
    }));

    keepPointer |= catchErrorExpr.type->generate_struct();
    keepPointer |= catchErrorExpr.type->kind == ResolvedTypeKind::Array;

    llvm::Function *function = get_current_function();
    auto *noErrorBB = llvm::BasicBlock::Create(*m_context, "catch.no_error", function);
    auto *hasErrorBB = llvm::BasicBlock::Create(*m_context, "catch.has_error", function);
    auto *exitBB = llvm::BasicBlock::Create(*m_context, "catch.exit", function);

    // Create alloca for result if not void
    llvm::Value *resultAddr = nullptr;
    if (catchErrorExpr.type->kind != ResolvedTypeKind::Void) {
        resultAddr = allocate_stack_variable(catchErrorExpr.location, "catch_result", *catchErrorExpr.type);
    }

    // Evaluate operand
    llvm::Value *error_struct = generate_expr(*catchErrorExpr.errorToCatch, true);
    ResolvedType *error_type = catchErrorExpr.errorToCatch->type.get();
    llvm::Value *error_value_ptr = m_builder.CreateStructGEP(generate_type(*error_type), error_struct, 1);

    // Check if error is NOT null
    llvm::Value *has_error = m_builder.CreateIsNotNull(m_builder.CreateLoad(m_builder.getPtrTy(), error_value_ptr));
    m_builder.CreateCondBr(has_error, hasErrorBB, noErrorBB);

    // --- No error case ---
    m_builder.SetInsertPoint(noErrorBB);
    if (resultAddr) {
        llvm::Value *value_ptr = m_builder.CreateStructGEP(generate_type(*error_type), error_struct, 0);
        llvm::Value *value = keepPointer ? value_ptr : load_value(value_ptr, *catchErrorExpr.type);
        store_value(value, resultAddr, *catchErrorExpr.type, *catchErrorExpr.type);
    }
    m_builder.CreateBr(exitBB);

    // --- Has error case ---
    m_builder.SetInsertPoint(hasErrorBB);

    // Register break target
    m_catchBreakTargets[&catchErrorExpr] = {resultAddr, exitBB};
    defer([&]() { m_catchBreakTargets.erase(&catchErrorExpr); });

    if (catchErrorExpr.errorVar) {
        llvm::Value *var_ptr = allocate_stack_variable(
            catchErrorExpr.errorVar->location, catchErrorExpr.errorVar->identifier, *catchErrorExpr.errorVar->type);
        m_declarations[catchErrorExpr.errorVar.get()] = var_ptr;
        llvm::Value *error_val =
            keepPointer ? error_value_ptr : load_value(error_value_ptr, *catchErrorExpr.errorVar->type);
        store_value(error_val, var_ptr, *catchErrorExpr.errorVar->type, *catchErrorExpr.errorVar->type);
    }

    if (auto resolvedHandlerExpr = dynamic_cast<const ResolvedExpr *>(catchErrorExpr.handler.get())) {
        llvm::Value *handler_val = generate_expr(*resolvedHandlerExpr, keepPointer);
        if (resultAddr && handler_val) {
            store_value(handler_val, resultAddr, *catchErrorExpr.type, *catchErrorExpr.type);
        }
    } else {
        generate_stmt(*catchErrorExpr.handler);
    }

    // Only jump to exit if the block didn't already terminate (e.g. via break or return)
    if (auto currentBB = m_builder.GetInsertBlock()) {
        if (!currentBB->getTerminator()) {
            m_builder.CreateBr(exitBB);
        }
    }

    // --- Exit ---
    m_builder.SetInsertPoint(exitBB);
    if (resultAddr) {
        ret = keepPointer ? resultAddr : load_value(resultAddr, *catchErrorExpr.type);
    }

    return ret;
}

llvm::Value *Codegen::generate_try_error_expr(const ResolvedTryErrorExpr &tryErrorExpr, bool keepPointer) {
    llvm::Value *ret = nullptr;
    debug_func(Dumper([&]() {
        if (ret) ret->print(llvm::errs());
    }));
    llvm::Function *function = get_current_function();

    // Implicitly clear error trace if this is a "root" call (non-failable calling failable)
    if (m_currentFunction && m_currentFunction->getFnType()->returnType->kind != ResolvedTypeKind::Optional) {
        generate_error_trace_clear();
    }

    auto *trueBB = llvm::BasicBlock::Create(*m_context, "if.true.try");
    auto *exitBB = llvm::BasicBlock::Create(*m_context, "if.exit.try");

    llvm::BasicBlock *elseBB = exitBB;

    llvm::Value *error_struct = generate_expr(*tryErrorExpr.errorToTry, true);

    llvm::Value *error_value_ptr =
        m_builder.CreateStructGEP(generate_type(*tryErrorExpr.errorToTry->type), error_struct, 1);
    llvm::Value *error_value = load_value(error_value_ptr, ResolvedTypeError{SourceLocation{}});

    llvm::Value *error_trace_idx = generate_error_trace_get_idx();
    m_builder.CreateCondBr(to_bool(error_value, ResolvedTypeError{SourceLocation{}}), trueBB, elseBB);

    trueBB->insertInto(function);
    m_builder.SetInsertPoint(trueBB);
    generate_error_trace_push(tryErrorExpr.location);
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
        llvm::Function *printErrorTraceFn = m_module->getFunction("std.builtin.printErrorTrace");
        if (!printErrorTraceFn) printErrorTraceFn = m_module->getFunction("printErrorTrace");

        auto printf_func = m_module->getOrInsertFunction(
            "printf", llvm::FunctionType::get(m_builder.getInt32Ty(), m_builder.getPtrTy(), true));
        if (printErrorTraceFn) {
            m_builder.CreateCall(printErrorTraceFn, {error_value});
        } else {
            auto fmt = create_global_string("help: import 'std' to enable error trace printing\n");
            m_builder.CreateCall(printf_func, {fmt});
        }

        std::string location = tryErrorExpr.location.file_name;
        if (std::filesystem::exists(location)) {
            location = std::filesystem::canonical(location);
        }
        location += ":";
        location += std::to_string(tryErrorExpr.location.line);
        location += ":";
        location += std::to_string(tryErrorExpr.location.col + 1);
        auto fmt = create_global_string(location + ": Aborted: Try catch an error value of '%s' in the function '" +
                                        m_currentFunction->identifier + "' that not return an optional\n");
        m_builder.CreateCall(printf_func, {fmt, error_value});
        auto exit_func = m_module->getOrInsertFunction(
            "exit", llvm::FunctionType::get(m_builder.getVoidTy(), m_builder.getInt32Ty(), false));
        m_builder.CreateCall(exit_func, {m_builder.getInt32(1)});
    }
    break_into_bb(exitBB);

    exitBB->insertInto(function);
    m_builder.SetInsertPoint(exitBB);
    generate_error_trace_clear(error_trace_idx);

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
    if (!typeOptional) dmz_unreachable(orelseErrorExpr.location, "unexpected type");
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

llvm::Value *Codegen::generate_slice_expr(const ResolvedType &type, const ResolvedExpr &from,
                                          const ResolvedRangeExpr &range) {
    const ResolvedTypeSlice *sliceType = dynamic_cast<const ResolvedTypeSlice *>(&type);
    if (!sliceType) dmz_unreachable(from.location, "unexpected type " + type.to_str());
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
        dmz_unreachable(range.location, "unexpected types in range '" + range.startExpr->type->to_str() + "' '" +
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
