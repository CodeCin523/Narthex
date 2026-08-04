add_library(nth_obj_log OBJECT
    nth_log.c
)

target_include_directories(nth_obj_log PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(nth_obj_log PUBLIC nth_config)