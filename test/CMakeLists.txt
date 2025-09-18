

cmake_host_system_information(RESULT NUM_PROCS QUERY NUMBER_OF_PHYSICAL_CORES)
math(EXPR NUM_JOBS "${NUM_PROCS} * 2")

add_custom_target(check
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/lit-runner.py" -v -s -j "${NUM_JOBS}" --timeout 2 --per-test-coverage --path="${CMAKE_BINARY_DIR}/bin" 
    "${CMAKE_CURRENT_SOURCE_DIR}/parser"
    "${CMAKE_CURRENT_SOURCE_DIR}/sema"
    "${CMAKE_CURRENT_SOURCE_DIR}/cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/constexpr"
    "${CMAKE_CURRENT_SOURCE_DIR}/codegen"
    "${CMAKE_CURRENT_SOURCE_DIR}/driver"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples"
    DEPENDS compiler
    USES_TERMINAL
)
