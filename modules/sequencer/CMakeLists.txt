add_library(nth_obj_sequencer OBJECT
    tmp.c)

target_include_directories(nth_obj_sequencer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(nth_obj_sequencer PUBLIC nth_config)
