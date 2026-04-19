#pragma once

#include "DMZPCH.hpp"
#include "lexer/Lexer.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ {
struct CompilerOptions {
    std::filesystem::path source;
    std::unordered_map<std::string, std::filesystem::path> imports;
    std::filesystem::path output;
    std::string optimizationLevel = "-O0";
    bool displayHelp = false;
    bool lexerDump = false;
    bool astDump = false;
    bool importDump = false;
    bool noRemoveUnused = false;
    bool resDump = false;
    bool depsDump = false;
    bool depsDotDump = false;
    bool llvmDump = false;
    bool asmDump = false;
    bool cfgDump = false;
    bool fmtDump = false;
    bool run = false;
    bool debugSymbols = false;
    bool fmt = false;
    bool test = false;
    bool testCompiler = false;
    bool isModule = false;
    bool printStats = false;
    bool quiet = false;
    bool lsp = false;
    int parallelJobs = 1;

    static CompilerOptions parse_arguments(int argc, char** argv);
};

class Driver {
    ThreadPool m_workers;
    std::mutex m_modulesMutex;
    std::vector<ptr<llvm::Module>> modules;
    std::atomic_bool m_haveError = {false};
    std::atomic_bool m_haveNormalExit = {false};

   public:
    std::unordered_map<std::filesystem::path, ptr<ModuleDecl>> imported_modules;

   public:
    CompilerOptions m_options;
    Driver(CompilerOptions options);
    ~Driver();
    int main();
    void display_help();

    bool need_exit();
    int exit_code();

    void check_sources_pass(std::filesystem::path& source);
    ptr<Lexer> lexer_pass(std::filesystem::path& source);
    ptr<ModuleDecl> parser_pass(ptr<Lexer> lexers);

    void fmt_pass(ptr<ModuleDecl> asts);
    void import_pass(ptr<ModuleDecl>& asts);
    std::pair<std::string, std::filesystem::path> register_import(SourceLocation location,
                                                                  const std::filesystem::path& source,
                                                                  std::string_view imported);

    std::vector<ptr<ResolvedModuleDecl>> semantic_pass(ptr<ModuleDecl> ast);
    std::pair<ptr<llvm::LLVMContext>, ptr<llvm::Module>> codegen_pass(
        std::vector<ptr<ResolvedModuleDecl>> resolvedTrees);
    int asm_pass(ptr<llvm::Module>& module);
    int jit_pass(ptr<llvm::LLVMContext>& context, ptr<llvm::Module>& module);
    int generate_exec_pass(ptr<llvm::Module>& module);
};
}  // namespace DMZ
