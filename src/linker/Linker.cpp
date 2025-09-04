#include "linker/Linker.hpp"

namespace DMZ {
Linker::Linker(std::vector<ptr<llvm::orc::ThreadSafeModule>> modules) : m_modules(std::move(modules)) {}

ptr<llvm::orc::ThreadSafeModule> Linker::link_modules() {
    if (m_modules.size() == 1) return std::move(m_modules[0]);

    llvm::Linker linker(*m_modules[0]->getModuleUnlocked());

    std::vector<ptr<llvm::Module>> modulesPtrs;
    for (size_t i = 1; i < m_modules.size(); i++) {
        m_modules[i]->consumingModuleDo([&](auto M) {
            // println("--------------------------------------");
            // M->dump();
            // println("--------------------------------------");
            linker.linkInModule(std::move(M), llvm::Linker::Flags::LinkOnlyNeeded);
        });
    }

    return std::move(m_modules[0]);
    // return makePtr<llvm::orc::ThreadSafeModule>(std::move(modulesPtrs[0]), m_modules[0]->getContext());
}
}  // namespace DMZ
