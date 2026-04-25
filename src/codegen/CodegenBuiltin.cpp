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
#include "codegen/Codegen.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {
llvm::Value *Codegen::generate_builtin_function(const ResolvedBuiltinFunctionDecl &builtin,
                                                const ResolvedCallExpr &call) {
    debug_func("" << builtin.identifier);
    if (builtin.identifier == "@call") {
        return generate_builtin_call(call);
    } else if (builtin.identifier == "@atomicLoad") {
        return generate_builtin_atomicLoad(call);
    } else if (builtin.identifier == "@atomicStore") {
        return generate_builtin_atomicStore(call);
    } else if (builtin.identifier == "@atomicCmpExS" || builtin.identifier == "@atomicCmpExW") {
        return generate_builtin_atomicCmpEx(call, builtin.identifier == "@atomicCmpExW");
    } else if (builtin.identifier == "@atomicRmw") {
        return generate_builtin_atomicRmw(call);
    }
    dmz_unreachable(call.location, "unsuported builtin function");
}

llvm::Value *Codegen::generate_builtin_call(const ResolvedCallExpr &call) {
    debug_func(call.location);
    llvm::Value *callee = generate_expr(*call.arguments[0]);
    llvm::Value *tuplePtr = generate_expr(*call.arguments[1], true);

    auto *fnPtrType = dynamic_cast<const ResolvedTypePointer *>(call.arguments[0]->type.get());
    if (!fnPtrType) {
        dmz_unreachable(call.location, "@call: callee is not a function pointer");
    }
    auto *fnType = dynamic_cast<const ResolvedTypeFunction *>(fnPtrType->pointerType.get());
    if (!fnType) {
        dmz_unreachable(call.location, "@call: callee is not a function pointer");
    }

    auto *tupleTypeStruct = dynamic_cast<const ResolvedTypeStruct *>(call.arguments[1]->type.get());
    if (!tupleTypeStruct) {
        dmz_unreachable(call.location, "@call: argument is not a tuple");
    }
    auto *tupleDecl = tupleTypeStruct->decl;

    std::vector<llvm::Value *> args;
    bool isReturningStruct = fnType->returnType->generate_struct();
    llvm::Value *callRetVal = nullptr;

    if (isReturningStruct) {
        callRetVal = allocate_stack_variable(call.location, "struct.ret.tmp", *fnType->returnType);
        args.emplace_back(callRetVal);
    }

    for (size_t i = 0; i < tupleDecl->fields.size(); i++) {
        auto &field = tupleDecl->fields[i];
        llvm::Value *fieldPtr = m_builder.CreateStructGEP(generate_type(*tupleTypeStruct, true), tuplePtr, i);
        llvm::Value *argVal = load_value(fieldPtr, *field->type);
        argVal = cast_to(argVal, *field->type, *fnType->paramsTypes[i]);
        args.emplace_back(argVal);
    }

    llvm::FunctionType *llvmType = generate_function_type(*fnType);
    llvm::CallInst *callInst = m_builder.CreateCall(llvmType, callee, args);
    callInst->setAttributes(construct_attr_list(*fnType));

    return isReturningStruct ? callRetVal : callInst;
}

llvm::Value *Codegen::generate_builtin_atomicLoad(const ResolvedCallExpr &call) {
    debug_func(call.location);
    llvm::Value *ptr = generate_expr(*call.arguments[0]);
    auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(call.arguments[0]->type.get());
    llvm::Type *llvmType = generate_type(*ptrType->pointerType);
    auto *load =
        m_builder.CreateAlignedLoad(llvmType, ptr, m_module->getDataLayout().getPrefTypeAlign(llvmType), "atomic.load");
    load->setOrdering(llvm::AtomicOrdering::SequentiallyConsistent);
    return load;
}

