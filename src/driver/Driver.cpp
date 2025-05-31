// #define DEBUG
#include "driver/Driver.hpp"

#include <wait.h>

#include <barrier>

namespace DMZ {
void Driver::display_help() {
    println("Usage:");
    println("  compiler [options] <source_files...>\n");
    println("Options:");
    println("  -h, -help        display this message");
    println("  -o <file>        write executable to <file>");
    println("  -lexer-dump      print the lexer dump");
    println("  -ast-dump        print the abstract syntax tree");
    println("  -res-dump        print the resolved syntax tree");
    println("  -cfg-dump        print the control flow graph");
    println("  -llvm-dump       print the llvm module");
    println("  -print-stats     print the time stats");
    println("  -module          compile a module to .o file");
    println("  -run             runs the program with lli (Just In Time)");
}

CompilerOptions CompilerOptions::parse_arguments(int argc, char **argv) {
    CompilerOptions options;

    int idx = 1;
    while (idx < argc) {
        std::string_view arg = argv[idx];
        if (arg[0] != '-') {
            options.sources.emplace_back(arg);
        } else {
            if (arg == "-h" || arg == "--help")
                options.displayHelp = true;
            else if (arg == "-o")
                options.output = ++idx >= argc ? "" : argv[idx];
            else if (arg == "-lexer-dump")
                options.lexerDump = true;
            else if (arg == "-ast-dump")
                options.astDump = true;
            else if (arg == "-res-dump")
                options.resDump = true;
            else if (arg == "-llvm-dump")
                options.llvmDump = true;
            else if (arg == "-cfg-dump")
                options.cfgDump = true;
            else if (arg == "-run")
                options.run = true;
            else if (arg == "-module")
                options.isModule = true;
            else if (arg == "-print-stats")
                options.printStats = true;
            else
                error("unexpected option '" + std::string(arg) + '\'');
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
        m_workers.submit([&, index]() { lexers[index] = std::make_unique<Lexer>(sources[index].c_str()); });
    }

    m_workers.wait();

    if (m_options.lexerDump) {
        for (auto &&lexer : lexers) {
            Token tok;
            do {
                tok = lexer->next_token();
                println(tok);
            } while (tok.type != TokenType::eof);
        }
        m_haveNormalExit = true;
        return {};
    }
    return lexers;
}

Driver::Type_Asts Driver::parser_pass(Type_Lexers &lexers) {
    std::vector<std::vector<std::unique_ptr<Decl>>> asts;
    asts.resize(lexers.size());
    for (size_t index = 0; index < lexers.size(); index++) {
        m_workers.submit([&, index]() {
            Parser parser(*lexers[index]);
            bool needMain = !m_options.isModule && index == 0;
            auto [ast, success] = parser.parse_source_file(needMain);
            asts[index] = std::move(ast);
            if (!success) m_haveError = true;
        });
    }

    m_workers.wait();

    if (m_options.astDump) {
        for (auto &&ast : asts) {
            for (auto &&fn : ast) {
                fn->dump();
            }
        }
        m_haveNormalExit = true;
        return {};
    }
    return asts;
}

Driver::Type_ResolvedTrees Driver::semantic_pass(Type_Asts &asts) {
    std::vector<std::unique_ptr<Sema>> semas;
    std::vector<std::vector<std::unique_ptr<ResolvedDecl>>> resolvedTrees;
    semas.resize(asts.size());
    resolvedTrees.resize(asts.size());
    std::barrier barrier(asts.size());
    for (size_t index = 0; index < asts.size(); index++) {
        debug_msg("Ast[" << index << "] size " << asts[index].size());
        m_workers.submit([&, index]() {
            semas[index] = std::make_unique<Sema>(std::move(asts[index]));
            resolvedTrees[index] = semas[index]->resolve_ast_decl();
            if (resolvedTrees[index].empty()) m_haveError = true;
        });
    }
    m_workers.wait();

    for (size_t index = 0; index < asts.size(); index++) {
        m_workers.submit([&, index]() {
            if (!semas[index]->resolve_ast_body(resolvedTrees[index])) m_haveError = true;
        });
    }
    m_workers.wait();

    if (m_options.resDump) {
        if (!m_haveError) {
            for (auto &&resolvedTree : resolvedTrees) {
                for (auto &&fn : resolvedTree) {
                    fn->dump();
                }
            }
        }
        m_haveNormalExit = true;
        return {};
    }

    if (m_options.cfgDump) {
        if (!m_haveError) {
            for (auto &&resolvedTree : resolvedTrees) {
                for (auto &&decl : resolvedTree) {
                    const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get());
                    if (!fn) continue;

                    std::cerr << fn->modIdentifier << ':' << '\n';
                    CFGBuilder().build(*fn).dump();
                }
            }
        }
        m_haveNormalExit = true;
        return {};
    }

    return resolvedTrees;
}

Driver::Type_Modules Driver::codegen_pass(Type_ResolvedTrees &resolvedTrees) {
    std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>> modules;
    modules.resize(resolvedTrees.size());
    for (size_t index = 0; index < resolvedTrees.size(); index++) {
        debug_msg("resolvedTree[" << index << "] size " << resolvedTrees[index].size());
        m_workers.submit([&, index]() {
            Codegen codegen(resolvedTrees[index], m_options.sources[index].c_str());
            modules[index] = codegen.generate_ir();
        });
    }
    m_workers.wait();

    return modules;
}

Driver::Type_Module Driver::linker_pass(Type_Modules &modules) {
    debug_msg("modules size " << modules.size());
    Linker linker(std::move(modules));
    auto module = linker.link_modules();

    if (m_options.llvmDump) {
        module->withModuleDo([](auto &m) { m.dump(); });
        m_haveNormalExit = true;
        return nullptr;
    }
    return module;
}

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
        args.emplace_back("-O3");
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
        for (auto &&arg : args) {
            println(arg);
        }

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
    check_exit();

    auto resolvedTrees = semantic_pass(asts);
    check_exit();

    auto modules = codegen_pass(resolvedTrees);
    check_exit();

    auto module = linker_pass(modules);
    check_exit();

    if (m_options.run) {
        return jit_pass(module);
    } else {
        return generate_exec_pass(module);
    }
}
}  // namespace DMZ