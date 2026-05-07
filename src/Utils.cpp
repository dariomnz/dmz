#include "Utils.hpp"

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

namespace DMZ {

void SourceLocation::dump() const { std::cerr << *this << "\n"; }

std::string SourceLocation::to_string() const {
    std::stringstream os;
    os << *this;
    return os.str();
}

std::ostream& operator<<(std::ostream& os, const SourceLocation& s) {
    os << s.file_name.data() << ":" << s.line << ":" << s.col + 1;
    return os;
}

std::string get_file_line(const std::string& file_name, size_t line_num) {
    static std::mutex cacheMutex;
    static std::unordered_map<std::string, std::vector<std::string>> fileCache;

    std::unique_lock lock(cacheMutex);
    auto it = fileCache.find(file_name);
    if (it == fileCache.end()) {
        std::ifstream file(file_name);
        if (!file.is_open()) return "";
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        it = fileCache.emplace(file_name, std::move(lines)).first;
    }

    if (line_num > 0 && line_num <= it->second.size()) {
        return it->second[line_num - 1];
    }
    return "";
}

[[noreturn]] void __internal_unreachable(const SourceLocation& loc, std::string msg, const char* source, int line) {
    std::stringstream ss;
    ss << "UNREACHABLE at " << source << ':' << line << ": " << msg;
    if (!loc.file_name.empty()) {
        ss << "\n" << loc;
        std::string line_str = get_file_line(loc.file_name, loc.line);
        if (!line_str.empty()) {
            ss << "\n" << line_str;
        }
    }
    std::string error_msg = ss.str();
    // std::cerr << error_msg << std::endl;
    throw std::runtime_error(error_msg);
}

std::nullptr_t report(SourceLocation loc, std::string_view message, bool isWarning) {
    static std::mutex reportMutex;
    std::unique_lock lock(reportMutex);

    bool is_terminal = isatty(STDERR_FILENO);
    const char* red = is_terminal ? "\033[1;31m" : "";
    const char* yellow = is_terminal ? "\033[1;33m" : "";
    const char* reset = is_terminal ? "\033[0m" : "";
    const char* bold = is_terminal ? "\033[1m" : "";

    std::cerr << bold << loc << ":" << reset;
    if (isWarning) {
        std::cerr << yellow << " warning: " << reset;
    } else {
        std::cerr << red << " error: " << reset;
    }
    std::cerr << bold << message << reset << '\n';

    std::string line = get_file_line(loc.file_name, loc.line);
    if (!line.empty()) {
        std::cerr << " " << loc.line << " | ";
        if (is_terminal) {
            std::string before = line.substr(0, std::min(loc.col, line.size()));
            std::string error_part =
                (loc.col < line.size()) ? line.substr(loc.col, std::min(loc.len, line.size() - loc.col)) : "";
            std::string after =
                (loc.col + error_part.size() < line.size()) ? line.substr(loc.col + error_part.size()) : "";
            std::cerr << before << (isWarning ? yellow : red) << bold << error_part << reset << after << '\n';
        } else {
            std::cerr << line << '\n';
        }

        std::cerr << " " << std::string(std::to_string(loc.line).size(), ' ') << " | ";
        for (size_t i = 0; i < loc.col; ++i) {
            if (line[i] == '\t')
                std::cerr << '\t';
            else
                std::cerr << ' ';
        }
        std::cerr << (isWarning ? yellow : red) << bold;
        for (size_t i = 0; i < loc.len; ++i) {
            std::cerr << '^';
        }
        std::cerr << reset << '\n';
    }

    return nullptr;
}

std::ostream& operator<<(std::ostream& os, const indent& di) {
    for (size_t i = 0; i < di.level; i++) {
        os << "  ";
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const indent_line& di) {
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

[[noreturn]] void error(std::string_view msg) {
    std::cerr << "error: " << msg << '\n';
    std::exit(1);
}

std::optional<std::string> str_from_source(std::string_view literal) {
    std::string res = "";
    static const std::unordered_map<char, char> specialChars = {
        {'n', '\n'}, {'t', '\t'}, {'r', '\r'},  {'v', '\v'},  {'b', '\b'},
        {'f', '\f'}, {'a', '\a'}, {'\\', '\\'}, {'\"', '\"'}, {'\'', '\''},
    };
    for (size_t i = 0; i < literal.length(); ++i) {
        if (literal[i] == '\\') {
            i++;
            if (i >= literal.length()) return std::nullopt;

            if (literal[i] == 'x') {
                i++;
                if (i + 1 < literal.length()) {
                    auto hexValue = [](char h) -> int {
                        if (h >= '0' && h <= '9') return h - '0';
                        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                        return -1;
                    };
                    int h = hexValue(literal[i]);
                    int l = hexValue(literal[i + 1]);
                    if (h != -1 && l != -1) {
                        res += static_cast<char>((h << 4) | l);
                        i++;
                    } else {
                        return std::nullopt;
                    }
                } else {
                    return std::nullopt;
                }
            } else if (literal[i] >= '0' && literal[i] <= '7') {
                int val = 0;
                int count = 0;
                while (count < 3 && i < literal.length() && literal[i] >= '0' && literal[i] <= '7') {
                    val = val * 8 + (literal[i] - '0');
                    i++;
                    count++;
                }
                i--;
                res += static_cast<char>(val);
            } else {
                auto it = specialChars.find(literal[i]);
                if (it != specialChars.end()) {
                    res += it->second;
                } else {
                    res += literal[i];
                }
            }
        } else {
            res += literal[i];
        }
    }
    return res;
}

std::string str_to_source(std::string_view str) {
    std::string res = "";
    static const std::unordered_map<char, const char*> specialChars = {
        {'\n', "\\n"}, {'\t', "\\t"}, {'\r', "\\r"},  {'\v', "\\v"},  {'\b', "\\b"},  {'\f', "\\f"},
        {'\a', "\\a"}, {'\0', "\\0"}, {'\\', "\\\\"}, {'\"', "\\\""}, {'\'', "\\\'"},
    };
    for (size_t i = 0; i < str.length(); ++i) {
        unsigned char uc = static_cast<unsigned char>(str[i]);
        auto it = specialChars.find(static_cast<char>(uc));
        if (it != specialChars.end()) {
            res += it->second;
        } else if (uc < 32 || uc > 126) {
            char hex[5];
            snprintf(hex, sizeof(hex), "\\x%02X", uc);
            res += hex;
        } else {
            res += str[i];
        }
    }
    return res;
}

size_t split_count(std::string_view s, std::string_view delimiter) {
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

std::string_view split(std::string_view s, std::string_view delimiter, size_t index) {
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