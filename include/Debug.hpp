#pragma once

#include <iosfwd>

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
    friend std::ostream &operator<<(std::ostream &os, const time_stamp &logger);
};

class debug_instance {
   public:
    static int &get_count();
};

struct indent_os {
    friend std::ostream &operator<<(std::ostream &os, [[maybe_unused]] const indent_os &logger);
};

#ifdef DEBUG
#define debug_ret(ret)                                          \
    ({                                                          \
        auto return_value = std::move(ret);                     \
        debug_msg_func(__func__, "Returning " << return_value); \
        std::move(return_value);                                \
    })
#define debug_msg(out_format) debug_msg_func(__func__, out_format)
#define debug_msg_func(func, out_format)                                                                             \
    {                                                                                                                \
        std::cerr << indent_os{} << std::dec << ::DMZ::time_stamp() << " [" << ::DMZ::get_file_name(__FILE__) << ":" \
                  << __LINE__ << "] [" << func << "] " << out_format << std::endl;                                   \
    }
#define debug_func(out_format)                                                       \
    auto ____func_name = __func__;                                                   \
    debug_msg("BEGIN " << ____func_name << " " << out_format);                       \
    debug_instance::get_count()++;                                                   \
    defer([&] {                                                                      \
        debug_instance::get_count()--;                                               \
        debug_msg_func(____func_name, "END " << ____func_name << " " << out_format); \
    });
#else
#define debug_ret(ret) ret
#define debug_msg(out_format)
#define debug_msg_func(func, out_format)
#define debug_func(out_format)
#endif

#ifdef DMZ_SINGLE_THREADED
#define println(out_format)                               \
    {                                                     \
        std::cout << std::dec << out_format << std::endl; \
    }
#else  // DMZ_SINGLE_THREADED
#define println(out_format)                                                                            \
    {                                                                                                  \
        std::cout << "[" << std::this_thread::get_id() << "] " << std::dec << out_format << std::endl; \
    }
#endif  // DMZ_SINGLE_THREADED
#define TODO(msg) assert(false && "TODO" && msg)
}  // namespace DMZ
