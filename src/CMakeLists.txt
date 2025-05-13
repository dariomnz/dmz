file(GLOB_RECURSE COMPILER_SOURCES "*.cpp")

add_executable(compiler ${COMPILER_SOURCES})

llvm_map_components_to_libnames(llvm_libs core)

target_link_libraries(compiler ${llvm_libs})