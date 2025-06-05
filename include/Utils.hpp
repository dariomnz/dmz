#pragma once

#include <functional>
#include <iostream>
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

[[noreturn]] [[maybe_unused]] static inline void __internal_unreachable(const char* msg, const char* source, int line) {
    std::cerr << "UNREACHABLE at " << source << ':' << line << ": " << msg << std::endl;
    std::abort();
}

struct SourceLocation {
    std::string_view file_name = {};
    size_t line = 0, col = 0;

    std::string to_string() const {
        std::stringstream os;
        os << *this;
        return os.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const SourceLocation& s) {
        os << s.file_name.data() << ":" << s.line + 1 << ":" << s.col + 1;
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

[[maybe_unused]] static inline std::string indent(size_t level) { return std::string(level * 2, ' '); }

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

}  // namespace DMZ