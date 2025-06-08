#pragma once

#include "DMZPCH.hpp"
#include "codegen/Codegen.hpp"
#include "lexer/Lexer.hpp"
#include "linker/Linker.hpp"
#include "parser/Parser.hpp"
#include "semantic/CFG.hpp"
#include "semantic/Semantic.hpp"

namespace DMZ {
struct CompilerOptions {
    std::vector<std::filesystem::path> sources;
    std::vector<std::filesystem::path> includes;
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

    static CompilerOptions parse_arguments(int argc, char** argv);
};

class Driver {
    ThreadPool m_workers;
    std::mutex m_modulesMutex;
    std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>> modules;
    std::atomic_bool m_haveError = {false};
    std::atomic_bool m_haveNormalExit = {false};
    CompilerOptions m_options;

   public:
    Driver(CompilerOptions options) : m_options(options) {}
    int main();
    void display_help();

    void check_exit();

    using Type_Sources = std::vector<std::filesystem::path>;
    using Type_Lexers = std::vector<std::unique_ptr<Lexer>>;
    using Type_Asts = std::vector<std::vector<std::unique_ptr<Decl>>>;
    using Type_ResolvedTrees = std::vector<std::vector<std::unique_ptr<ResolvedDecl>>>;
    using Type_Modules = std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>>;
    using Type_Module = std::unique_ptr<llvm::orc::ThreadSafeModule>;

    void check_sources_pass(Type_Sources& sources);
    Type_Lexers lexer_pass(Type_Sources& sources);
    Type_Asts parser_pass(Type_Lexers& lexers, bool expectMain = true);
    Type_Sources find_modules(const Type_Sources& includeDirs,
                              const std::unordered_set<std::string_view>& importedModuleIDs);
    void include_pass(Type_Lexers& lexers, Type_Asts& asts);
    Type_ResolvedTrees semantic_pass(Type_Asts& asts);
    Type_Modules codegen_pass(Type_ResolvedTrees& resolvedTrees);
    Type_Module linker_pass(Type_Modules& modules);
    int jit_pass(Type_Module& module);
    int generate_exec_pass(Type_Module& module);
};
}  // namespace DMZ
