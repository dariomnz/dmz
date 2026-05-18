#include "semantic/Constexpr.hpp"
#ifdef DEBUG_CODEGEN
#ifndef DEBUG
#define DEBUG
#endif
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/InlineAsm.h>
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
    } else if (builtin.identifier == "@sizeof") {
        return generate_builtin_sizeof(call);
    } else if (builtin.identifier == "@typeid") {
        return generate_builtin_typeid(call);
    } else if (builtin.identifier == "@typeinfo") {
        return generate_builtin_typeinfo(call);
    } else if (builtin.identifier == "@hasMethod") {
        return generate_builtin_hasmethod(call);
    } else if (builtin.identifier == "@simdSize") {
        return generate_builtin_simdsize(call);
    } else if (builtin.identifier == "@simdSplat") {
        return generate_builtin_simdsplat(call);
    } else if (builtin.identifier == "@simdIota") {
        return generate_builtin_simdiota(call);
    } else if (builtin.identifier == "@testNum") {
        return generate_builtin_testnum(call);
    } else if (builtin.identifier == "@testName") {
        return generate_builtin_testname(call);
    } else if (builtin.identifier == "@testRun") {
        return generate_builtin_testrun(call);
    } else if (builtin.identifier == "@simdLoad") {
        return generate_builtin_simdLoad(call);
    } else if (builtin.identifier == "@simdStore") {
        return generate_builtin_simdStore(call);
    } else if (builtin.identifier == "@simdSelect") {
        return generate_builtin_simdSelect(call);
    } else if (builtin.identifier == "@simdReduce") {
        return generate_builtin_simdReduce(call);
    } else if (builtin.identifier == "@simdShuffle") {
        return generate_builtin_simdShuffle(call);
    } else if (builtin.identifier == "@errorTrace") {
        return generate_builtin_error_trace();
    } else if (builtin.identifier == "@asm") {
        return generate_builtin_asm(call);
    } else if (builtin.identifier == "@ptrCast") {
        return generate_builtin_ptrCast(call);
    } else if (builtin.identifier == "@intCast") {
        return generate_builtin_intCast(call);
    } else if (builtin.identifier == "@floatCast") {
        return generate_builtin_floatCast(call);
    } else if (builtin.identifier == "@bitCast") {
        return generate_builtin_bitCast(call);
    } else if (builtin.identifier == "@sqrt") {
        return generate_builtin_sqrt(call);
    }
    dmz_unreachable(call.location, "unsuported builtin function " + builtin.identifier);
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
    auto constValOpt = call.arguments[1]->get_constant_value();
    int opVal = constValOpt.has_value() ? constValOpt->getInt() : -1;
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

llvm::Value *Codegen::generate_builtin_sizeof(const ResolvedCallExpr &callExpr) {
    llvm::Type *type = nullptr;
    if (auto comptimeVal = callExpr.arguments[0]->get_constant_value()) {
        if (comptimeVal->isType()) {
            type = generate_type(*comptimeVal->getType());
        } else {
            dmz_unreachable(callExpr.location, "this should not happend: " + comptimeVal->to_str());
        }
    } else {
        type = generate_type(*callExpr.arguments[0]->type);
    }
    auto size = llvm::ConstantExpr::getSizeOf(type);
    return size;
}

llvm::Value *Codegen::generate_builtin_hasmethod(const ResolvedCallExpr &callExpr) {
    if (auto constVal = callExpr.get_constant_value()) {
        return llvm::ConstantInt::get(generate_type(*callExpr.type), constVal->getInt());
    } else {
        dmz_unreachable(callExpr.location, "this should not happend");
    }
}

llvm::Value *Codegen::generate_builtin_simdsize(const ResolvedCallExpr &callExpr) {
    if (auto constVal = callExpr.get_constant_value()) {
        return llvm::ConstantInt::get(generate_type(*callExpr.type), constVal->getInt());
    } else {
        dmz_unreachable(callExpr.location, "this should not happend");
    }
}

llvm::Value *Codegen::generate_builtin_typeid(const ResolvedCallExpr &callExpr) {
    if (auto typeId = callExpr.get_constant_value()) {
        return llvm::ConstantInt::get(generate_type(*callExpr.type), typeId->getInt());
    } else {
        dmz_unreachable(callExpr.location, "this should not happend");
    }
}

