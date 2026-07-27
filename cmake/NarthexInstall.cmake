include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(NTH_ENABLED_MODULES "")
foreach(c IN LISTS NTH_ALL_MODULES)
    if(NTH_ENABLE_${c})
        list(APPEND NTH_ENABLED_MODULES ${c})
    endif()
endforeach()

install(TARGETS narthex
    EXPORT  NarthexTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/narthex/nth_config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/narthex)

install(EXPORT NarthexTargets
    FILE      NarthexTargets.cmake
    NAMESPACE narthex::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Narthex)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/NarthexConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NarthexConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Narthex)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/NarthexConfigVersion.cmake
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NarthexConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NarthexConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Narthex)