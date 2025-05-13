#pragma once

#include <filesystem>

namespace DMZ {
struct CompilerOptions {
    std::filesystem::path source;
    std::filesystem::path output;
    bool displayHelp = false;
    bool lexerDump = false;
    bool astDump = false;
    bool resDump = false;
    bool llvmDump = false;
    bool cfgDump = false;
    bool run = false;
    bool printStats = false;
};
}  // namespace DMZ
