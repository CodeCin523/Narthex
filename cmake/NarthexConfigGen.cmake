set(NTH_CONFIG_DEFINES "")
foreach(c IN LISTS NTH_ALL_MODULES)
    string(TOUPPER "${c}" c_up)
    if(NTH_ENABLE_${c})
        string(APPEND NTH_CONFIG_DEFINES "#define NTH_HAS_${c_up} 1\n")
    else()
        string(APPEND NTH_CONFIG_DEFINES "#define NTH_HAS_${c_up} 0\n")
    endif()
endforeach()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/nth_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/narthex/nth_config.h
    @ONLY)