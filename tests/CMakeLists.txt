# One directory per domain, one file per test, named after what it runs:
#   unit_*   run without hardware        ctest -L unit
#   test_*   require display / GPU       ctest -L integration
#   bench_*  performance comparison      ctest -L bench, no pass/fail

set(NTH_TEST_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(mem)
add_subdirectory(log)
add_subdirectory(path)

if(NTH_ENABLE_vulkan)
    add_subdirectory(vulkan)
endif()
