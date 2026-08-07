add_library(nth_obj_core OBJECT
    core.c
)

target_include_directories(nth_obj_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(nth_obj_core PUBLIC nth_config)
