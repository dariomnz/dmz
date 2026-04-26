// #define DEBUG
#include "driver/Driver.hpp"

#include "codegen/Codegen.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/Module.h>
#pragma GCC diagnostic pop

#include <sys/wait.h>

#include "Debug.hpp"
#include "Stats.hpp"
#include "fmt/Formatter.hpp"
#include "lsp/server.hpp"
#include "parser/Parser.hpp"
#include "test_runner/test_runner.hpp"

namespace DMZ {

Driver::Driver(CompilerOptions options) : m_options(options) {}
Driver::~Driver() = default;

void Driver::display_help() {
    println("Usage:");
    println("  dmz [options] <source_file>\n");
    println("Options:");
    println("  -h, -help            display this message");
    println("  -I <module> <path>   include <module> <path> to search for modules");
    println("  -o <file>            write executable to <file>");
    println("  -O0,-O1,-O2,-O3      set the optimization level (default: -O0)");
    println("  -lexer-dump          print the lexer dump");
    println("  -ast-dump            print the abstract syntax tree");
    println("  -lsp                 start the language server");
    println("  -fmt-dump            print the fmt syntax tree");
    println("  -import-dump         print the abstract syntax tree after import");
    println("  -no-remove-unused    disable the removal of unused code");
    println("  -res-dump            print the resolved syntax tree");
    println("  -deps-dump           print the resolved syntax tree with dependencies");
    println("  -deps-dot-dump       print the resolved syntax tree with dependencies in dot format");
    println("  -cfg-dump            print the control flow graph");
    println("  -llvm-dump           print the llvm module");
    println("  -print-stats         print the time stats");
    println("  -module              compile a module to .o file");
    println("  -g                   generate debug symbols");
    println("  -run                 runs the program with lli (Just In Time)");
    println("  -test [file|dir]     runs tests with lli (JIT); if dir given, runs all .dmz files in it");
    println("  -test-compiler [dir] runs the compiler tests in [dir] (default: ./test)");
    println("  -fmt                 format the dmz source file");
    println("  -quiet               suppress output for successful tests");
    println("  -fail-fast, -ff      terminate tests on first failure");
    println("  -nolibc              do not link with libc");
}

CompilerOptions CompilerOptions::parse_arguments(int argc, char **argv) {
    CompilerOptions options;

    int idx = 1;
    while (idx < argc) {
        std::string_view arg = argv[idx];
        if ((arg == "-" && options.source.empty()) || (arg[0] != '-' && options.source.empty())) {
            options.source = arg;
        } else {
            if (arg == "-h" || arg == "--help") {
                options.displayHelp = true;
            } else if (arg == "-o") {
                options.output = ++idx >= argc ? "" : argv[idx];
            } else if (arg.starts_with("-O")) {
                options.optimizationLevel = arg;
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
            } else if (arg == "-deps-dot-dump") {
                options.depsDotDump = true;
            } else if (arg == "-llvm-dump") {
                options.llvmDump = true;
            } else if (arg == "-asm-dump") {
                options.asmDump = true;
            } else if (arg == "-cfg-dump") {
                options.cfgDump = true;
            } else if (arg == "-run") {
                options.run = true;
            } else if (arg == "-g") {
                options.debugSymbols = true;
            } else if (arg == "-test") {
                options.test = true;
            } else if (arg == "-test-compiler") {
                options.testCompiler = true;
                if (++idx < argc) {
                    std::string_view nextArg = argv[idx];
                    if (nextArg[0] != '-') {
                        options.source = nextArg;
                    } else {
                        --idx;  // Put it back, it's another option
                    }
                }
                if (options.source.empty()) {
                    options.source = "./test";
                }
            } else if (arg == "-j") {
                if (++idx < argc) {
                    options.parallelJobs = std::stoi(argv[idx]);
                }
            } else if (arg == "-fmt-dump") {
                options.fmtDump = true;
            } else if (arg == "-fmt") {
                options.fmt = true;
            } else if (arg == "-module") {
                options.isModule = true;
            } else if (arg == "-print-stats") {
                options.printStats = true;
                Stats::instance().enabled = true;
            } else if (arg == "-lsp" || arg == "--lsp") {
                options.lsp = true;
            } else if (arg == "-quiet") {
                options.quiet = true;
            } else if (arg == "-fail-fast" || arg == "-ff") {
                options.failFast = true;
            } else if (arg == "-nolibc") {
                options.noLibc = true;
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

int Driver::exit_code() {
    if (m_haveError) return debug_ret(EXIT_FAILURE);
    return debug_ret(EXIT_SUCCESS);
}

void Driver::check_sources_pass(std::filesystem::path &source) {
    // stdin
    if (source == "-") return;

    if (source.empty()) {
        error("no source file specified");
        m_haveError = true;
        return;
    }

    if (!std::filesystem::exists(source)) {
        error("failed to open '" + source.string() + '\'');
        m_haveError = true;
        return;
    }

    if (std::filesystem::is_directory(source)) {
        if (!m_options.test && !m_options.testCompiler) {
            error("source is a directory, but -test or -test-compiler not specified");
            m_haveError = true;
        }
        return;
    }

    if (source.extension() != ".dmz") {
        error("unexpected source file extension '" + source.extension().string() + "'");
        m_haveError = true;
    }

    source = std::filesystem::canonical(source);
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
    Parser parser(*this, *lexer);
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

    if (m_options.fmtDump) {
        node->dump();
    } else {
        fmt.print(node);
    }

    m_haveNormalExit = true;
}

std::vector<ptr<ResolvedModuleDecl>> Driver::semantic_pass(ptr<ModuleDecl> ast) {
    debug_func("");
    ScopedTimer(StatType::Semantic);
    std::vector<ptr<ResolvedModuleDecl>> resolvedTree;
    Sema sema(*this, std::move(ast));
    bool needMain = !m_options.isModule && !m_options.test;
    resolvedTree = sema.resolve_ast_decl(m_options.source, needMain);
    if (resolvedTree.empty()) m_haveError = true;

    if (!m_haveError && !sema.resolve_ast_body(resolvedTree)) m_haveError = true;

    if (m_options.depsDump || m_options.depsDotDump) {
        if (!m_haveError) {
            for (auto &&fn : resolvedTree) {
                fn->dump_dependencies(0, m_options.depsDotDump);
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

std::pair<ptr<llvm::LLVMContext>, ptr<llvm::Module>> Driver::codegen_pass(
    std::vector<ptr<ResolvedModuleDecl>> resolvedTree) {
    debug_func("");
    Codegen codegen(std::move(resolvedTree), m_options.source.c_str(), m_options.debugSymbols, m_options.noRemoveUnused,
                    m_options.isModule);
    std::pair<ptr<llvm::LLVMContext>, ptr<llvm::Module>> module =
        codegen.generate_ir(m_options.test, m_options.optimizationLevel);

    if (m_options.llvmDump) {
        module.second->dump();
        m_haveNormalExit = true;
        return {};
    }

    return module;
}

int Driver::generate_exec_pass(ptr<llvm::Module> &module) {
    debug_func("");
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
        if (!m_options.optimizationLevel.empty()) {
            args.emplace_back(m_options.optimizationLevel.c_str());
        } else {
            args.emplace_back("-O0");
        }
        if (m_options.debugSymbols) {
            args.emplace_back("-g");
        }
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
        if (m_options.noLibc) {
            args.emplace_back("-nolibc");
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

        module->print(pipe_stream, nullptr);

        close(pipefd[1]);

        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
}

int Driver::asm_pass(ptr<llvm::Module> &module) {
    debug_func("");
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

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
        args.emplace_back("-g");
        args.emplace_back("-x");
        args.emplace_back("ir");
        args.emplace_back("-");
        args.emplace_back("-S");
        if (m_options.isModule) {
            args.emplace_back("-c");
        }
        args.emplace_back("-o");
        args.emplace_back("-");
        args.emplace_back(nullptr);

        execvp(cmd, const_cast<char *const *>(args.data()));
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // parent
        close(pipefd[0]);

        llvm::raw_fd_ostream pipe_stream(pipefd[1], false);

        module->print(pipe_stream, nullptr);

        close(pipefd[1]);

        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
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

    if (m_options.lsp) {
        lsp::LSPServer server;
        server.run();
        return EXIT_SUCCESS;
    }

    check_sources_pass(m_options.source);
    if (need_exit()) return exit_code();

    if (m_options.testCompiler) {
        TestOptions testOpts;
        testOpts.parallel_jobs = m_options.parallelJobs;
        testOpts.quiet = m_options.quiet;
        testOpts.fail_fast = m_options.failFast;
        return run_tests(m_options.source.string(), testOpts);
    }

    vec<ptr<ResolvedModuleDecl>> resolvedTree;
    if (m_options.test && std::filesystem::is_directory(m_options.source)) {
        namespace fs = std::filesystem;
        fs::path dir = fs::canonical(m_options.source);

        std::vector<fs::path> files;
        for (const auto &entry : fs::recursive_directory_iterator(dir))
            if (entry.is_regular_file() && entry.path().extension() == ".dmz") files.push_back(entry.path());
        std::sort(files.begin(), files.end());

        if (files.empty()) {
            println("No .dmz files found in " + dir.string());
            return EXIT_SUCCESS;
        }

        m_options.testDir = dir;
        m_options.source = files[0];

        auto firstLexer = lexer_pass(files[0]);
        if (need_exit()) return exit_code();
        auto firstAst = parser_pass(std::move(firstLexer));
        if (need_exit()) return exit_code();

        Sema sema(*this, std::move(firstAst));

        for (size_t i = 1; i < files.size(); i++) {
            m_options.source = files[i];
            auto lexer = lexer_pass(files[i]);
            if (need_exit()) return exit_code();
            auto ast = parser_pass(std::move(lexer));
            if (need_exit()) return exit_code();
            sema.queue_module(std::move(ast), files[i]);
        }

        m_options.source = files[0];
        resolvedTree = sema.resolve_ast_decl(files[0], false);
        if (resolvedTree.empty()) return EXIT_FAILURE;
        if (!sema.resolve_ast_body(resolvedTree)) return EXIT_FAILURE;
    } else {
        auto lexer = lexer_pass(m_options.source);
        if (need_exit()) return exit_code();
        auto ast = parser_pass(std::move(lexer));
        if (need_exit()) return exit_code();
        if (m_options.fmt || m_options.fmtDump) {
            fmt_pass(std::move(ast));
            if (need_exit()) return exit_code();
        }

        resolvedTree = semantic_pass(std::move(ast));
        if (need_exit()) return exit_code();
    }

    auto module = codegen_pass(std::move(resolvedTree));
    if (need_exit()) return exit_code();

    if (m_options.asmDump) {
        return asm_pass(module.second);
    }

    if (m_options.run) {
        return jit_pass(module.first, module.second);
    } else {
        return generate_exec_pass(module.second);
    }
}
}  // namespace DMZ