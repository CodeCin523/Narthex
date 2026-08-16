cmake_minimum_required(VERSION 3.23)
project(Narthex VERSION 0.1.0 LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(NarthexModules)
include(modules.cmake)
nth_resolve_modules()
nth_print_summary()

option(NTH_SHARED "Build narthex as a shared library" ON)
option(NTH_POISON "Fill discarded arena memory with a pattern" OFF)
option(NTH_ASAN   "Build with AddressSanitizer" OFF)
option(NTH_UBSAN  "Build with UndefinedBehaviorSanitizer (not available on MSVC)" OFF)

include(NarthexConfigGen)

if(NTH_SHARED)
    set(NTH_LIB_KIND SHARED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON) # object libraries do not inherit it
else()
    set(NTH_LIB_KIND STATIC)
endif()

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

target_compile_features(nth_config INTERFACE c_std_17)

target_compile_options(nth_config INTERFACE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wshadow -Wvla>
    $<$<C_COMPILER_ID:MSVC>:/W4>)

target_compile_definitions(nth_config INTERFACE NTH_BUILDING=1)

if(NTH_SHARED)
    target_compile_options(nth_config INTERFACE
        $<$<C_COMPILER_ID:GNU,Clang>:-fvisibility=hidden>)
endif()

add_subdirectory(src)

foreach(c IN LISTS NTH_ALL_MODULES)
    if(NTH_ENABLE_${c})
        add_subdirectory(modules/${c})
        target_sources(narthex PRIVATE $<TARGET_OBJECTS:nth_obj_${c}>)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/modules/${c}/include)
            target_include_directories(narthex PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/modules/${c}/include>)
        endif()
    endif()
endforeach()

include(NarthexInstall)

option(NTH_BUILD_TESTS "Build Narthex tests" OFF)
if(NTH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
