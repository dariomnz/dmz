#pragma once

#include <iostream>
#include <string_view>

#include "debug.hpp"

namespace C {

#define varOrReturn(var, init) \
    auto var = (init);         \
    if (!var) return nullptr;
#define matchOrReturn(tok, msg) \
    if (m_nextToken.type != tok) return report(m_nextToken.loc, msg);

struct SourceLocation {
    std::string_view file_name = {};
    size_t line = 0, col = 0;

    friend std::ostream& operator<<(std::ostream& os, const SourceLocation& s) {
        os << get_file_name(s.file_name.data()) << ":" << s.line + 1 << ":" << s.col + 1;
        return os;
    }
};

static inline std::nullptr_t report(SourceLocation loc, std::string_view message, bool isWarning = false) {
    std::cerr << loc << ':' << (isWarning ? " warning: " : " error: ") << message << '\n';

    return nullptr;
}

static inline std::string indent(size_t level) { return std::string(level * 2, ' '); }

[[noreturn]] static inline void error(std::string_view msg) {
    std::cerr << "error: " << msg << '\n';
    std::exit(1);
}
}  // namespace C