llvm::Value *Codegen::generate_builtin_atomicStore(const ResolvedCallExpr &call) {
    debug_func(call.location);
    llvm::Value *ptr = generate_expr(*call.arguments[0]);
    llvm::Value *val = generate_expr(*call.arguments[1]);
    auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(call.arguments[0]->type.get());
    val = cast_to(val, *call.arguments[1]->type, *ptrType->pointerType);
    auto *store = m_builder.CreateAlignedStore(val, ptr, m_module->getDataLayout().getPrefTypeAlign(val->getType()));
    store->setOrdering(llvm::AtomicOrdering::SequentiallyConsistent);
    return nullptr;
}

llvm::Value *Codegen::generate_builtin_atomicCmpEx(const ResolvedCallExpr &call, bool isWeak) {
    debug_func(call.location);
    llvm::Value *ptr = generate_expr(*call.arguments[0]);
    llvm::Value *expected = generate_expr(*call.arguments[1]);
    llvm::Value *desired = generate_expr(*call.arguments[2]);
    auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(call.arguments[0]->type.get());
    expected = cast_to(expected, *call.arguments[1]->type, *ptrType->pointerType);
    desired = cast_to(desired, *call.arguments[2]->type, *ptrType->pointerType);

    auto successOrdering = llvm::AtomicOrdering::SequentiallyConsistent;
    auto failureOrdering = llvm::AtomicOrdering::SequentiallyConsistent;
    auto *res = m_builder.CreateAtomicCmpXchg(ptr, expected, desired,
                                              m_module->getDataLayout().getPrefTypeAlign(expected->getType()),
                                              successOrdering, failureOrdering);
    res->setWeak(isWeak);
    return m_builder.CreateExtractValue(res, 1, "cmpxchg.success");
}

llvm::Value *Codegen::generate_builtin_atomicRmw(const ResolvedCallExpr &call) {
    debug_func(call.location);
    llvm::Value *ptr = generate_expr(*call.arguments[0]);
    int opVal = call.arguments[1]->get_constant_value().value_or(-1);
    if (opVal < 0) {
        dmz_unreachable(call.location, "@atomicRmw: operator is not a constant");
    }
    llvm::Value *value = generate_expr(*call.arguments[2]);
    auto align = m_module->getDataLayout().getABITypeAlign(value->getType());
    llvm::AtomicRMWInst::BinOp llvmOp;
    bool isFloat = false;
    if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(call.arguments[2]->type.get())) {
        isFloat = numType->numberKind == ResolvedNumberKind::Float;
    }
    switch (opVal) {
        case 0:  // .Add
            llvmOp = isFloat ? llvm::AtomicRMWInst::FAdd : llvm::AtomicRMWInst::Add;
            break;
        case 1:  // .Sub
            llvmOp = isFloat ? llvm::AtomicRMWInst::FSub : llvm::AtomicRMWInst::Sub;
            break;
        case 2:  // .And
            llvmOp = llvm::AtomicRMWInst::And;
            break;
        case 3:  // .Nand
            llvmOp = llvm::AtomicRMWInst::Nand;
            break;
        case 4:  // .Or
            llvmOp = llvm::AtomicRMWInst::Or;
            break;
        case 5:  // .Xor
            llvmOp = llvm::AtomicRMWInst::Xor;
            break;
        case 6:  // .Min
            llvmOp = isFloat ? llvm::AtomicRMWInst::FMin : llvm::AtomicRMWInst::Min;
            break;
        case 7:  // .Max
            llvmOp = isFloat ? llvm::AtomicRMWInst::FMax : llvm::AtomicRMWInst::Max;
            break;
        case 8:  // .Xchg
            llvmOp = llvm::AtomicRMWInst::Xchg;
            break;
        default:
            dmz_unreachable(call.location, "unsupported atomic RMW operator");
    }
    return m_builder.CreateAtomicRMW(llvmOp, ptr, value, llvm::MaybeAlign(align),
                                     llvm::AtomicOrdering::SequentiallyConsistent);
}
}  // namespace DMZ
