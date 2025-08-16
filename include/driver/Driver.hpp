#pragma once

#include "DMZPCH.hpp"
#include "Debug.hpp"
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
    bool importDump = false;
    bool noRemoveUnused = false;
    bool resDump = false;
    bool depsDump = false;
    bool llvmDump = false;
    bool cfgDump = false;
    bool run = false;
    bool test = false;
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
    std::unordered_map<std::string, ModuleDecl*> imported_modules;

   public:
    Driver(CompilerOptions options) : m_options(options) {}
    int main();
    void display_help();

    void check_exit();

    using Type_Sources = std::vector<std::filesystem::path>;
    using Type_Lexers = std::vector<std::unique_ptr<Lexer>>;
    using Type_Ast = std::vector<std::unique_ptr<ModuleDecl>>;
    using Type_ResolvedTree = std::vector<std::unique_ptr<ResolvedDecl>>;
    using Type_Module = std::unique_ptr<llvm::orc::ThreadSafeModule>;

    void check_sources_pass(Type_Sources& sources);
    Type_Lexers lexer_pass(Type_Sources& sources);
    Type_Ast parser_pass(Type_Lexers& lexers, bool expectMain = true);
    Type_Sources find_modules(const Type_Sources& includeDirs,
                              const std::unordered_set<std::string_view>& importedModuleIDs);
    void include_pass(Type_Ast& asts);
    std::unique_ptr<ModuleDecl> merge_modules(std::vector<std::unique_ptr<ModuleDecl>> modules);
    static void register_import(std::string_view imported);
    static ModuleDecl* get_import(std::string_view imported);
    bool all_imported();
    ModuleDecl* find_module(std::string_view name, std::unique_ptr<ModuleDecl>& find_ast);

    Type_ResolvedTree semantic_pass(Type_Ast& asts);
    Type_Module codegen_pass(Type_ResolvedTree& resolvedTrees);
    // Type_Module linker_pass(Type_Module& modules);
    int jit_pass(Type_Module& module);
    int generate_exec_pass(Type_Module& module);

   private:
    static std::unique_ptr<Driver> driver_instance;

   public:
    static Driver& create_instance(CompilerOptions options) {
        if (driver_instance) dmz_unreachable("Driver instance already created");

        driver_instance = std::make_unique<Driver>(options);
        if (!driver_instance) dmz_unreachable("Driver instance not created");
        return *driver_instance;
    }

    static Driver& instance() {
        if (!driver_instance) dmz_unreachable("Driver instance not created");
        return *driver_instance;
    }
};
}  // namespace DMZ
