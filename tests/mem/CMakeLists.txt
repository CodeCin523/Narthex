set(NTH_TEST_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(test_mem_arena arena.c)
target_link_libraries(test_mem_arena PRIVATE narthex::narthex)
target_include_directories(test_mem_arena PRIVATE ${NTH_TEST_INCLUDE})
add_test(NAME mem/arena COMMAND test_mem_arena)
set_tests_properties(mem/arena PROPERTIES LABELS "unit")

add_executable(test_mem_dyn_arena dyn_arena.c)
target_link_libraries(test_mem_dyn_arena PRIVATE narthex::narthex)
target_include_directories(test_mem_dyn_arena PRIVATE ${NTH_TEST_INCLUDE})
add_test(NAME mem/dyn_arena COMMAND test_mem_dyn_arena)
set_tests_properties(mem/dyn_arena PROPERTIES LABELS "unit")
