// #define DEBUG
#include "driver/Driver.hpp"

namespace DMZ {

std::unique_ptr<Driver> Driver::driver_instance = nullptr;

void Driver::display_help() {
    println("Usage:");
    println("  compiler [options] <source_files...>\n");
    println("Options:");
    println("  -h, -help        display this message");
    println("  -I <dir path>    include <dir path> to search for modules");
    println("  -o <file>        write executable to <file>");
    println("  -lexer-dump      print the lexer dump");
    println("  -ast-dump        print the abstract syntax tree");
    println("  -import-dump     print the abstract syntax tree after import");
    println("  -res-dump        print the resolved syntax tree");
    println("  -cfg-dump        print the control flow graph");
    println("  -llvm-dump       print the llvm module");
    println("  -print-stats     print the time stats");
    println("  -module          compile a module to .o file");
    println("  -run             runs the program with lli (Just In Time)");
    println("  -test            runs the test with lli (Just In Time)");
}

CompilerOptions CompilerOptions::parse_arguments(int argc, char **argv) {
    CompilerOptions options;

    int idx = 1;
    while (idx < argc) {
        std::string_view arg = argv[idx];
        if (arg[0] != '-') {
            options.sources.emplace_back(arg);
        } else {
            if (arg == "-h" || arg == "--help") {
                options.displayHelp = true;
            } else if (arg == "-o") {
                options.output = ++idx >= argc ? "" : argv[idx];
            } else if (arg == "-I") {
                if (++idx < argc) options.includes.emplace_back(argv[idx]);
            } else if (arg == "-lexer-dump") {
                options.lexerDump = true;
            } else if (arg == "-ast-dump") {
                options.astDump = true;
            } else if (arg == "-import-dump") {
                options.importDump = true;
            } else if (arg == "-res-dump") {
                options.resDump = true;
            } else if (arg == "-llvm-dump") {
                options.llvmDump = true;
            } else if (arg == "-cfg-dump") {
                options.cfgDump = true;
            } else if (arg == "-run") {
                options.run = true;
            } else if (arg == "-test") {
                options.test = true;
            } else if (arg == "-module") {
                options.isModule = true;
            } else if (arg == "-print-stats") {
                options.printStats = true;
            } else {
                error("unexpected option '" + std::string(arg) + '\'');
            }
        }

        ++idx;
    }

    return options;
}

void Driver::check_exit() {
    if (m_haveError || m_haveNormalExit) exit(EXIT_SUCCESS);
}

void Driver::check_sources_pass(Type_Sources &sources) {
    if (sources.empty()) {
        error("no source files specified");
        m_haveError = true;
        return;
    }

    for (auto &&source : sources) {
        if (source.extension() != ".dmz") {
            error("unexpected source file extension '" + source.extension().string() + "'");
            m_haveError = true;
        }

        if (!std::filesystem::exists(source)) {
            error("failed to open '" + source.string() + '\'');
            m_haveError = true;
        }
    }
}

Driver::Type_Lexers Driver::lexer_pass(Type_Sources &sources) {
    std::vector<std::unique_ptr<Lexer>> lexers;
    lexers.resize(sources.size());
    for (size_t index = 0; index < sources.size(); index++) {
        debug_msg(sources[index]);
        // m_workers.submit([&, index]() {
        lexers[index] = std::make_unique<Lexer>(sources[index].c_str());
        //  });
    }

    // m_workers.wait();

    if (m_options.lexerDump) {
        for (auto &&lexer : lexers) {
            Token tok;
            do {
                tok = lexer->next_token();
                println(tok);
            } while (tok.type != TokenType::eof);
        }
        m_haveNormalExit = true;
    }
    return lexers;
}

Driver::Type_Ast Driver::parser_pass(Type_Lexers &lexers, bool expectMain) {
    Type_Ast asts;
    asts.reserve(lexers.size());
    for (size_t index = 0; index < lexers.size(); index++) {
        // m_workers.submit([&, index]() {
        Parser parser(*lexers[index]);
        bool needMain = expectMain && !m_options.isModule && !m_options.test && index == 0;
        auto [ast, success] = parser.parse_source_file(needMain);
        if (!success) m_haveError = true;

        asts.emplace_back(std::move(ast));
        // });
    }

    // m_workers.wait();

    // for (size_t i = 1; i < asts.size(); i++) {
    //     for (auto &&decl : asts[i]) {
    //         asts[0].emplace_back(std::move(decl));
    //     }
    // }

    // asts.resize(1);

    if (m_options.astDump) {
        for (auto &&ast : asts) {
            ast->dump();
        }
        m_haveNormalExit = true;
    }
    return asts;
}

void Driver::register_import(std::string_view imported) {
    auto &d = instance();
    d.imported_modules.emplace(imported, nullptr);
}

ModuleDecl *Driver::get_import(std::string_view imported) {
    auto &d = instance();
    auto it = d.imported_modules.find(std::string(imported));
    if (it == d.imported_modules.end()) {
        return nullptr;
    } else {
        return (*it).second;
    }
}

bool Driver::all_imported() {
    for (auto &&[k, v] : imported_modules) {
        if (v == nullptr) return false;
    }
    return true;
}

ModuleDecl *Driver::find_module(std::string_view name, std::unique_ptr<DMZ::ModuleDecl> &find_ast) {
    debug_msg("Find module: " << name);
    for (auto &&decl : find_ast->declarations) {
        debug_msg("Decl: " << decl->identifier);
        if (auto modDecl = dynamic_cast<ModuleDecl *>(decl.get())) {
            debug_msg("Encounter module: " << modDecl->identifier);
            if (name == modDecl->identifier) {
                return modDecl;
            }
        }
    }
    return nullptr;
};

void Driver::include_pass(Type_Ast &ast) {
    debug_func("");

    if (imported_modules.empty()) return;

    // TODO: make in a way that only import the necesary modules

    std::vector<std::unique_ptr<ModuleDecl>> imported;
    for (const auto &includeDir : m_options.includes) {
        if (!std::filesystem::exists(includeDir)) {
            std::cerr << "Warning: The include directory does not exist: " << includeDir << std::endl;
            continue;
        }
        if (!std::filesystem::is_directory(includeDir)) {
            std::cerr << "Warning: The include directory is not a directory: " << includeDir << std::endl;
            continue;
        }

        for (const auto &entry : std::filesystem::recursive_directory_iterator(includeDir)) {
            if (std::filesystem::is_regular_file(entry.status())) {
                if (entry.path().extension() == ".dmz") {
                    Lexer l(entry.path().c_str());
                    Parser p(l);
                    auto [import_ast, success] = p.parse_source_file(false);
                    if (!success) {
                        continue;
                    }
                    imported.emplace_back(std::move(import_ast));
                }
            }
        }
    }

    auto imported_module = merge_modules(std::move(imported));
    if (!imported_module) {
        report(SourceLocation{.file_name = "imported"}, "importing modules");
    } else {
        ast.emplace_back(std::move(imported_module));
    }

    //     std::unordered_set<std::filesystem::path> sources;
    //     // Insert already sources to not repeat
    //     for (auto &&source : m_options.sources) {
    //         debug_msg("Add to sources: " << source);
    //         sources.emplace(source);
    //     }

    //     size_t source_size = sources.size();
    //     do {
    //         if (all_imported()) break;
    // #ifdef DEBUG
    //         debug_msg("Current imported modules");
    //         for (auto &&[k, v] : imported_modules) {
    //             debug_msg("  Module " << k << " " << (v == nullptr ? "false" : "true"));
    //         }
    // #endif
    //         source_size = sources.size();
    //         for (const auto &includeDir : m_options.includes) {
    //             if (!std::filesystem::exists(includeDir)) {
    //                 std::cerr << "Warning: The include directory does not exist: " << includeDir << std::endl;
    //                 continue;
    //             }
    //             if (!std::filesystem::is_directory(includeDir)) {
    //                 std::cerr << "Warning: The include directory is not a directory: " << includeDir << std::endl;
    //                 continue;
    //             }

    //             for (const auto &entry : std::filesystem::recursive_directory_iterator(includeDir)) {
    //                 if (std::filesystem::is_regular_file(entry.status())) {
    //                     if (entry.path().extension() == ".dmz") {
    //                         debug_msg("Parsing: " << entry.path());
    //                         Lexer l(entry.path().c_str());
    //                         Parser p(l);
    //                         std::unordered_map<std::string, DMZ::ModuleDecl *> saved_imported_modules;
    //                         saved_imported_modules.swap(imported_modules);

    //                         auto [import_ast, success] = p.parse_source_file(false);
    //                         if (!success) {
    //                             imported_modules.swap(saved_imported_modules);
    //                             continue;
    //                         }

    //                         bool found = false;
    //                         for (auto &&[k, v] : saved_imported_modules) {
    //                             if (v == nullptr) {
    //                                 debug_msg("Need module: " << k);
    //                                 if (auto modDecl = find_module(k, import_ast)) {
    //                                     debug_msg("Add to sources: " << entry.path());
    //                                     sources.emplace(entry.path());
    //                                     debug_msg("Save ast");
    //                                     ast.emplace_back(std::move(import_ast));
    //                                     v = modDecl;
    //                                     found = true;
    //                                     break;
    //                                 }
    //                             }
    //                         }
    //                         imported_modules.swap(saved_imported_modules);
    //                         if (found) {
    //                             for (auto &&[k, v] : saved_imported_modules) {
    //                                 imported_modules.emplace(k, v);
    //                             }
    //                         }
    //                     }
    //                 }
    //                 if (all_imported()) goto exit_while;
    //             }
    //         }

    //         // println("asdf");
    //         // prev_size = sources.size();
    //         // moduleIDs.clear();

    //         // for (auto &&ast : asts) {
    //         //     for (auto &&decl : ast) {
    //         //         if (auto importDecl = dynamic_cast<ImportDecl *>(decl.get())) {
    //         //             std::string moduleID = importDecl->get_moduleID();
    //         //             if (importedModules.find(moduleID) != importedModules.end()) continue;
    //         //             auto moduleIDEmplaced = importedModules.emplace(moduleID);
    //         //             if (moduleIDEmplaced.second) moduleIDs.emplace(*moduleIDEmplaced.first);
    //         //         }
    //         //     }
    //         // }

    //         // auto sources = find_modules(m_options.includes, moduleIDs);
    //         // for (auto &&path : sources) {
    //         //     newSources.emplace(path);
    //         // }

    //         // if (prev_size == newSources.size()) break;

    //         // for (auto &&path : sources) {
    //         //     m_options.sources.emplace_back(path);
    //         // }
    //         // auto newLexers = lexer_pass(sources);
    //         // auto newAsts = parser_pass(newLexers, false);

    //         // for (auto &&lexer : newLexers) {
    //         //     lexers.emplace_back(std::move(lexer));
    //         // }
    //         // for (auto &&ast : newAsts) {
    //         //     asts.emplace_back(std::move(ast));
    //         // }
    //     } while (!all_imported() && source_size != sources.size());
    // exit_while:

    // #ifdef DEBUG
    //     debug_msg("Current imported modules");
    //     for (auto &&[k, v] : imported_modules) {
    //         debug_msg("  Module " << k << " " << (v == nullptr ? "false" : "true"));
    //         if (v) {
    //             v->dump();
    //         }
    //     }
    // #endif
    if (m_options.importDump) {
        for (auto &&decl : ast) {
            decl->dump();
        }
        m_haveNormalExit = true;
    }
    return;
}

bool merge_module_decls(std::vector<std::unique_ptr<Decl>> &decls1, std::vector<std::unique_ptr<Decl>> &decls2) {
    debug_func("");
    bool error = false;
    for (auto &&decl2 : decls2) {
        auto it = std::find_if(decls1.begin(), decls1.end(),
                               [&decl2](std::unique_ptr<Decl> &d) { return decl2->identifier == d->identifier; });

        if (it == decls1.end()) {
            // Not found
            decls1.emplace_back(std::move(decl2));
            continue;
        } else {
            // Found
            if (auto modDecl2 = dynamic_cast<ModuleDecl *>(decl2.get())) {
                if (auto modDecl1 = dynamic_cast<ModuleDecl *>((*it).get())) {
                    error |= !merge_module_decls(modDecl1->declarations, modDecl2->declarations);
                } else {
                    report(decl2->location, decl2->identifier + " already declared in the module");
                    error |= true;
                    continue;
                }
            } else {
                report(decl2->location, "expected module declaration to merge");
                error |= true;
                continue;
            }
        }
    }
    return !error;
}

std::unique_ptr<ModuleDecl> Driver::merge_modules(std::vector<std::unique_ptr<ModuleDecl>> modules) {
    debug_func("");
    std::vector<std::unique_ptr<Decl>> declarations;
    for (auto &&mod : modules) {
        if (!merge_module_decls(declarations, mod->declarations)) {
            return report(mod->location, "cannot merge modules");
        }
    }
    return std::make_unique<ModuleDecl>(SourceLocation{.file_name = "imported"}, "imported.dmz",
                                        std::move(declarations));
}

Driver::Type_ResolvedTree Driver::semantic_pass(Type_Ast &asts) {
    debug_func("");
    std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree;
    Sema sema(std::move(asts));
    resolvedTree = sema.resolve_ast_decl();
    if (resolvedTree.empty()) m_haveError = true;

    if (!sema.resolve_ast_body(resolvedTree)) m_haveError = true;

    if (m_options.resDump) {
        if (!m_haveError) {
            for (auto &&fn : resolvedTree) {
                fn->dump();
            }
        }
        m_haveNormalExit = true;
        return {};
    }

    if (m_options.cfgDump) {
        if (!m_haveError) {
            for (auto &&decl : resolvedTree) {
                if (const auto *md = dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
                    for (auto &&func : md->declarations) {
                        const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(func.get());
                        if (!fn) continue;

                        std::cerr << fn->identifier << ':' << '\n';
                        CFGBuilder().build(*fn->body).dump();
                    }
                }
            }
        }
        m_haveNormalExit = true;
        return {};
    }

    return resolvedTree;
}

Driver::Type_Module Driver::codegen_pass(Type_ResolvedTree &resolvedTree) {
    debug_func("");
    std::unique_ptr<llvm::orc::ThreadSafeModule> module;
    Codegen codegen(resolvedTree, m_options.sources[0].c_str());
    module = codegen.generate_ir(m_options.test);

    if (m_options.llvmDump) {
        module->withModuleDo([](auto &m) { m.dump(); });
        m_haveNormalExit = true;
        return nullptr;
    }

    return module;
}

// Driver::Type_Module Driver::linker_pass(Type_Module &modules) {
//     debug_msg("modules size " << modules.size());
//     Linker linker(std::move(modules));
//     auto module = linker.link_modules();

//     if (m_options.llvmDump) {
//         module->withModuleDo([](auto &m) { m.dump(); });
//         m_haveNormalExit = true;
//         return nullptr;
//     }
//     return module;
// }

int Driver::jit_pass(Type_Module &module) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    ScopedTimer timer(Stats::type::runTime);

