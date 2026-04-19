#pragma once

#include "DMZPCH.hpp"

namespace DMZ {

template <typename T>
using vec = std::vector<T>;

template <typename T>
using ptr = std::unique_ptr<T>;
template <typename T, typename... Args>
inline ptr<T> makePtr(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}
template <typename T_out, typename T_in>
inline std::unique_ptr<T_out> castPtr(std::unique_ptr<T_in>&& ptr) {
    return std::unique_ptr<T_out>(static_cast<T_out*>(ptr.release()));
}

template <typename T>
using ref = std::shared_ptr<T>;
template <typename T, typename... Args>
inline ref<T> makeRef(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

}  // namespace DMZ