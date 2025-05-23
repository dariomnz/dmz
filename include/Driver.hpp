#pragma once

#include <filesystem>
#include <vector>

namespace DMZ {
struct CompilerOptions {
    std::vector<std::filesystem::path> sources;
    std::filesystem::path output;
    bool displayHelp = false;
    bool lexerDump = false;
    bool astDump = false;
    bool resDump = false;
    bool llvmDump = false;
    bool cfgDump = false;
    bool run = false;
    bool isModule = false;
    bool printStats = false;
};
}  // namespace DMZ