llvm::Value *Codegen::generate_builtin_typeinfo(const ResolvedCallExpr &callExpr) {
    auto targetType = callExpr.arguments[0]->type.get();
    auto returnStructPtrType = dynamic_cast<const ResolvedTypePointer *>(callExpr.type.get());
    if (!returnStructPtrType) dmz_unreachable(callExpr.location, "unreachable: " + callExpr.type->to_str());
    auto returnStructType = returnStructPtrType->pointerType.get();
    // auto llvmUnionType = static_cast<llvm::StructType *>(generate_type(*returnStructType));
    llvm::Type *sizeTy = llvm::Type::getIntNTy(*m_context, m_module->getDataLayout().getPointerSizeInBits());

    std::string globalName = "TypeInfo." + targetType->to_str();
    if (auto existingGlobal = m_module->getNamedGlobal(globalName)) {
        return existingGlobal;
    }

    const ResolvedUnionDecl *unionDecl = nullptr;
    if (auto ut = dynamic_cast<const ResolvedTypeUnion *>(returnStructType))
        unionDecl = ut->unionDecl();
    else if (auto ut = dynamic_cast<const ResolvedTypeUnionDecl *>(returnStructType))
        unionDecl = ut->unionDecl();
    if (!unionDecl) dmz_unreachable(callExpr.location, "TypeInfo must be a union");

    uint64_t maxSize = 0;
    const llvm::DataLayout &dl = m_module->getDataLayout();
    for (auto &&field : unionDecl->fields) {
        if (field->type->kind == ResolvedTypeKind::Void) continue;
        llvm::Type *t = generate_type(*field->type, true);
        maxSize = std::max(maxSize, dl.getTypeAllocSize(t).getFixedValue());
    }

    ResolvedTypeSlice sliceU8Type(callExpr.location,
                                  makePtr<ResolvedTypeNumber>(callExpr.location, ResolvedNumberKind::UInt, 8));
    auto llvmSliceU8Type = static_cast<llvm::StructType *>(generate_type(sliceU8Type));

    auto constValOpt = ConstantExpressionEvaluator{}.evaluate_type(*targetType);
    int tag = constValOpt.has_value() ? constValOpt->getInt() : 99;
    llvm::Constant *payload = nullptr;
    llvm::Type *payloadType = nullptr;

    if (targetType->kind == ResolvedTypeKind::Number) {
        auto numType = static_cast<const ResolvedTypeNumber *>(targetType);
        payloadType = llvm::StructType::get(*m_context, {m_builder.getInt32Ty()}, false);
        payload = llvm::ConstantStruct::get(static_cast<llvm::StructType *>(payloadType),
                                            {m_builder.getInt32(numType->bitSize)});
    } else if (targetType->kind == ResolvedTypeKind::StructDecl || targetType->kind == ResolvedTypeKind::Struct) {
        ResolvedStructDecl *structDecl = nullptr;
        if (auto sd = dynamic_cast<const ResolvedTypeStructDecl *>(targetType))
            structDecl = sd->decl;
        else if (auto sd = dynamic_cast<const ResolvedTypeStruct *>(targetType))
            structDecl = sd->decl;

        auto structNameGlobal = create_global_string(structDecl->name(), "typeinfo.name.str");
        auto structNameLen = llvm::ConstantInt::get(sizeTy, structDecl->name().size());
        auto structNameSlice = llvm::ConstantStruct::get(llvmSliceU8Type, {structNameGlobal, structNameLen});

        ResolvedTypeSlice sliceSliceU8Type(callExpr.location,
                                           makePtr<ResolvedTypeSlice>(callExpr.location, sliceU8Type.clone()));
        auto llvmSliceSliceU8Type = static_cast<llvm::StructType *>(generate_type(sliceSliceU8Type));

        // Fields setup
        std::vector<llvm::Constant *> fieldsArrayVals;
        for (auto &field : structDecl->fields_strs) {
            auto nameGlobal = create_global_string(field, "typeinfo.str");
            auto nameLen = llvm::ConstantInt::get(sizeTy, field.size());
            auto nameSlice = llvm::ConstantStruct::get(llvmSliceU8Type, {nameGlobal, nameLen});
            fieldsArrayVals.push_back(nameSlice);
        }

        llvm::Constant *fieldsSlice = llvm::Constant::getNullValue(llvmSliceSliceU8Type);
        if (!fieldsArrayVals.empty()) {
            llvm::ArrayType *fieldsArrayTy = llvm::ArrayType::get(llvmSliceU8Type, fieldsArrayVals.size());
            llvm::Constant *fieldsArrayInit = llvm::ConstantArray::get(fieldsArrayTy, fieldsArrayVals);
            llvm::GlobalVariable *fieldsArrayGV =
                new llvm::GlobalVariable(*m_module, fieldsArrayTy, true, llvm::GlobalValue::PrivateLinkage,
                                         fieldsArrayInit, globalName + ".fields");
            llvm::Constant *fieldsLen = llvm::ConstantInt::get(sizeTy, fieldsArrayVals.size());
            fieldsSlice = llvm::ConstantStruct::get(llvmSliceSliceU8Type, {fieldsArrayGV, fieldsLen});
        }

        // Methods setup
        std::vector<llvm::Constant *> methodsArrayVals;
        for (auto &method : structDecl->functions_strs) {
            auto nameGlobal = create_global_string(method, "typeinfo.str");
            auto nameLen = llvm::ConstantInt::get(sizeTy, method.size());
            auto nameSlice = llvm::ConstantStruct::get(llvmSliceU8Type, {nameGlobal, nameLen});
            methodsArrayVals.push_back(nameSlice);
        }

        llvm::Constant *methodsSlice = llvm::Constant::getNullValue(llvmSliceSliceU8Type);
        if (!methodsArrayVals.empty()) {
            llvm::ArrayType *methodsArrayTy = llvm::ArrayType::get(llvmSliceU8Type, methodsArrayVals.size());
            llvm::Constant *methodsArrayInit = llvm::ConstantArray::get(methodsArrayTy, methodsArrayVals);
            llvm::GlobalVariable *methodsArrayGV =
                new llvm::GlobalVariable(*m_module, methodsArrayTy, true, llvm::GlobalValue::PrivateLinkage,
                                         methodsArrayInit, globalName + ".methods");
            llvm::Constant *methodsLen = llvm::ConstantInt::get(sizeTy, methodsArrayVals.size());
            methodsSlice = llvm::ConstantStruct::get(llvmSliceSliceU8Type, {methodsArrayGV, methodsLen});
        }

        payloadType =
            llvm::StructType::get(*m_context, {llvmSliceU8Type, llvmSliceSliceU8Type, llvmSliceSliceU8Type}, false);
        payload = llvm::ConstantStruct::get(static_cast<llvm::StructType *>(payloadType),
                                            {structNameSlice, fieldsSlice, methodsSlice});
    } else if (targetType->kind == ResolvedTypeKind::Simd) {
        auto simdType = static_cast<const ResolvedTypeSimd *>(targetType);

        auto elementType = simdType->to_str();
        auto elementNameGlobal = create_global_string(elementType, "typeinfo.simd.element.str");
        auto elementNameLen = llvm::ConstantInt::get(sizeTy, elementType.size());
        auto elementNameSlice = llvm::ConstantStruct::get(llvmSliceU8Type, {elementNameGlobal, elementNameLen});

        auto simdLen = m_builder.getInt32(simdType->simdSize);

        payloadType = llvm::StructType::get(*m_context, {llvmSliceU8Type, m_builder.getInt32Ty()}, false);
        payload = llvm::ConstantStruct::get(static_cast<llvm::StructType *>(payloadType), {elementNameSlice, simdLen});
    }

    // Construct the union initializer
    llvm::Constant *unionInit = nullptr;
    llvm::Type *tagTy = generate_type(*unionDecl->tag->type, true);
    unsigned tagBitSize = dl.getTypeSizeInBits(tagTy);
    if (payload) {
        std::vector<llvm::Type *> unionFields = {tagTy, payloadType};
        auto *unionTmpType = llvm::StructType::get(*m_context, unionFields);
        std::vector<llvm::Constant *> unionVals = {m_builder.getIntN(tagBitSize, tag), payload};
        unionInit = llvm::ConstantStruct::get(unionTmpType, unionVals);
    } else {
        std::vector<llvm::Type *> unionFields = {tagTy, llvm::ArrayType::get(m_builder.getInt8Ty(), maxSize)};
        auto *unionTmpType = llvm::StructType::get(*m_context, unionFields);
        std::vector<llvm::Constant *> unionVals = {
            m_builder.getIntN(tagBitSize, tag),
            llvm::ConstantAggregateZero::get(llvm::ArrayType::get(m_builder.getInt8Ty(), maxSize))};
        unionInit = llvm::ConstantStruct::get(unionTmpType, unionVals);
    }

    auto global = new llvm::GlobalVariable(*m_module, unionInit->getType(), true, llvm::GlobalValue::PrivateLinkage,
                                           unionInit, globalName);
    return global;
}

