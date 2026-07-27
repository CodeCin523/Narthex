cmake_minimum_required(VERSION 3.23)
project(Narthex VERSION 0.1.0 LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(NarthexModules)
include(modules.cmake)
nth_resolve_modules()
nth_print_summary()

include(NarthexConfigGen)

add_library(nth_config INTERFACE)
target_include_directories(nth_config INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

set(NTH_BASE_MODULES mem) # lifetime logger path loader
foreach(m IN LISTS NTH_BASE_MODULES)
    add_subdirectory(src/${m})
endforeach()

foreach(c IN LISTS NTH_ALL_MODULES)
    if(NTH_ENABLE_${c})
        add_subdirectory(modules/${c})
    endif()
endforeach()

set(NTH_OBJECTS "")
foreach(m IN LISTS NTH_BASE_MODULES)
    list(APPEND NTH_OBJECTS $<TARGET_OBJECTS:nth_obj_${m}>)
endforeach()
foreach(c IN LISTS NTH_ALL_MODULES)
    if(NTH_ENABLE_${c})
        list(APPEND NTH_OBJECTS $<TARGET_OBJECTS:nth_obj_${c}>)
    endif()
endforeach()

add_library(narthex ${NTH_OBJECTS})
add_library(narthex::narthex ALIAS narthex)

target_include_directories(narthex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)

include(NarthexInstall)

option(NTH_BUILD_TESTS "Build Narthex tests" OFF)
if(NTH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
