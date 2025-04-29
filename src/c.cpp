#include <iostream>

#include "Lexer.hpp"
#include "Parser.hpp"
#include "debug.hpp"

using namespace C;

static int s_argc = 0;
static char** s_argv = nullptr;

void usage() { print("Usage: " << s_argv[0] << " <file name>"); }

int main(int argc, char* argv[]) {
    s_argc = argc;
    s_argv = argv;

    if (argc != 2) {
        usage();
        exit(0);
    }
    {
        Lexer l(s_argv[1]);

        print("Content of file " << l.get_file_name() << ": ");
        print(l.get_file_content());

        auto tokens = l.tokenize_file();
        print(tokens);
    }
    {
        Lexer l(s_argv[1]);
        Parser p(l);

        auto ret = p.parse_source_file();
        for (auto &val : ret.first)
        {
            val->dump();
        }
        
    }
}