#include "Debug.hpp"
#include "Stats.hpp"
#include "driver/Driver.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#pragma GCC diagnostic pop

namespace DMZ {

int Driver::jit_pass(ptr<llvm::LLVMContext> &context, ptr<llvm::Module> &module) {
    debug_func("Ejecutando JIT interno...");
    ScopedTimer(StatType::Run);

    std::string errorMessage;
    llvm::raw_string_ostream os(errorMessage);

    if (llvm::verifyModule(*module, &os)) {
        llvm::errs() << "Invalid IR detected before JIT\n";
        llvm::errs() << os.str();
        return 1;
    }

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto JIT = llvm::orc::LLJITBuilder()
                   .setObjectLinkingLayerCreator([&](llvm::orc::ExecutionSession &ES,
                                                     [[maybe_unused]] const llvm::Triple &TT) {
                       auto GetMemMgr = []() { return std::make_unique<llvm::SectionMemoryManager>(); };
                       auto ObjLayer = std::make_unique<llvm::orc::RTDyldObjectLinkingLayer>(ES, std::move(GetMemMgr));

                       auto *GDBListener = llvm::JITEventListener::createGDBRegistrationListener();
                       ObjLayer->registerJITEventListener(*GDBListener);

                       ObjLayer->setProcessAllSections(true);

                       return ObjLayer;
                   })
                   .create();

    auto TSM = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));
    if (auto Err = (*JIT)->addIRModule(std::move(TSM))) {
        llvm::errs() << "Error adding module: " << std::move(Err) << "\n";
        return 1;
    }

    auto MainSym = (*JIT)->lookup("main");
    if (!MainSym) {
        llvm::errs() << "No find function 'main': " << MainSym.takeError() << "\n";
        return 1;
    }

    auto *MainPtr = MainSym->toPtr<int (*)(int, char **)>();

    int result = MainPtr(0, nullptr);
    return result;
}
}  // namespace DMZ