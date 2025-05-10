cmake_minimum_required(VERSION 3.12)
project(compiler)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Coverage")
    message("Coverage activated")
    add_compile_options("-ggdb" "-O0" "-fprofile-instr-generate" "-fcoverage-mapping")
    add_link_options( "-fprofile-instr-generate" "-fcoverage-mapping")
else()
    add_compile_options("-O3")
endif()

find_package(LLVM REQUIRED CONFIG)
include_directories(include "${LLVM_INCLUDE_DIR}")

# add_compile_options("-DDEBUG")
add_compile_options("-ggdb" "-Wall" "-Wextra")
include_directories(include)

add_subdirectory(src)
add_subdirectory(test)