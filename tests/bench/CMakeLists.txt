find_package(Threads REQUIRED)

add_executable(bench_log log.c)
target_link_libraries(bench_log PRIVATE narthex::narthex Threads::Threads)
target_include_directories(bench_log PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME log/bench COMMAND bench_log)
set_tests_properties(log/bench PROPERTIES LABELS "bench")
