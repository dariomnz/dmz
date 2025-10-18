#pragma once

#include "DMZPCH.hpp"

namespace DMZ {

struct profiler_data {
    pid_t m_pid;
    std::thread::id m_tid;
    std::variant<const char*, std::string> m_name;
    uint32_t m_start;
    uint32_t m_duration;

    profiler_data(pid_t pid, std::thread::id tid, std::variant<const char*, std::string> name, uint32_t start,
                  uint32_t duration)
        : m_pid(pid), m_tid(tid), m_name(name), m_start(start), m_duration(duration) {}

    void dump_data(std::ostream& json) const {
        json << ",{";
        json << "\"cat\":\"function\",";
        json << "\"dur\":" << m_duration << ',';
        if (std::holds_alternative<const char*>(m_name)) {
            json << "\"name\":\"" << std::get<const char*>(m_name) << "\",";
        } else {
            json << "\"name\":\"" << std::get<std::string>(m_name) << "\",";
        }
        json << "\"ph\":\"X\",";
        json << "\"pid\":\"" << m_pid << "\",";
        json << "\"tid\":" << m_tid << ",";
        json << "\"ts\":" << m_start;
        json << "}\n";
    }
};

class profiler {
   public:
    profiler(const profiler&) = delete;
    profiler(profiler&&) = delete;

    void begin_session(const std::string& name) {
        std::lock_guard lock(m_mutex);
        m_buffer = std::vector<profiler_data>();
        m_buffer.reserve(m_buffer_cap);
        m_current_session_file = name;

        std::ofstream archivo_salida(m_current_session_file, std::ios::out);

        if (archivo_salida.is_open()) {
            archivo_salida << get_header();
            archivo_salida.close();
        } else {
            std::cerr << "ERROR: Could not open file " << m_current_session_file << " for writing.\n";
        }
    }

    void end_session() {
        std::lock_guard lock(m_mutex);
        if (m_buffer.size() > 0) {
            save_data(std::move(m_buffer));
        }
        for (auto& fut : m_fut_save_data) {
            if (fut.valid()) {
                fut.get();
            }
        }
        std::ofstream archivo_salida(m_current_session_file, std::ios::out | std::ios::app);

        if (archivo_salida.is_open()) {
            archivo_salida << get_footer();
            archivo_salida.close();
        } else {
            std::cerr << "ERROR: Could not open file " << m_current_session_file << " for writing.\n";
        }
    }

    void write_profile(std::variant<const char*, std::string> name, uint32_t start, uint32_t duration) {
        std::lock_guard lock(m_mutex);
        if (!m_current_session_file.empty()) {
            // debug_msg("Buffer size " << m_buffer.size());
            m_buffer.emplace_back(getpid(), std::this_thread::get_id(), name, start, duration);

            if (m_buffer.size() >= m_buffer_cap) {
                save_data(std::move(m_buffer));
                m_buffer = std::vector<profiler_data>();
                m_buffer.reserve(m_buffer_cap);
            }
        }
    }

    static std::string get_header() { return "{\"otherData\": {},\"traceEvents\":[{}\n"; }
    static std::string get_footer() { return "]}"; }

    static profiler& get_instance() {
        static profiler instance;
        return instance;
    }

   private:
    profiler() {}
    ~profiler() {}

    void save_data(const std::vector<profiler_data>&& message) {
        m_fut_save_data.remove_if([](auto& fut) {
            if (fut.valid() && fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                fut.get();
                return true;
            }
            return false;
        });
        m_fut_save_data.push_back(
            std::async(std::launch::async, [msg = std::move(message), current_session_file = m_current_session_file]() {
                std::stringstream ss;
                for (auto& data : msg) {
                    data.dump_data(ss);
                }
                std::string str = ss.str();
                // debug_msg("profiler send msgs " << msg.size() << " and " << str.size() << " str size");

                std::ofstream archivo_salida(current_session_file, std::ios::out | std::ios::app);

                if (archivo_salida.is_open()) {
                    archivo_salida << str;
                    archivo_salida.close();
                    return 0;
                } else {
                    std::cerr << "ERROR: Could not open file " << current_session_file << " for writing.\n";
                    return -1;
                }
                return 0;
            }));
    }

   private:
    std::mutex m_mutex;
    std::string m_current_session_file = "";
    std::list<std::future<int>> m_fut_save_data;
    constexpr static uint64_t m_buffer_cap = 1024;
    std::vector<profiler_data> m_buffer;
};

class profiler_timer {
   public:
    profiler_timer(const char* name) : m_name(name) { m_start_timepoint = std::chrono::high_resolution_clock::now(); }

    template <typename... Args>
    profiler_timer(Args... args) {
        m_start_timepoint = std::chrono::high_resolution_clock::now();
        m_name_str = concatenate(args...);
    }

    template <typename T>
    std::string toString(const T& value) {
        std::stringstream ss;
        ss << value;
        return ss.str();
    }

    template <typename T, typename... Rest>
    std::string concatenate(const T& value, const Rest&... rest) {
        std::string result = toString(value);
        if constexpr (sizeof...(rest) > 0) {
            result += ", " + concatenate(rest...);
        }
        return result;
    }

    ~profiler_timer() {
        if (!m_stopped) Stop();
    }

    void Stop() {
        auto start =
            std::chrono::duration_cast<std::chrono::microseconds>(m_start_timepoint.time_since_epoch()).count();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::high_resolution_clock::now() - m_start_timepoint)
                                .count();
        if (m_name == nullptr) {
            profiler::get_instance().write_profile(m_name_str, start, elapsed_time);
        } else {
            profiler::get_instance().write_profile(m_name, start, elapsed_time);
        }
        m_stopped = true;
    }

   private:
    const char* m_name = nullptr;
    std::string m_name_str;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start_timepoint;
    bool m_stopped = false;
};
}  // namespace DMZ

#define dmz_profile 0
#if dmz_profile
#define dmz_profile_begin_session(name)         ::DMZ::profiler::get_instance().begin_session(name)
#define dmz_profile_end_session()               ::DMZ::profiler::get_instance().end_session()
#define dmz_profile_scope_line2(name, line)     ::DMZ::profiler_timer timer##line(name)
#define dmz_profile_scope_line(name, line)      dmz_profile_scope_line2(name, line)
#define dmz_profile_scope(name)                 dmz_profile_scope_line(name, __LINE__)
#define dmz_profile_function()                  dmz_profile_scope(__func__)

#define dmz_profile_scope_args_line2(line, ...) ::DMZ::profiler_timer timer##line(__VA_ARGS__)
#define dmz_profile_scope_args_line(line, ...)  dmz_profile_scope_args_line2(line, __VA_ARGS__)
#define dmz_profile_scope_args(name, ...)       dmz_profile_scope_args_line(__LINE__, name, __VA_ARGS__)
#define dmz_profile_function_args(...)          dmz_profile_scope_args(__func__, __VA_ARGS__)
#else
#define dmz_profile_begin_session(name)
#define dmz_profile_end_session()
#define dmz_profile_scope(name)
#define dmz_profile_function()

#define dmz_profile_scope_args(name, ...)
#define dmz_profile_function_args(...)
#endif