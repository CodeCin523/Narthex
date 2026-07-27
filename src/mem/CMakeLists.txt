add_library(nth_obj_mem OBJECT mem.c)

target_include_directories(nth_obj_mem PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(nth_obj_mem PUBLIC nth_config)