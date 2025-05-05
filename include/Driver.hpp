#pragma once

#include <filesystem>

namespace C {
struct CompilerOptions {
    std::filesystem::path source;
    std::filesystem::path output;
    bool displayHelp = false;
    bool lexerDump = false;
    bool astDump = false;
    bool resDump = false;
    bool llvmDump = false;
    bool cfgDump = false;
};
}  // namespace C
