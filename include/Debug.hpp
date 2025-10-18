#pragma once

#include "DMZPCH.hpp"
#include "Profiler.hpp"
namespace DMZ {

constexpr const char *get_file_name(const char *path) {
    const char *file = path;
    while (*path) {
        if (*path++ == '/') {
            file = path;
        }
    }
    return file;
}
struct time_stamp {
    friend std::ostream &operator<<(std::ostream &os, [[maybe_unused]] const time_stamp &logger) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm = *std::localtime(&now_c);

        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << local_tm.tm_hour << ":" << std::setw(2) << std::setfill('0')
            << local_tm.tm_min << ":" << std::setw(2) << std::setfill('0') << local_tm.tm_sec << ":" << std::setw(3)
            << std::setfill('0') << milliseconds.count();

        os << oss.str();
        return os;
    }
};

class debug_lock {
   public:
    static std::recursive_mutex &get_lock() {
        static std::recursive_mutex mutex;
        return mutex;
    }
    static uint32_t &get_count() {
        static uint32_t count;
        return count;
    }
};

struct indent_os {
    friend std::ostream &operator<<(std::ostream &os, [[maybe_unused]] const indent_os &logger) {
        auto count = debug_lock::get_count();
        for (size_t i = 0; i < count; i++) {
            os << "  ";
        }
        return os;
    }
};

#ifdef DEBUG
template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
    os << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        os << vec[i];
        if (i < vec.size() - 1) {
            os << ", ";
        }
    }
    os << "]";
    return os;
}
#define debug_ret(ret)                                          \
    ({                                                          \
        auto return_value = ret;                                \
        debug_msg_func(__func__, "Returning " << return_value); \
        return_value;                                           \
    })
#define debug_msg(out_format) debug_msg_func(__func__, out_format)
#define debug_msg_func(func, out_format)                                                                             \
    {                                                                                                                \
        std::unique_lock internal_debug_lock(::DMZ::debug_lock::get_lock());                                         \
        std::cerr << indent_os{} << std::dec << ::DMZ::time_stamp() << " [" << ::DMZ::get_file_name(__FILE__) << ":" \
                  << __LINE__ << "] [" << func << "] " << out_format << std::endl;                                   \
    }
#define debug_func(out_format)                                                       \
    dmz_profile_function();                                                          \
    auto ____func_name = __func__;                                                   \
    debug_msg("BEGIN " << ____func_name << " " << out_format);                       \
    debug_lock::get_count()++;                                                       \
    defer([&] {                                                                      \
        debug_lock::get_count()--;                                                   \
        debug_msg_func(____func_name, "END " << ____func_name << " " << out_format); \
    });
#else
#define debug_ret(ret) ret
#define debug_msg(out_format)
#define debug_msg_func(func, out_format)
#define debug_func(out_format) dmz_profile_function();
#endif

#ifdef DMZ_SINGLE_THREADED
#define println(out_format)                                                  \
    {                                                                        \
        std::unique_lock internal_debug_lock(::DMZ::debug_lock::get_lock()); \
        std::cout << std::dec << out_format << std::endl;                    \
    }
#else  // DMZ_SINGLE_THREADED
#define println(out_format)                                                                            \
    {                                                                                                  \
        std::unique_lock internal_debug_lock(::DMZ::debug_lock::get_lock());                           \
        std::cout << "[" << std::this_thread::get_id() << "] " << std::dec << out_format << std::endl; \
    }
#endif  // DMZ_SINGLE_THREADED
#define TODO(msg) assert(false && "TODO" && msg)
}  // namespace DMZ