llvm::Value *Codegen::generate_builtin_simdsplat(const ResolvedCallExpr &callExpr) {
    debug_func(callExpr.location);
    llvm::Value *val = generate_expr(*callExpr.arguments[0]);
    auto vecType = dynamic_cast<const ResolvedTypeSimd *>(callExpr.type.get());
    if (!vecType) {
        dmz_unreachable(callExpr.location, "unexpected type in simdsplat");
    }
    val = cast_to(val, *callExpr.arguments[0]->type, *vecType->simdType);
    return m_builder.CreateVectorSplat(vecType->simdSize, val);
}

llvm::Value *Codegen::generate_builtin_simdiota(const ResolvedCallExpr &callExpr) {
    debug_func(callExpr.location);
    auto vecType = dynamic_cast<const ResolvedTypeSimd *>(callExpr.type.get());
    if (!vecType) {
        dmz_unreachable(callExpr.location, "unexpected type in simdiota");
    }
    auto numType = dynamic_cast<const ResolvedTypeNumber *>(vecType->simdType.get());
    if (!numType) {
        dmz_unreachable(callExpr.location, "unexpected type in simdiota");
    }
    auto llvmVecType = generate_type(*callExpr.type);
    auto llvmNumType = generate_type(*numType);
    llvm::Value *vec = llvm::UndefValue::get(llvmVecType);
    for (int i = 0; i < vecType->simdSize; i++) {
        llvm::Value *val;
        if (numType->numberKind == ResolvedNumberKind::Float) {
            val = llvm::ConstantFP::get(llvmNumType, i);
        } else {
            val = llvm::ConstantInt::get(llvmNumType, i);
        }
        vec = m_builder.CreateInsertElement(vec, val, i);
    }
    return vec;
}

