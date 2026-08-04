cmake_minimum_required(VERSION 3.23)
project(Narthex VERSION 0.1.0 LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(NarthexModules)
include(modules.cmake)
nth_resolve_modules()
nth_print_summary()

include(NarthexConfigGen)

option(NTH_POISON "Fill discarded arena memory with a pattern" OFF)
option(NTH_ASAN   "Build with AddressSanitizer" OFF)
option(NTH_UBSAN  "Build with UndefinedBehaviorSanitizer (not available on MSVC)" OFF)

if(NTH_ASAN)
    if(MSVC)
        string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
        add_compile_options(/fsanitize=address)
        add_link_options(/INCREMENTAL:NO)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    else()
        message(FATAL_ERROR "NTH_ASAN: no AddressSanitizer support known for ${CMAKE_C_COMPILER_ID}")
    endif()
endif()

if(NTH_UBSAN)
    if(MSVC)
        message(FATAL_ERROR "NTH_UBSAN: MSVC has no UndefinedBehaviorSanitizer. Use NTH_ASAN instead.")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=undefined)
    else()
        message(FATAL_ERROR "NTH_UBSAN: no UndefinedBehaviorSanitizer support known for ${CMAKE_C_COMPILER_ID}")
    endif()
endif()

add_library(nth_config INTERFACE)
target_include_directories(nth_config INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

if(NTH_POISON)
    target_compile_definitions(nth_config INTERFACE NTH_POISON=1)
endif()

set(NTH_BASE_MODULES mem log) # lifetime path loader
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

target_compile_features(narthex
    PRIVATE   c_std_17
    INTERFACE c_std_99)

target_compile_options(narthex PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wshadow -Wvla>
    $<$<C_COMPILER_ID:MSVC>:/W4>)

include(NarthexInstall)

option(NTH_BUILD_TESTS "Build Narthex tests" OFF)
if(NTH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
