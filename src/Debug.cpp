#include "Debug.hpp"

#include <chrono>

namespace DMZ {

std::ostream &operator<<(std::ostream &os, [[maybe_unused]] const time_stamp &logger) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm;
    ::localtime_r(&now_c, &local_tm);

    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << local_tm.tm_hour << ":" << std::setw(2) << std::setfill('0')
        << local_tm.tm_min << ":" << std::setw(2) << std::setfill('0') << local_tm.tm_sec << ":" << std::setw(3)
        << std::setfill('0') << milliseconds.count();

    os << oss.str();
    return os;
}

int &debug_instance::get_count() {
    static int count;
    return count;
}

std::ostream &operator<<(std::ostream &os, [[maybe_unused]] const indent_os &logger) {
    auto count = debug_instance::get_count();
    for (int i = 0; i < count; i++) {
        os << "  ";
    }
    return os;
}
}  // namespace DMZ
