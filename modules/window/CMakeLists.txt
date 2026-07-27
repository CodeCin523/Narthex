add_library(nth_obj_window OBJECT
    window.c)

target_include_directories(nth_obj_window PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(nth_obj_window PUBLIC
    nth_obj_core
    nth_obj_lifetime)