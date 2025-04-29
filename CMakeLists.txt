cmake_minimum_required(VERSION 3.10)
project(c)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add_compile_options("-DDEBUG")
add_compile_options("-O3" "-ggdb" "-Wall" "-Wextra")
include_directories(include)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)