std::vector<const ResolvedTestDecl *> Codegen::get_tests() {
    if (cached_tests.empty()) {
        auto sourcePath = m_module->getSourceFileName();
        for (auto &decl : m_resolvedTree) {
            if (auto *moduleDecl = dynamic_cast<ResolvedModuleDecl *>(decl.get())) {
                bool isRoot = (moduleDecl->module_path == sourcePath);
                bool inTestDir = !m_testDir.empty() && [&]() {
                    auto rel = std::filesystem::relative(moduleDecl->module_path, m_testDir);
                    return !rel.empty() && !rel.string().starts_with("..");
                }();
                if (!isRoot && !inTestDir) continue;
                for (auto &modDecl : moduleDecl->declarations) {
                    if (auto testDecl = dynamic_cast<ResolvedTestDecl *>(modDecl.get())) {
                        cached_tests.emplace_back(testDecl);
                    }
                }
            }
        }
    }
    return cached_tests;
}

llvm::Value *Codegen::generate_builtin_testnum(const ResolvedCallExpr &callExpr) {
    if (auto constVal = callExpr.get_constant_value()) {
        return llvm::ConstantInt::get(generate_type(*callExpr.type), constVal->getInt());
    } else {
        auto tests = get_tests();
        return llvm::ConstantInt::get(generate_type(*callExpr.type), tests.size());
    }
}

llvm::Value *Codegen::generate_builtin_testname(const ResolvedCallExpr &callExpr) {
    debug_func(callExpr.location);
    if (callExpr.arguments.empty()) dmz_unreachable(callExpr.location, "expected argument");
    if (auto testNum = callExpr.arguments[0]->get_constant_value()) {
        auto tests = get_tests();
        auto ptr = create_global_string(tests[testNum->getInt()]->identifier,
                                        "test.name." + std::to_string(testNum->getInt()));
        auto slice = allocate_stack_variable(callExpr.location, "test.name.slice." + std::to_string(testNum->getInt()),
                                             *callExpr.type);
        auto sliceType = generate_type(*callExpr.type);
        llvm::Value *slice_value = llvm::UndefValue::get(sliceType);
        slice_value = m_builder.CreateInsertValue(slice_value, ptr, 0);
        auto length = llvm::ConstantInt::get(m_builder.getIntPtrTy(m_module->getDataLayout()),
                                             tests[testNum->getInt()]->identifier.size());
        slice_value = m_builder.CreateInsertValue(slice_value, length, 1);
        m_builder.CreateStore(slice_value, slice);
        return slice;
    } else {
        dmz_unreachable(callExpr.location, "callExpr not constant value this should not happend");
    }
}

