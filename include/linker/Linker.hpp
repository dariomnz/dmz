#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Linker/Linker.h"
#pragma GCC diagnostic pop

namespace DMZ {

class Linker {
    std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>> m_modules;

   public:
    Linker(std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>> modules);

    std::unique_ptr<llvm::orc::ThreadSafeModule> link_modules();
};
}  // namespace DMZ