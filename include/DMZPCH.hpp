#pragma once

#include <wait.h>

#include <cassert>
#include <charconv>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stack>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "ThreadPool.hpp"
#include "Utils.hpp"

template <typename T>
using ptr = std::unique_ptr<T>;
template <typename T, typename... Args>
constexpr ptr<T> makePtr(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}
template <typename T_out, typename T_in>
std::unique_ptr<T_out> castPtr(std::unique_ptr<T_in>&& ptr) {
    return std::unique_ptr<T_out>(static_cast<T_out*>(ptr.release()));
}

template <typename T>
using ref = std::shared_ptr<T>;
template <typename T, typename... Args>
constexpr ref<T> makeRef(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}