    pid_t pid = fork();

    int status;
    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // child
        close(pipefd[1]);

        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        const char *cmd = nullptr;
        std::vector<const char *> args;

        cmd = "lli";
        args.emplace_back("lli");
        args.emplace_back("-O3");
        args.emplace_back(nullptr);

        execvp(cmd, const_cast<char *const *>(args.data()));
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // parent
        close(pipefd[0]);

        llvm::raw_fd_ostream pipe_stream(pipefd[1], false);

        module->withModuleDo([&pipe_stream](auto &m) { m.print(pipe_stream, nullptr); });

        close(pipefd[1]);

        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
}
int Driver::generate_exec_pass(Type_Module &module) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    ScopedTimer timer(Stats::type::compileTime);

    pid_t pid = fork();

    int status;
    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // child
        close(pipefd[1]);

        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        const char *cmd = nullptr;
        std::vector<const char *> args;

        cmd = "clang";
        args.emplace_back("clang");
        args.emplace_back("-O0");
        args.emplace_back("-ggdb");
        args.emplace_back("-x");
        args.emplace_back("ir");
        args.emplace_back("-");
        if (m_options.isModule) {
            args.emplace_back("-c");
        }
        if (!m_options.output.empty()) {
            args.emplace_back("-o");
            args.emplace_back(m_options.output.c_str());
        }
        args.emplace_back(nullptr);
        // for (auto &&arg : args) {
        //     println(arg);
        // }

        execvp(cmd, const_cast<char *const *>(args.data()));
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // parent
        close(pipefd[0]);

        llvm::raw_fd_ostream pipe_stream(pipefd[1], false);

        module->withModuleDo([&pipe_stream](auto &m) { m.print(pipe_stream, nullptr); });

        close(pipefd[1]);

        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
}

int Driver::main() {
    defer([&] {
        if (m_options.printStats) Stats::instance().dump();
    });

    if (m_options.displayHelp) {
        display_help();
        return EXIT_SUCCESS;
    }

    check_sources_pass(m_options.sources);
    check_exit();

    auto lexers = lexer_pass(m_options.sources);
    check_exit();
    auto asts = parser_pass(lexers);
    lexers.clear();
    check_exit();
    include_pass(asts);
    check_exit();

    auto resolvedTrees = semantic_pass(asts);
    check_exit();

    auto module = codegen_pass(resolvedTrees);
    check_exit();

    // auto module = linker_pass(modules);
    // check_exit();

    if (m_options.run) {
        return jit_pass(module);
    } else {
        return generate_exec_pass(module);
    }
}
}  // namespace DMZ