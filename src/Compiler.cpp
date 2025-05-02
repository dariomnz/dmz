#include <iostream>

#include "Codegen.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Semantic.hpp"
#include "debug.hpp"

using namespace C;

static int s_argc = 0;
static char **s_argv = nullptr;

void usage() { println("Usage: " << s_argv[0] << " <file name>"); }

int main(int argc, char *argv[]) {
    s_argc = argc;
    s_argv = argv;

    if (argc != 2) {
        usage();
        exit(0);
    }
    {
        Lexer l(s_argv[1]);

        println("Content of file " << l.get_file_name() << ": ");
        println(l.get_file_content());
        std::cout << "---------------------------------" << std::endl;

        auto tokens = l.tokenize_file();
        println(tokens);
    }
    std::cout << "---------------------------------" << std::endl;
    {
        Lexer l(s_argv[1]);
        Parser p(l);

        auto ret = p.parse_source_file();
        for (auto &val : ret.first) {
            val->dump();
        }
    }
    std::cout << "---------------------------------" << std::endl;
    {
        Lexer l(s_argv[1]);
        Parser p(l);
        auto ret = p.parse_source_file();
        Sema s(ret.first);
        auto ret_s = s.resolve_ast();
        for (auto &val : ret_s) {
            val->dump();
        }
    }
    std::cout << "---------------------------------" << std::endl;
    {
        Lexer l(s_argv[1]);
        Parser p(l);
        auto ret = p.parse_source_file();
        Sema s(ret.first);
        auto ret_s = s.resolve_ast();
        Codegen c(std::move(ret_s), s_argv[1]);
        llvm::Module *llvmIR = c.generate_ir();
        llvmIR->print(llvm::dbgs(), nullptr);
    }
}