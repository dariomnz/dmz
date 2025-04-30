file(GLOB C_SOURCES "*.cpp")

add_executable(c ${C_SOURCES})

llvm_map_components_to_libnames(llvm_libs core)

target_link_libraries(c ${llvm_libs})