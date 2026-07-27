# ctest -L unit        run without hardware
# ctest -L integration require display / GPU

add_executable(test_mem test_mem.c)
target_link_libraries(test_mem PRIVATE narthex::narthex)
add_test(NAME mem COMMAND test_mem)
set_tests_properties(mem PROPERTIES LABELS "unit")

if(NTH_ENABLE_window)
    add_executable(test_window test_window.c)
    target_link_libraries(test_window PRIVATE narthex::narthex)
    add_test(NAME window COMMAND test_window)
    set_tests_properties(window PROPERTIES LABELS "integration")
endif()

if(NTH_ENABLE_vulkan)
    add_executable(test_vulkan test_vulkan.c)
    target_link_libraries(test_vulkan PRIVATE narthex::narthex)
    add_test(NAME vulkan COMMAND test_vulkan)
    set_tests_properties(vulkan PROPERTIES LABELS "integration")
endif()
