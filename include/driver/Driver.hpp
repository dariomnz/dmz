#pragma once

#include "DMZPCH.hpp"
#include "Debug.hpp"
#include "codegen/Codegen.hpp"
#include "lexer/Lexer.hpp"
#include "linker/Linker.hpp"
#include "parser/Parser.hpp"
#include "parser/ParserSymbols.hpp"
#include "semantic/CFG.hpp"
#include "semantic/Semantic.hpp"

namespace DMZ {
struct CompilerOptions {
    std::filesystem::path source;
    std::unordered_map<std::string, std::filesystem::path> imports;
    std::filesystem::path output;
    bool displayHelp = false;
    bool lexerDump = false;
    bool astDump = false;
    bool importDump = false;
    bool noRemoveUnused = false;
    bool resDump = false;
    bool depsDump = false;
    bool llvmDump = false;
    bool cfgDump = false;
    bool fmtDump = false;
    bool run = false;
    bool fmt = false;
    bool test = false;
    bool isModule = false;
    bool printStats = false;

    static CompilerOptions parse_arguments(int argc, char** argv);
};

class Driver {
    ThreadPool m_workers;
    std::mutex m_modulesMutex;
    std::vector<ptr<llvm::orc::ThreadSafeModule>> modules;
    std::atomic_bool m_haveError = {false};
    std::atomic_bool m_haveNormalExit = {false};

   public:
    std::unordered_map<std::filesystem::path, ptr<ModuleDecl>> imported_modules;

   public:
    CompilerOptions m_options;
    Driver(CompilerOptions options) : m_options(options) {}
    int main();
    void display_help();

    bool need_exit();

    void check_sources_pass(std::filesystem::path& source);
    ptr<Lexer> lexer_pass(std::filesystem::path& source);
    ptr<ModuleDecl> parser_pass(ptr<Lexer> lexers);

    void fmt_pass(ptr<ModuleDecl> asts);
    void import_pass(ptr<ModuleDecl>& asts);
    static std::pair<std::string, std::filesystem::path> register_import(const std::filesystem::path& source,
                                                                         std::string_view imported);

    std::vector<ptr<ResolvedModuleDecl>> semantic_pass(ptr<ModuleDecl> ast);
    ptr<llvm::orc::ThreadSafeModule> codegen_pass(std::vector<ptr<ResolvedModuleDecl>> resolvedTrees);
    int jit_pass(ptr<llvm::orc::ThreadSafeModule>& module);
    int generate_exec_pass(ptr<llvm::orc::ThreadSafeModule>& module);

    int ptrBitSize();

   private:
    static ptr<Driver> driver_instance;

   public:
    static Driver& create_instance(CompilerOptions options) {
        if (driver_instance) dmz_unreachable("Driver instance already created");

        driver_instance = makePtr<Driver>(options);
        if (!driver_instance) dmz_unreachable("Driver instance not created");
        return *driver_instance;
    }

    static Driver& instance() {
        if (!driver_instance) dmz_unreachable("Driver instance not created");
        return *driver_instance;
    }
};
}  // namespace DMZ
