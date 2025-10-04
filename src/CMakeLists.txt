file(GLOB_RECURSE DMZ_SOURCES "*.cpp")

add_executable(dmz ${DMZ_SOURCES})

llvm_map_components_to_libnames(llvm_libs ${LLVM_TARGETS_TO_BUILD} core)

target_link_libraries(dmz ${llvm_libs})

target_precompile_headers(dmz 
    PRIVATE ../include/DMZPCH.hpp 
    PRIVATE ../include/DMZPCHSymbols.hpp 
    PRIVATE ../include/DMZPCHLLVM.hpp
)