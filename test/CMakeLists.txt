
set(TEST_LIST
    # test_files/just_main.dmz
    test_files/decl_fn.dmz
)

# add the executables cpp
foreach (test_name ${TEST_LIST})
    add_test(NAME "${test_name}" COMMAND c ${CMAKE_CURRENT_LIST_DIR}/${test_name})
endforeach (test_name)