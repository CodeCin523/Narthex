add_library(nth_obj_allocator OBJECT
    arena.c
    dyn_arena.c
    free_list.c)

target_include_directories(nth_obj_allocator PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(nth_obj_allocator PUBLIC nth_config)