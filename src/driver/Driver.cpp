// #define DEBUG
#include "driver/Driver.hpp"

#include "Stats.hpp"
#include "fmt/Formatter.hpp"

namespace DMZ {

ptr<Driver> Driver::driver_instance = nullptr;

void Driver::display_help() {
    println("Usage:");
    println("  dmz [options] <source_file>\n");
    println("Options:");
    println("  -h, -help          display this message");
    println("  -I <module> <path> include <module> <path> to search for modules");
    println("  -o <file>          write executable to <file>");
    println("  -lexer-dump        print the lexer dump");
    println("  -ast-dump          print the abstract syntax tree");
    println("  -fmt-dump          print the fmt syntax tree");
    println("  -import-dump       print the abstract syntax tree after import");
    println("  -no-remove-unused  disable the removal of unused code");
    println("  -res-dump          print the resolved syntax tree");
    println("  -deps-dump         print the resolved syntax tree with dependencies");
    println("  -cfg-dump          print the control flow graph");
    println("  -llvm-dump         print the llvm module");
    println("  -print-stats       print the time stats");
    println("  -module            compile a module to .o file");
    println("  -run               runs the program with lli (Just In Time)");
    println("  -test              runs the test with lli (Just In Time)");
    println("  -fmt               format the dmz source file");
}

CompilerOptions CompilerOptions::parse_arguments(int argc, char **argv) {
    CompilerOptions options;

    int idx = 1;
    while (idx < argc) {
        std::string_view arg = argv[idx];
        if (arg[0] != '-' && options.source.empty()) {
            options.source = arg;
        } else {
            if (arg == "-h" || arg == "--help") {
                options.displayHelp = true;
            } else if (arg == "-o") {
                options.output = ++idx >= argc ? "" : argv[idx];
            } else if (arg == "-I") {
                std::string module_id;
                std::string module_path;
                if (++idx < argc) module_id = argv[idx];
                if (++idx < argc) module_path = argv[idx];
                if (!std::filesystem::exists(module_path)) {
                    error("import '" + module_id + "' have a non existing path '" + module_path + '\'');
                }
                module_path = std::filesystem::canonical(module_path);
                options.imports.emplace(module_id, module_path);

            } else if (arg == "-lexer-dump") {
                options.lexerDump = true;
            } else if (arg == "-ast-dump") {
                options.astDump = true;
            } else if (arg == "-import-dump") {
                options.importDump = true;
            } else if (arg == "-no-remove-unused") {
                options.noRemoveUnused = true;
            } else if (arg == "-res-dump") {
                options.resDump = true;
            } else if (arg == "-deps-dump") {
                options.depsDump = true;
            } else if (arg == "-llvm-dump") {
                options.llvmDump = true;
            } else if (arg == "-cfg-dump") {
                options.cfgDump = true;
            } else if (arg == "-run") {
                options.run = true;
            } else if (arg == "-test") {
                options.test = true;
            } else if (arg == "-fmt-dump") {
                options.fmtDump = true;
            } else if (arg == "-fmt") {
                options.fmt = true;
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

bool Driver::need_exit() {
    if (m_haveError || m_haveNormalExit) return debug_ret(true);
    return debug_ret(false);
}

void Driver::check_sources_pass(std::filesystem::path &source) {
    if (source.empty()) {
        error("no source file specified");
        m_haveError = true;
        return;
    }

    // for (auto &&source : sources) {
    if (source.extension() != ".dmz") {
        error("unexpected source file extension '" + source.extension().string() + "'");
        m_haveError = true;
    }

    if (!std::filesystem::exists(source)) {
        error("failed to open '" + source.string() + '\'');
        m_haveError = true;
    }
    // }
}

ptr<Lexer> Driver::lexer_pass(std::filesystem::path &source) {
    ptr<Lexer> lexer = makePtr<Lexer>(source.c_str());

    if (m_options.lexerDump) {
        Token tok;
        do {
            tok = lexer->next_token();
            println(tok);
        } while (tok.type != TokenType::eof);
        m_haveNormalExit = true;
    }
    return lexer;
}

ptr<ModuleDecl> Driver::parser_pass(ptr<Lexer> lexer) {
    Parser parser(*lexer);
    auto [ast, success] = parser.parse_source_file();
    if (!success) {
        m_haveError = true;
    }

    if (m_options.astDump) {
        if (ast) ast->dump();
        m_haveNormalExit = true;
    }
    return std::move(ast);
}

void Driver::fmt_pass(ptr<ModuleDecl> ast) {
    debug_func("");
    fmt::Formatter fmt(120);

    auto node = fmt.fmt_ast(*ast);
    // auto gen = fmt::Generator(120);
    // auto build = fmt::Builder();
    // std::vector<ptr<fmt::Node>> args;
    // args.emplace_back(makePtr<fmt::Text>("1000000000000000000000000000000"));
    // auto node = build.call("foo", std::move(args));
    // std::vector<ptr<Expr>> args;
    // args.emplace_back(makePtr<IntLiteral>(SourceLocation{}, "100000000000000"));
    // auto call = makePtr<CallExpr>(SourceLocation{}, makePtr<DeclRefExpr>(SourceLocation{}, "foo"), std::move(args));

    if (m_options.fmtDump) {
        node->dump();
    } else {
        fmt.print(node);
    }
    // fmt.fmt_expr(*call);

    // auto node = build.call(makeRef<fmt::Text>("foo"),
    //                        {makeRef<fmt::Text>("1000000000000000000000000000000"),
    //                         build.call(makeRef<fmt::Text>("bar"),
    //                                    {makeRef<fmt::Text>("200000000000000000000"), build.string("this is a
    //                                    string"),
    //                                     build.call(makeRef<fmt::Text>("whithout_arguments"), {})})});
    //   [
    //     Node.Text('1000000000000000000000000000000'),
    //     build.call(
    //       'bar',
    //       [
    //         Node.Text('2000000000000000000000000000000'),
    //         build.string('this is a string'),
    //         build.call('without_arguments', []),
    //       ],
    //     ),
    //   ],

    // gen.generate(*node);
    // gen.print();

    m_haveNormalExit = true;
}

std::pair<std::string, std::filesystem::path> Driver::register_import(const std::filesystem::path &source,
                                                                      std::string_view imported) {
    debug_func("source: '" << source << "' imported '" << imported << "'");
    auto &d = instance();

#ifdef DEBUG
    debug_msg("Registed modules " << d.imported_modules.size());
    for (auto &&[k, v] : d.imported_modules) {
        debug_msg(k);
    }
#endif
    std::string identifier = "";
    std::filesystem::path module_path;
    if (imported.ends_with(".dmz")) {
        auto directory = source.parent_path();
        module_path = directory.append(imported);

        debug_msg("module_path " << module_path);
        if (!std::filesystem::exists(module_path)) {
            debug_msg("error: doesnt exists module_path " << module_path);
            return {"", ""};
        }
        module_path = std::filesystem::canonical(module_path);

        std::filesystem::path parent_path = "";
        std::string module_path_str = module_path.string();
        // Find in imports the path to imported module
        for (auto &&[k, v] : d.m_options.imports) {
            auto imported_dir = v.parent_path();
            imported_dir = std::filesystem::canonical(imported_dir);
            if (module_path_str.find(imported_dir) != std::string::npos) {
                parent_path = imported_dir;
                identifier = k;
                break;
            }
        }
        if (parent_path.empty()) {
            // if not in imports it need to be in the proyect dir
            std::filesystem::path proyect_path = d.m_options.source.parent_path();
            proyect_path = std::filesystem::canonical(proyect_path);
            debug_msg("proyect_path " << proyect_path << " module_path_str " << module_path_str);
            if (module_path_str.find(proyect_path) != std::string::npos) {
                parent_path = proyect_path;
            }
        }
        if (parent_path.empty()) {
            debug_msg("error: parent_path empty");
            return {"", ""};
        }
        // Extract the diferente, in other words the relative path
        std::string parent_path_str = parent_path.string();
        auto diff = module_path_str.substr(parent_path_str.size());
        std::string termination = ".dmz";
        if (diff.find_last_of(termination) != diff.size() - 1) {
            dmz_unreachable("unexpected diff " + std::to_string(diff.find_last_of(termination)) + " " + diff);
        }

        // convert the relative path to symbol name
        int start_pos = (diff.size() > 0 && diff[0] == '/') ? 1 : 0;
        diff = diff.substr(start_pos, diff.size() - (termination.size() + start_pos));
        std::replace(diff.begin(), diff.end(), '/', '.');
        // identifier empty if is not from imports
        if (!identifier.empty() && !diff.empty()) {
            identifier += ".";
        }
        identifier += diff;
    } else {
        auto it = d.m_options.imports.find(std::string(imported));
        if (it == d.m_options.imports.end()) {
            debug_msg("error: not in imports");
            return {"", ""};
        }
        module_path = (*it).second;
        // The identifier is simply the imported module
        identifier = imported;
    }

    debug_msg("module_path " << module_path);
    debug_msg("identifier " << identifier);

    debug_msg("Search: " << module_path);
    if (d.imported_modules.find(module_path) != d.imported_modules.end()) {
        debug_msg("find not reimport: " << module_path);
        return {identifier, module_path};
    }

    debug_msg("Register module: '" << module_path << "'");
    d.imported_modules.emplace(module_path, nullptr);
    return {identifier, module_path};
}

void Driver::import_pass(ptr<ModuleDecl> &ast) {
    debug_func("");
    auto all_imported = [&]() -> bool {
        for (auto &&[k, v] : imported_modules) {
            if (!v) return false;
        }
        return true;
    };

    while (!all_imported()) {
        std::vector<std::filesystem::path> to_remove;
        for (auto &&[k, v] : imported_modules) {
            if (!v) {
                if (!std::filesystem::exists(k)) {
                    debug_msg("error opening file " << k);
                    std::cerr << "error: opening file '" << k << "'\n";
                    to_remove.emplace_back(k);
                    continue;
                }
                Lexer l(k.c_str());
                Parser p(l);
                auto [parse_ast, success] = p.parse_source_file();
                if (!success) {
                    debug_msg("error parsing " << k);
                    to_remove.emplace_back(k);
                    continue;
                }
                v = std::move(parse_ast);
            }
        }
        if (to_remove.size() != 0) {
            m_haveError = true;
            for (auto &&k : to_remove) {
                imported_modules.erase(k);
            }
        }
    }

    if (m_options.importDump) {
        ast->dump();
        for (auto &&[k, v] : imported_modules) {
            v->dump();
        }

        m_haveNormalExit = true;
    }
    return;
}

std::vector<ptr<ResolvedModuleDecl>> Driver::semantic_pass(ptr<ModuleDecl> ast) {
    debug_func("");
    ScopedTimer(StatType::Semantic);
    std::vector<ptr<ResolvedModuleDecl>> resolvedTree;
    Sema sema(std::move(ast));
    bool needMain = !m_options.isModule && !m_options.test;
    resolvedTree = sema.resolve_ast_decl(needMain);
    if (resolvedTree.empty()) m_haveError = true;

    if (!m_haveError && !sema.resolve_ast_body(resolvedTree)) m_haveError = true;
    if (!m_haveError && !m_options.noRemoveUnused) sema.remove_unused(resolvedTree, m_options.test);

    if (m_options.depsDump) {
        if (!m_haveError) {
            for (auto &&fn : resolvedTree) {
                fn->dump_dependencies();
            }
        }
        m_haveNormalExit = true;
        return {};
    }

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

ptr<llvm::orc::ThreadSafeModule> Driver::codegen_pass(std::vector<ptr<ResolvedModuleDecl>> resolvedTree) {
    debug_func("");
    ptr<llvm::orc::ThreadSafeModule> module;
    Codegen codegen(std::move(resolvedTree), m_options.source.c_str());
    module = codegen.generate_ir(m_options.test);

    if (m_options.llvmDump) {
        module->withModuleDo([](auto &m) { m.dump(); });
        m_haveNormalExit = true;
        return nullptr;
    }

    return module;
}

int Driver::jit_pass(ptr<llvm::orc::ThreadSafeModule> &module) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    ScopedTimer(StatType::Run);

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
        args.emplace_back("-O0");
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
int Driver::generate_exec_pass(ptr<llvm::orc::ThreadSafeModule> &module) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    ScopedTimer(StatType::Compile);

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

int Driver::ptrBitSize() {
    llvm::LLVMContext context;
    llvm::Module module("tmp", context);
    return module.getDataLayout().getPointerSizeInBits();
}

int Driver::main() {
    defer([&] {
        if (m_options.printStats) Stats::instance().dump();
    });
    ScopedTimer(StatType::Total);

    if (m_options.displayHelp) {
        display_help();
        return EXIT_SUCCESS;
    }

    check_sources_pass(m_options.source);
    if (need_exit()) return 0;

    auto lexer = lexer_pass(m_options.source);
    if (need_exit()) return 0;
    auto ast = parser_pass(std::move(lexer));
    if (need_exit()) return 0;
    if (m_options.fmt || m_options.fmtDump) {
        fmt_pass(std::move(ast));
        if (need_exit()) return 0;
    }

    import_pass(ast);
    if (need_exit()) return 0;

    auto resolvedTrees = semantic_pass(std::move(ast));
    if (need_exit()) return 0;

    auto module = codegen_pass(std::move(resolvedTrees));
    if (need_exit()) return 0;

    // auto module = linker_pass(modules);
    // need_exit();

    if (m_options.run) {
        return jit_pass(module);
    } else {
        return generate_exec_pass(module);
    }
}
}  // namespace DMZ