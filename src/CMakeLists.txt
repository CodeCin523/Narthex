add_library(narthex ${NTH_LIB_KIND}
    narthex.c
    nth_log.c
    nth_path.c
    nth_uptime.c
    mem/arena.c
    mem/dyn_arena.c)

add_library(narthex::narthex ALIAS narthex)

if(NTH_SHARED)
    set_target_properties(narthex PROPERTIES
        VERSION   ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})
endif()

target_link_libraries(narthex PRIVATE $<BUILD_INTERFACE:nth_config>)

target_include_directories(narthex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>)

target_compile_features(narthex INTERFACE c_std_99)
