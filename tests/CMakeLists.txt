# One directory per domain, one file per test, named after what it runs:
#   unit_*   run without hardware        ctest -L unit
#   test_*   require display / GPU       ctest -L integration
#   bench_*  performance comparison      ctest -L bench, no pass/fail

set(NTH_TEST_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(mem)
add_subdirectory(log)
add_subdirectory(path)
add_subdirectory(time)

if(NTH_ENABLE_vulkan)
    add_subdirectory(vulkan)
endif()

add_executable(unit_lifecycle unit_lifecycle.c)
target_link_libraries(unit_lifecycle PRIVATE narthex::narthex)
target_include_directories(unit_lifecycle PRIVATE ${NTH_TEST_INCLUDE})
add_test(NAME lifecycle COMMAND unit_lifecycle)
set_tests_properties(lifecycle PROPERTIES LABELS "unit")