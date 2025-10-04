cmake_minimum_required(VERSION 3.12)
project(compiler)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
# set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Coverage")
    message("Coverage activated")
    add_compile_options("-ggdb" "-O0" "-fprofile-instr-generate" "-fcoverage-mapping")
    add_link_options( "-fprofile-instr-generate" "-fcoverage-mapping")
else()
    add_compile_options("-O0")
endif()

# add_compile_options(-fsanitize=thread)
# add_link_options(-fsanitize=thread)

find_package(LLVM REQUIRED CONFIG)
include_directories(include "${LLVM_INCLUDE_DIR}")

add_compile_options("-DDMZ_SINGLE_THREADED")
add_compile_options("-DDEBUG")
add_compile_options("-ggdb" "-Wall" "-Wextra")
include_directories(include)

add_subdirectory(src)
add_subdirectory(test)