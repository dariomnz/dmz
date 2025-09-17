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
    bool run = false;
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

    using Type_Source = std::filesystem::path;
    using Type_Lexers = std::vector<ptr<Lexer>>;
    using Type_Ast = std::vector<ptr<ModuleDecl>>;
    using Type_ResolvedTree = std::vector<ptr<ResolvedDecl>>;
    using Type_Module = ptr<llvm::orc::ThreadSafeModule>;

    void check_sources_pass(Type_Source& source);
    Type_Lexers lexer_pass(Type_Source& source);
    Type_Ast parser_pass(Type_Lexers& lexers, bool expectMain = true);
    // Type_Sources find_modules(const Type_Sources& includeDirs,
    //                           const std::unordered_set<std::string_view>& importedModuleIDs);
    void import_pass(Type_Ast& asts);
    // ptr<ModuleDecl> merge_modules(std::vector<ptr<ModuleDecl>> modules);
    static std::pair<std::string, std::filesystem::path> register_import(const std::filesystem::path& source,
                                                                         std::string_view imported);
    // static imported_module* get_import(std::string_view imported);
    // bool all_imported();
    // ModuleDecl* find_module(std::string_view name, ptr<ModuleDecl>& find_ast);

    Type_ResolvedTree semantic_pass(Type_Ast& asts);
    Type_Module codegen_pass(Type_ResolvedTree& resolvedTrees);
    // Type_Module linker_pass(Type_Module& modules);
    int jit_pass(Type_Module& module);
    int generate_exec_pass(Type_Module& module);

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
