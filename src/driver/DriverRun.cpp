#include "Debug.hpp"
#include "Stats.hpp"
#include "driver/Driver.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#pragma GCC diagnostic pop

namespace DMZ {

int Driver::jit_pass(ptr<llvm::LLVMContext> &context, ptr<llvm::Module> &module) {
    debug_func("");

    std::string errorMessage;
    llvm::raw_string_ostream os(errorMessage);
    {
        ScopedTimer(StatType::Verify);
        debug_msg("Verifying module");
        if (llvm::verifyModule(*module, &os)) {
            llvm::errs() << "Invalid IR detected before JIT\n";
            llvm::errs() << os.str();
            return 1;
        }
        debug_msg("Module verified");
    }
    ptr<llvm::orc::LLJIT> JIT;
    int (*MainPtr)(int, char **) = nullptr;
    {
        ScopedTimer(StatType::Compile);
        debug_msg("Initializing native target");
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        auto expectedJIT =
            llvm::orc::LLJITBuilder()
                .setObjectLinkingLayerCreator(
                    [&](llvm::orc::ExecutionSession &ES, [[maybe_unused]] const llvm::Triple &TT) {
                        auto GetMemMgr = []() { return std::make_unique<llvm::SectionMemoryManager>(); };
                        auto ObjLayer = std::make_unique<llvm::orc::RTDyldObjectLinkingLayer>(ES, std::move(GetMemMgr));

                        auto *GDBListener = llvm::JITEventListener::createGDBRegistrationListener();
                        ObjLayer->registerJITEventListener(*GDBListener);

                        ObjLayer->setProcessAllSections(true);

                        return ObjLayer;
                    })
                .create();
        if (auto Err = expectedJIT.takeError()) {
            llvm::errs() << "Error creating JIT: " << std::move(Err) << "\n";
            return 1;
        }
        JIT = std::move(*expectedJIT);
        auto TSM = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));
        if (auto Err = JIT->addIRModule(std::move(TSM))) {
            llvm::errs() << "Error adding module: " << std::move(Err) << "\n";
            return 1;
        }

        auto MainSym = JIT->lookup("main");
        debug_msg("Searching main symbol " << MainSym->getValue());
        if (!MainSym) {
            llvm::errs() << "No find function 'main': " << MainSym.takeError() << "\n";
            return 1;
        }
        MainPtr = MainSym->toPtr<int (*)(int, char **)>();
        if (!MainPtr) {
            llvm::errs() << "Null function pointer to 'main'\n";
            return 1;
        }
    }
    int result;
    {
        ScopedTimer(StatType::Run);
        debug_msg("Executing main function");
        result = MainPtr(0, nullptr);
        debug_msg("Main function executed");
    }
    return result;
}
}  // namespace DMZ