llvm::Value *Codegen::generate_builtin_testrun(const ResolvedCallExpr &callExpr) {
    debug_func(callExpr.location);
    if (callExpr.arguments.empty()) dmz_unreachable(callExpr.location, "expected argument");
    if (auto testNum = callExpr.arguments[0]->get_constant_value()) {
        auto tests = get_tests();
        auto test = tests[testNum->getInt()];
        auto ref = makePtr<ResolvedDeclRefExpr>(callExpr.location, test->identifier, *test, test->type->clone());
        ResolvedCallExpr call(callExpr.location, callExpr.type->clone(), std::move(ref), {});
        return generate_call_expr(call);
    } else {
        dmz_unreachable(callExpr.location, "callExpr not constant value this should not happend");
    }
}
llvm::Value *Codegen::generate_builtin_simdLoad(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.empty()) dmz_unreachable(call.location, "expected argument");
    llvm::Value *ptr = generate_expr(*call.arguments[0]);
    auto *simdType = dynamic_cast<const ResolvedTypeSimd *>(call.type.get());
    llvm::Type *llvmSimdType = generate_type(*simdType);
    auto load = m_builder.CreateLoad(llvmSimdType, ptr, "simd.load");
    load->setAlignment(llvm::Align(1));
    return load;
}

llvm::Value *Codegen::generate_builtin_simdStore(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.size() != 2) dmz_unreachable(call.location, "expected 2 arguments");
    llvm::Value *destPtr = generate_expr(*call.arguments[1]);
    llvm::Value *simdVal = generate_expr(*call.arguments[0]);
    auto store = m_builder.CreateStore(simdVal, destPtr);
    store->setAlignment(llvm::Align(1));
    return nullptr;
}

llvm::Value *Codegen::generate_builtin_simdSelect(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.size() != 3) dmz_unreachable(call.location, "expected 3 arguments");
    llvm::Value *vecA = generate_expr(*call.arguments[0]);
    llvm::Value *vecB = generate_expr(*call.arguments[1]);
    llvm::Value *mask = generate_expr(*call.arguments[2]);
    return m_builder.CreateSelect(mask, vecA, vecB);
}

llvm::Value *Codegen::generate_builtin_simdReduce(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.size() != 2) dmz_unreachable(call.location, "expected 2 arguments");
    llvm::Value *simdVal = generate_expr(*call.arguments[0]);
    auto constValOpt = call.arguments[1]->get_constant_value();
    int opVal = constValOpt.has_value() ? constValOpt->getInt() : -1;
    if (opVal < 0) {
        dmz_unreachable(call.location, "@simdReduce: operator is not a constant");
    }

    auto simdType = dynamic_cast<const ResolvedTypeSimd *>(call.arguments[0]->type.get());
    if (!simdType) dmz_unreachable(call.location, "@simdReduce: element type is not a simd");
    auto elementType = simdType->simdType.get();
    auto numType = dynamic_cast<const ResolvedTypeNumber *>(elementType);
    if (!numType) dmz_unreachable(call.location, "@simdReduce: element type is not a number");

    bool isFloat = numType->numberKind == ResolvedNumberKind::Float;
    bool isSigned = numType->numberKind == ResolvedNumberKind::Int;

    switch (opVal) {
        case 0:  // .Add
            if (isFloat) {
                llvm::Type *scalarTy = generate_type(*numType);
                return m_builder.CreateFAddReduce(llvm::ConstantFP::get(scalarTy, 0.0), simdVal);
            } else {
                return m_builder.CreateAddReduce(simdVal);
            }
        case 2:  // .Mul
            if (isFloat) {
                llvm::Type *scalarTy = generate_type(*numType);
                return m_builder.CreateFMulReduce(llvm::ConstantFP::get(scalarTy, 1.0), simdVal);
            } else {
                return m_builder.CreateMulReduce(simdVal);
            }
        case 3:  // .Min
            if (isFloat) {
                return m_builder.CreateFPMinReduce(simdVal);
            } else {
                return m_builder.CreateIntMinReduce(simdVal, isSigned);
            }
        case 4:  // .Max
            if (isFloat) {
                return m_builder.CreateFPMaxReduce(simdVal);
            } else {
                return m_builder.CreateIntMaxReduce(simdVal, isSigned);
            }
        case 5:  // .And
            return m_builder.CreateAndReduce(simdVal);
        case 6:  // .Or
            return m_builder.CreateOrReduce(simdVal);
        case 7:  // .Xor
            return m_builder.CreateXorReduce(simdVal);
        default:
            dmz_unreachable(call.location, "unsupported simd reduction operator");
    }
}

