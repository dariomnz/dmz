#include "linker/Linker.hpp"

#include <iostream>

namespace DMZ {
Linker::Linker(std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>> modules) : m_modules(std::move(modules)) {}

std::unique_ptr<llvm::orc::ThreadSafeModule> Linker::link_modules() {
    if (m_modules.size() == 1) return std::move(m_modules[0]);

    std::vector<std::unique_ptr<llvm::Module>> modulesPtrs;
    for (auto &&module : m_modules) {
        module->consumingModuleDo([&](auto M){ modulesPtrs.emplace_back(std::move(M));});
    }

    for (size_t i = 1; i < modulesPtrs.size(); i++)
    {
        llvm::Linker::linkModules(*modulesPtrs[0], std::move(modulesPtrs[i]), llvm::Linker::Flags::LinkOnlyNeeded);
    }

    return std::make_unique<llvm::orc::ThreadSafeModule>(std::move(modulesPtrs[0]), m_modules[0]->getContext());
}
}  // namespace DMZ
