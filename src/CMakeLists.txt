file(GLOB_RECURSE COMPILER_SOURCES "*.cpp")

add_executable(compiler ${COMPILER_SOURCES})

llvm_map_components_to_libnames(llvm_libs ${LLVM_TARGETS_TO_BUILD} core)

target_link_libraries(compiler ${llvm_libs})

target_precompile_headers(compiler 
    PRIVATE ../include/DMZPCH.hpp 
    PRIVATE ../include/DMZPCHSymbols.hpp 
    PRIVATE ../include/DMZPCHLLVM.hpp
)