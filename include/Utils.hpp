#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>

namespace DMZ {

#define varOrReturn(var, init) \
    auto var = (init);         \
    if (!var) return nullptr;
#define matchOrReturn(tok, msg) \
    if (m_nextToken.type != tok) return report(m_nextToken.loc, msg);

#define dmz_unreachable(msg) ::DMZ::__internal_unreachable(msg, __FILE__, __LINE__)

[[noreturn]] [[maybe_unused]] static inline void __internal_unreachable(std::string msg, const char* source, int line) {
    std::cerr << "UNREACHABLE at " << source << ':' << line << ": " << msg << std::endl;
    std::exit(EXIT_FAILURE);
}

struct SourceLocation {
    std::string file_name = {};
    size_t line = 0, col = 0;

    std::string to_string() const {
        std::stringstream os;
        os << *this;
        return os.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const SourceLocation& s) {
        os << s.file_name.data() << ":" << s.line << ":" << s.col + 1;
        return os;
    }
};

[[maybe_unused]] static inline std::nullptr_t report(SourceLocation loc, std::string_view message,
                                                     bool isWarning = false) {
    static std::mutex reportMutex;
    std::unique_lock lock(reportMutex);
    std::cerr << loc << ':' << (isWarning ? " warning: " : " error: ") << message << '\n';

    return nullptr;
}

struct indent {
    size_t level;

    indent(size_t level) : level(level) {}

    friend std::ostream& operator<<(std::ostream& os, const indent& di) {
        for (size_t i = 0; i < di.level; i++) {
            os << "  ";
        }
        return os;
    }
};

struct indent_line {
    size_t level = 0;
    size_t extra_level = 0;
    bool horizontal = false;

    indent_line(size_t level, size_t extra_level, bool horizontal)
        : level(level), extra_level(extra_level), horizontal(horizontal) {}

    friend std::ostream& operator<<(std::ostream& os, const indent_line& di) {
        for (size_t i = 0; i < di.level; i++) {
            if (di.horizontal && i == di.level - 1) {
                os << " ├─";
            } else {
                os << " │ ";
            }
        }
        for (size_t j = 0; j < di.extra_level; j++) {
            os << "  ";
        }
        return os;
    }
};

class Dumper {
   public:
    Dumper(std::function<void()> action) : m_action(action) {}

   private:
    std::function<void()> m_action;

   public:
    friend std::ostream& operator<<(std::ostream& os, const Dumper& dumper) {
        if (dumper.m_action) dumper.m_action();
        return os;
    }
};

[[maybe_unused]] [[noreturn]] static inline void error(std::string_view msg) {
    std::cerr << "error: " << msg << '\n';
    std::exit(1);
}

class DeferAction {
   public:
    explicit DeferAction(std::function<void()> action) : m_action(action) {}
    ~DeferAction() {
        if (m_action) {
            m_action();
        }
    }

   private:
    std::function<void()> m_action;
};

#define ____defer(action, line) DeferAction defer_object_##line(action)
#define __defer(action, line)   ____defer(action, line)
#define defer(action)           __defer(action, __LINE__)

// helper type for the visitor #4
template <class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overload(Ts...) -> overload<Ts...>;

[[maybe_unused]] static std::optional<std::string> str_from_source(std::string_view literal) {
    std::string res = "";
    static const std::unordered_map<char, char> specialChars = {
        {'n', '\n'}, {'t', '\t'}, {'r', '\r'},  {'v', '\v'},  {'b', '\b'},  {'f', '\f'},
        {'a', '\a'}, {'0', '\0'}, {'\\', '\\'}, {'\"', '\"'}, {'\'', '\''},
    };
    for (size_t i = 0; i < literal.length(); ++i) {
        if (literal[i] == '\\') {
            i++;
            if (i < literal.length()) {
                auto it = specialChars.find(literal[i]);
                if (it != specialChars.end()) {
                    res += it->second;
                }
            } else {
                // Error end '\'
                return std::nullopt;
            }
        } else {
            res += literal[i];
        }
    }
    return res;
}

[[maybe_unused]] static std::string str_to_source(std::string_view str) {
    std::string res = "";
    static const std::unordered_map<char, const char*> specialChars = {
        {'\n', "\\n"}, {'\t', "\\t"}, {'\r', "\\r"},  {'\v', "\\v"},  {'\b', "\\b"},  {'\f', "\\f"},
        {'\a', "\\a"}, {'\0', "\\0"}, {'\\', "\\\\"}, {'\"', "\\\""}, {'\'', "\\\'"},
    };
    for (size_t i = 0; i < str.length(); ++i) {
        auto it = specialChars.find(str[i]);
        if (it != specialChars.end()) {
            res += it->second;
        } else {
            res += str[i];
        }
    }
    return res;
}

[[maybe_unused]] static size_t split_count(std::string_view s, std::string_view delimiter) {
    if (s.empty() || delimiter.empty()) {
        return 1;
    }

    size_t count = 1;
    size_t pos = s.find(delimiter);

    while (pos != std::string_view::npos) {
        count++;
        pos = s.find(delimiter, pos + 1);
    }

    return count;
}

[[maybe_unused]] static std::string_view split(std::string_view s, std::string_view delimiter, size_t index) {
    size_t currentSplitIndex = 0;
    size_t start = 0;
    size_t end = s.find(delimiter);

    while (end != std::string_view::npos) {
        if (currentSplitIndex == index) {
            return s.substr(start, end - start);
        }
        start = end + delimiter.size();
        end = s.find(delimiter, start);
        currentSplitIndex++;
    }

    if (currentSplitIndex == index) {
        return s.substr(start);
    }

    return {};
}

template <typename from, typename to>
std::vector<std::unique_ptr<to>> move_vector_ptr(std::vector<std::unique_ptr<from>>& source_vector) {
    std::vector<std::unique_ptr<to>> target_vector;
    target_vector.reserve(source_vector.size());

    for (auto& ptr_derived : source_vector) {
        if (auto ptr = dynamic_cast<to*>(ptr_derived.release())) {
            target_vector.emplace_back(ptr);
        } else {
            dmz_unreachable("unexpected error, cannot convert in move_vector_ptr");
        }
    }

    source_vector.clear();
    return target_vector;
}

}  // namespace DMZ