llvm::Value *Codegen::generate_builtin_simdShuffle(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.size() != 3) dmz_unreachable(call.location, "expected 3 arguments");
    llvm::Value *v1 = generate_expr(*call.arguments[0]);
    llvm::Value *v2 = generate_expr(*call.arguments[1]);

    auto maskComptime = call.arguments[2]->get_constant_value();
    if (!maskComptime || !maskComptime->isArray()) {
        dmz_unreachable(call.location, "@simdShuffle mask must be a compile-time constant array");
    }

    const auto &maskElements = maskComptime->getArray().elements;
    std::vector<int> indices;
    indices.reserve(maskElements.size());
    for (const auto &elem : maskElements) {
        if (!elem.isInt()) dmz_unreachable(call.location, "@simdShuffle mask element is not an integer");
        indices.push_back(elem.getInt());
    }

    return m_builder.CreateShuffleVector(v1, v2, indices);
}

llvm::Value *Codegen::generate_builtin_ptrCast(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.empty()) dmz_unreachable(call.location, "@ptrCast expects 1 argument");
    return generate_expr(*call.arguments[0]);
}

llvm::Value *Codegen::generate_builtin_intCast(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.empty()) dmz_unreachable(call.location, "@intCast expects 1 argument");
    auto val = generate_expr(*call.arguments[0]);
    return cast_to(val, *call.arguments[0]->type, *call.type);
}

llvm::Value *Codegen::generate_builtin_floatCast(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.empty()) dmz_unreachable(call.location, "@floatCast expects 1 argument");
    auto val = generate_expr(*call.arguments[0]);
    return cast_to(val, *call.arguments[0]->type, *call.type);
}

llvm::Value *Codegen::generate_builtin_bitCast(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.empty()) dmz_unreachable(call.location, "@bitCast expects 1 argument");
    auto val = generate_expr(*call.arguments[0]);
    auto targetLLVMType = generate_type(*call.type);
    return m_builder.CreateBitCast(val, targetLLVMType);
}

llvm::Value *Codegen::generate_builtin_asm(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.size() < 2) dmz_unreachable(call.location, "expected at least 2 arguments");

    auto *asmCodeLit = dynamic_cast<const ResolvedStringLiteral *>(call.arguments[0].get());
    auto *constraintsLit = dynamic_cast<const ResolvedStringLiteral *>(call.arguments[1].get());

    if (!asmCodeLit || !constraintsLit) {
        dmz_unreachable(call.location, "@asm first two arguments must be string literals");
    }

    llvm::Type *retType = generate_type(*call.type);
    std::vector<llvm::Type *> paramTypes;
    std::vector<llvm::Value *> args;

    for (size_t i = 2; i < call.arguments.size(); ++i) {
        auto *val = generate_expr(*call.arguments[i]);
        paramTypes.emplace_back(val->getType());
        args.emplace_back(val);
    }

    llvm::FunctionType *asmFnType = llvm::FunctionType::get(retType, paramTypes, false);

    bool hasSideEffects = true;
    bool isAlignStack = false;
    auto dialect = llvm::InlineAsm::AD_ATT;

    llvm::InlineAsm *ia = llvm::InlineAsm::get(asmFnType, asmCodeLit->value, constraintsLit->value, hasSideEffects,
                                               isAlignStack, dialect);

    return m_builder.CreateCall(asmFnType, ia, args);
}

llvm::Value *Codegen::generate_builtin_sqrt(const ResolvedCallExpr &call) {
    debug_func(call.location);
    if (call.arguments.empty()) dmz_unreachable(call.location, "@sqrt expects 1 argument");
    auto val = generate_expr(*call.arguments[0]);
    auto SqrtIntrin = llvm::Intrinsic::getOrInsertDeclaration(m_module.get(), llvm::Intrinsic::sqrt, val->getType());
    return m_builder.CreateCall(SqrtIntrin, val, "sqrt");
}
}  // namespace DMZ
