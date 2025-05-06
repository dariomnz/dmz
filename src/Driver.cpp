#include "Driver.hpp"

#include <wait.h>

#include "CFG.hpp"
#include "Codegen.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Semantic.hpp"

namespace C {
static void displayHelp() {
    println("Usage:");
    println("  compiler [options] <source_file>\n");
    println("Options:");
    println("  -h, -help    display this message");
    println("  -o <file>    write executable to <file>");
    println("  -lexer-dump    print the lexer dump");
    println("  -ast-dump    print the abstract syntax tree");
    println("  -res-dump    print the resolved syntax tree");
    println("  -cfg-dump    print the control flow graph");
    println("  -llvm-dump   print the llvm module");
    println("  -run         runs the program with lli (Just In Time)");
}

static CompilerOptions parseArguments(int argc, char **argv) {
    CompilerOptions options;

    int idx = 1;
    while (idx < argc) {
        std::string_view arg = argv[idx];
        if (arg[0] != '-') {
            if (!options.source.empty()) {
                error("unexpected argument '" + std::string(arg) + '\'');
            }
            options.source = arg;
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
            else if (arg == "-o")
                options.output = ++idx >= argc ? "" : argv[idx];
            else
                error("unexpected option '" + std::string(arg) + '\'');
        }

        ++idx;
    }

    return options;
}
}  // namespace C

using namespace C;

int main(int argc, char *argv[]) {
    CompilerOptions options = parseArguments(argc, argv);

    if (options.displayHelp) {
        displayHelp();
        return 0;
    }

    if (options.source.empty()) {
        error("no source file specified");
    }

    if (options.source.extension() != ".dmz") {
        error("unexpected source file extension '" + options.source.extension().string() + "'");
    }

    std::ifstream file(options.source);
    if (!file) {
        error("failed to open '" + options.source.string() + '\'');
    }

    Lexer lexer(options.source.c_str());

    if (options.lexerDump) {
        Token tok;
        do {
            tok = lexer.next_token();
            println(tok);
        } while (tok.type != TokenType::eof);
        return 0;
    }

    Parser parser(lexer);
    auto [ast, success] = parser.parse_source_file();

    if (options.astDump) {
        for (auto &&fn : ast) fn->dump();
        return 0;
    }

    if (!success) return 1;

    Sema sema(ast);
    auto resolvedTree = sema.resolve_ast();

    if (options.resDump) {
        for (auto &&fn : resolvedTree) fn->dump();
        return 0;
    }

    if (options.cfgDump) {
        for (auto &&decl : resolvedTree) {
            const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get());
            if (!fn) continue;

            std::cerr << fn->identifier << ':' << '\n';
            CFGBuilder().build(*fn).dump();
        }
        return 0;
    }

    if (resolvedTree.empty()) return 1;

    Codegen codegen(std::move(resolvedTree), options.source.c_str());
    llvm::Module *llvmIR = codegen.generate_ir();

    if (options.llvmDump) {
        llvmIR->dump();
        return 0;
    }

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

        llvmIR->print(pipe_stream, nullptr);

        close(pipefd[1]);

        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }

    return 1;
}