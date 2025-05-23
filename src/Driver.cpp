#include "Driver.hpp"

#include <wait.h>

#include <barrier>
#include <filesystem>

#include "ThreadPool.hpp"
#include "codegen/Codegen.hpp"
#include "lexer/Lexer.hpp"
#include "linker/Linker.hpp"
#include "parser/Parser.hpp"
#include "semantic/CFG.hpp"
#include "semantic/Semantic.hpp"

namespace DMZ {
static void displayHelp() {
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
    println("  -run             runs the program with lli (Just In Time)");
}

static CompilerOptions parseArguments(int argc, char **argv) {
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
            else if (arg == "-print-stats")
                options.printStats = true;
            else
                error("unexpected option '" + std::string(arg) + '\'');
        }

        ++idx;
    }

    return options;
}
}  // namespace DMZ

using namespace DMZ;

int main(int argc, char *argv[]) {
    CompilerOptions options = parseArguments(argc, argv);
    defer([&options] {
        if (options.printStats) Stats::instance().dump();
    });

    if (options.displayHelp) {
        displayHelp();
        return 0;
    }

    if (options.sources.empty()) {
        error("no source files specified");
    }

    ThreadPool workers;
    std::mutex modulesMutex;
    std::barrier barrier(options.sources.size());
    std::atomic_bool haveError(false);
    std::atomic_bool normalExit(false);
    std::vector<std::unique_ptr<llvm::orc::ThreadSafeModule>> modules;
    std::condition_variable inOrderCV;
    std::filesystem::path inOrderCurrent = options.sources[0];

    for (auto &&source : options.sources) {
        if (source.extension() != ".dmz") {
            error("unexpected source file extension '" + source.extension().string() + "'");
        }

        if (!std::filesystem::exists(source)) {
            error("failed to open '" + source.string() + '\'');
        }
    }
    size_t index = -1;
    for (auto &&s : options.sources) {
        index++;
        workers.submit([&, source = s, i = index]() {
            size_t index = i;
            Lexer lexer(source.c_str());

            if (options.lexerDump) {
                std::unique_lock lock(modulesMutex);
                if (inOrderCurrent != source) {
                    inOrderCV.wait(lock, [source, &inOrderCurrent]() { return inOrderCurrent == source; });
                }

                Token tok;
                do {
                    tok = lexer.next_token();
                    println(tok);
                } while (tok.type != TokenType::eof);
                normalExit = true;
                if (index != options.sources.size() - 1) {
                    inOrderCurrent = options.sources[++index];
                    inOrderCV.notify_all();
                }
                return;
            }

            Parser parser(lexer);
            auto [ast, success] = parser.parse_source_file();

            if (options.astDump) {
                std::unique_lock lock(modulesMutex);
                if (inOrderCurrent != source) {
                    inOrderCV.wait(lock, [source, &inOrderCurrent]() { return inOrderCurrent == source; });
                }

                for (auto &&fn : ast) fn->dump();
                normalExit = true;
                if (index != options.sources.size() - 1) {
                    inOrderCurrent = options.sources[++index];
                    inOrderCV.notify_all();
                }
                return;
            }

            if (!success) haveError = true;
            barrier.arrive_and_wait();
            if (haveError) return;

            Sema sema(ast);
            auto resolvedTree = sema.resolve_ast();

            if (options.resDump) {
                std::unique_lock lock(modulesMutex);
                if (inOrderCurrent != source) {
                    inOrderCV.wait(lock, [source, &inOrderCurrent]() { return inOrderCurrent == source; });
                }

                for (auto &&fn : resolvedTree) fn->dump();
                normalExit = true;
                if (index != options.sources.size() - 1) {
                    inOrderCurrent = options.sources[++index];
                    inOrderCV.notify_all();
                }
                return;
            }

            if (options.cfgDump) {
                std::unique_lock lock(modulesMutex);
                if (inOrderCurrent != source) {
                    inOrderCV.wait(lock, [source, &inOrderCurrent]() { return inOrderCurrent == source; });
                }

                for (auto &&decl : resolvedTree) {
                    const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get());
                    if (!fn) continue;

                    std::cerr << fn->identifier << ':' << '\n';
                    CFGBuilder().build(*fn).dump();
                }
                normalExit = true;
                if (index != options.sources.size() - 1) {
                    inOrderCurrent = options.sources[++index];
                    inOrderCV.notify_all();
                }
                return;
            }

            if (resolvedTree.empty()) haveError = true;
            barrier.arrive_and_wait();
            if (haveError) return;

            Codegen codegen(std::move(resolvedTree), source.c_str());
            std::unique_lock lock(modulesMutex);
            if (inOrderCurrent != source) {
                inOrderCV.wait(lock, [source, &inOrderCurrent]() { return inOrderCurrent == source; });
            }

            modules.emplace_back(codegen.generate_ir());
            if (index != options.sources.size() - 1) {
                inOrderCurrent = options.sources[++index];
                inOrderCV.notify_all();
            }
        });
    }

    workers.wait();
    if (normalExit) return 0;
    if (haveError) return 1;

    Linker linker(std::move(modules));

    auto llvmModule = linker.link_modules();

    if (options.llvmDump) {
        llvmModule->withModuleDo([](auto &m) { m.dump(); });
        return 0;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    std::unique_ptr<ScopedTimer> timer;
    if (options.run) {
        timer = std::make_unique<ScopedTimer>(Stats::type::runTime);
    } else {
        timer = std::make_unique<ScopedTimer>(Stats::type::compileTime);
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

        if (options.run) {
            cmd = "lli";
            args.emplace_back("lli");
            args.emplace_back("-O3");
        } else {
            cmd = "clang";
            args.emplace_back("clang");
            args.emplace_back("-O3");
            args.emplace_back("-x");
            args.emplace_back("ir");
            args.emplace_back("-");
            if (!options.output.empty()) {
                args.emplace_back("-o");
                args.emplace_back(options.output.c_str());
            }
        }
        args.emplace_back(nullptr);

        execvp(cmd, const_cast<char *const *>(args.data()));
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // parent
        close(pipefd[0]);

        llvm::raw_fd_ostream pipe_stream(pipefd[1], false);

        llvmModule->withModuleDo([&pipe_stream](auto &m) { m.print(pipe_stream, nullptr); });

        close(pipefd[1]);

        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }

    return 1;
}