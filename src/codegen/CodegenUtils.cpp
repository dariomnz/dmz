
#ifdef DEBUG_CODEGEN
#ifndef DEBUG
#define DEBUG
#endif
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#pragma GCC diagnostic pop

#include "Debug.hpp"
#include "codegen/Codegen.hpp"
#include "codegen/CodegenUtils.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

int CodegenUtils::ptrBitSize() {
    static int ptrSize = -1;
    if (ptrSize >= 0) {
        return ptrSize;
    }
    llvm::LLVMContext context;
    llvm::Module module("tmp", context);
    ptrSize = module.getDataLayout().getPointerSizeInBits();
    return ptrSize;
}

int CodegenUtils::typeBitSize(const ResolvedType &type) {
    if (type.is_generic()) {
        return ptrBitSize();
    }
    Codegen codegen(std::vector<ptr<ResolvedModuleDecl>>{}, "", false, true, false);
    llvm::Type *llvmType = codegen.generate_type(type, true);
    if (!llvmType) dmz_unreachable(type.location, "type is null");
    return codegen.m_module->getDataLayout().getTypeSizeInBits(llvmType);
}

int CodegenUtils::target_simd_size() {
    static int simdSize = -1;
    if (simdSize >= 0) {
        return simdSize;
    }

    llvm::InitializeNativeTarget();
    std::string TripleStr = llvm::sys::getDefaultTargetTriple();
    std::string Error;
    const llvm::Target *Target = llvm::TargetRegistry::lookupTarget(TripleStr, Error);

    // 3. Crear el TargetMachine (aquí es donde defines CPU y Features)
    llvm::TargetOptions opt;
    auto RM = std::optional<llvm::Reloc::Model>();
    std::string CPU = llvm::sys::getHostCPUName().str();
    std::string Features;
    auto allFeatures = llvm::sys::getHostCPUFeatures();
    for (auto &feature : allFeatures) {
        if (feature.second) {
            Features += "+" + feature.first().str() + ",";
        }
    }
    llvm::TargetMachine *TM = Target->createTargetMachine(TripleStr, CPU, Features, opt, RM);
    llvm::LLVMContext ctx;
    llvm::Module mod("tmp", ctx);
    llvm::FunctionType *FTy = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false);
    llvm::Function *TempF = llvm::Function::Create(FTy, llvm::Function::ExternalLinkage, "__temp_tti", &mod);
    llvm::TargetTransformInfo TTI = TM->getTargetTransformInfo(*TempF);

    simdSize = TTI.getRegisterBitWidth(llvm::TargetTransformInfo::RGK_FixedWidthVector);

    debug_msg("El ancho de banda SIMD para '" << TripleStr << "' cpu: '" << CPU << "' features: '" << Features
                                              << "' es: " << simdSize << " bits");
    return simdSize;
}

}  // namespace DMZ
