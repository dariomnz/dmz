#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "UtilsPtr.hpp"

#define DMZ_TYPE_NAME() \
    std::string_view className() const override { return type_name<std::remove_cvref_t<decltype(*this)>>(); }

template <typename T>
constexpr auto type_name() {
    std::string_view name, prefix, suffix;
#ifdef __clang__
    name = __PRETTY_FUNCTION__;
    prefix = "auto type_name() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
    prefix = "constexpr auto type_name() [with T = ";
    suffix = "]";
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
    prefix = "auto __cdecl type_name<";
    suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

namespace DMZ {

#define varOrReturn(var, init) \
    auto var = (init);         \
    if (!var) return nullptr;
#define matchOrReturn(tok, msg) \
    if (m_nextToken.type != tok) return report(m_nextToken.loc, msg);

#define dmz_unreachable(loc, msg) ::DMZ::__internal_unreachable((loc), (msg), __FILE__, __LINE__)

struct SourceLocation {
    std::string file_name = {};
    size_t line = 0, col = 0, len = 1;

    static SourceLocation builtin() { return SourceLocation{"<builtin>", 0, 0, 1}; }

    std::string to_string() const;

    friend std::ostream& operator<<(std::ostream& os, const SourceLocation& s);
};

std::string get_file_line(const std::string& file_name, size_t line_num);

[[noreturn]] void __internal_unreachable(const SourceLocation& loc, std::string msg, const char* source, int line);

std::nullptr_t report(SourceLocation loc, std::string_view message, bool isWarning = false);

struct indent {
    size_t level;

    indent(size_t level) : level(level) {}

    friend std::ostream& operator<<(std::ostream& os, const indent& di);
};

struct indent_line {
    size_t level = 0;
    size_t extra_level = 0;
    bool horizontal = false;

    indent_line(size_t level, size_t extra_level, bool horizontal)
        : level(level), extra_level(extra_level), horizontal(horizontal) {}

    friend std::ostream& operator<<(std::ostream& os, const indent_line& di);
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

[[noreturn]] void error(std::string_view msg);

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

std::optional<std::string> str_from_source(std::string_view literal);

std::string str_to_source(std::string_view str);

size_t split_count(std::string_view s, std::string_view delimiter);

std::string_view split(std::string_view s, std::string_view delimiter, size_t index);

template <typename from, typename to>
vec<ptr<to>> move_vector_ptr(vec<ptr<from>>& source_vector) {
    std::vector<std::unique_ptr<to>> target_vector;
    target_vector.reserve(source_vector.size());

    for (auto& ptr_derived : source_vector) {
        if (auto ptr = dynamic_cast<to*>(ptr_derived.release())) {
            target_vector.emplace_back(ptr);
        } else {
            dmz_unreachable(SourceLocation{}, "unexpected error, cannot convert in move_vector_ptr");
        }
    }

    source_vector.clear();
    return target_vector;
}

}  // namespace DMZ