#ifdef DEBUG_CODEGEN
#ifndef DEBUG
#define DEBUG
#endif
#endif
#include "codegen/CodegenUtils.hpp"

#include "Debug.hpp"
#include "codegen/Codegen.hpp"
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
    llvm::LLVMContext context;
    llvm::Module module("tmp", context);
    llvm::Type *llvmType = Codegen(std::vector<ptr<ResolvedModuleDecl>>{}, "", false, true, false).generate_type(type);
    return module.getDataLayout().getTypeSizeInBits(llvmType);
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
