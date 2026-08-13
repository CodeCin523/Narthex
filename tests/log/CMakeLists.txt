find_package(Threads REQUIRED)

add_executable(unit_log unit_log.c)
target_link_libraries(unit_log PRIVATE narthex::narthex Threads::Threads)
target_include_directories(unit_log PRIVATE ${NTH_TEST_INCLUDE})
add_test(NAME log/unit COMMAND unit_log)
set_tests_properties(log/unit PROPERTIES LABELS "unit")

add_executable(bench_log bench_log.c)
target_link_libraries(bench_log PRIVATE narthex::narthex Threads::Threads)
target_include_directories(bench_log PRIVATE ${NTH_TEST_INCLUDE})
add_test(NAME log/bench COMMAND bench_log)
set_tests_properties(log/bench PROPERTIES LABELS "bench")
