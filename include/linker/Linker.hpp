#pragma once

#include "DMZPCHLLVM.hpp"

namespace DMZ {

class Linker {
    std::vector<ptr<llvm::orc::ThreadSafeModule>> m_modules;

   public:
    Linker(std::vector<ptr<llvm::orc::ThreadSafeModule>> modules);

    ptr<llvm::orc::ThreadSafeModule> link_modules();
};
}  // namespace DMZ