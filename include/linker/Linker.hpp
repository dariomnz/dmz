#pragma once

#include "DMZPCHLLVM.hpp"

namespace DMZ {

class Linker {
    std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>> m_modules;

   public:
    Linker(std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>> modules);

    std::unique_ptr<llvm::orc::ThreadSafeModule> link_modules();
};
}  // namespace DMZ