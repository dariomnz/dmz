#include "Driver.hpp"

#include <fstream>
#include <iostream>

#include "Codegen.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Semantic.hpp"
#include "debug.hpp"

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
    println("  -llvm-dump   print the llvm module");
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
        do{
            tok = lexer.next_token();
            println(tok);
        }while(tok.type != TokenType::eof);
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

    if (resolvedTree.empty()) return 1;

    Codegen codegen(std::move(resolvedTree), options.source.c_str());
    llvm::Module *llvmIR = codegen.generate_ir();

    if (options.llvmDump) {
        llvmIR->dump();
        return 0;
    }

    std::stringstream path;
    path << "tmp-" << std::filesystem::hash_value(options.source) << ".ll";
    const std::string &llvmIRPath = path.str();

    std::error_code errorCode;
    llvm::raw_fd_ostream f(llvmIRPath, errorCode);
    llvmIR->print(f, nullptr);

    std::stringstream command;
    command << "clang " << llvmIRPath;
    if (!options.output.empty()) command << " -o " << options.output;

    int ret = std::system(command.str().c_str());

    std::filesystem::remove(llvmIRPath);

    return ret;
}