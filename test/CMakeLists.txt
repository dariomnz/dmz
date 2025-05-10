

cmake_host_system_information(RESULT NUM_PROCS QUERY NUMBER_OF_PHYSICAL_CORES)
math(EXPR NUM_JOBS "${NUM_PROCS} * 2")

add_custom_target(check
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/lit-runner.py ${CMAKE_CURRENT_SOURCE_DIR} -v -s -j ${NUM_JOBS} --per-test-coverage --path=${CMAKE_BINARY_DIR}/bin
    DEPENDS compiler
    USES_TERMINAL)
