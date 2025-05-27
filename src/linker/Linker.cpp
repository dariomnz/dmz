#include "linker/Linker.hpp"

#include <iostream>

#include "Debug.hpp"

namespace DMZ {
Linker::Linker(std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>> modules) : m_modules(std::move(modules)) {}

std::unique_ptr<llvm::orc::ThreadSafeModule> Linker::link_modules() {
    if (m_modules.size() == 1) return std::move(m_modules[0]);

    llvm::Linker linker(*m_modules[0]->getModuleUnlocked());

    std::vector<std::unique_ptr<llvm::Module>> modulesPtrs;
    for (size_t i = 1; i < m_modules.size(); i++) {
        m_modules[i]->consumingModuleDo([&](auto M) {
            // println("--------------------------------------");
            // M->dump();
            // println("--------------------------------------");
            linker.linkInModule(std::move(M), llvm::Linker::Flags::LinkOnlyNeeded);
        });
    }

    return std::move(m_modules[0]);
    // return std::make_unique<llvm::orc::ThreadSafeModule>(std::move(modulesPtrs[0]), m_modules[0]->getContext());
}
}  // namespace DMZ
