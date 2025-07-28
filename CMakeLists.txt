cmake_minimum_required(VERSION 3.22)
project(unicorn VERSION 1.1.0 LANGUAGES C)

include(CheckIncludeFile)

option(UNICORN_BUILD_ARCHIVES "Build the amalgamated release archives." OFF)

set(UNICORN_CONFIG "features.json" CACHE STRING "Path to a JSON file that configures the Unicorn feature set.")
if (UNICORN_CONFIG STREQUAL "")
    message(FATAL_ERROR "please specify a unicorn configuration file (must be a JSON file)")
endif()

# Check for required C standard library headers.
check_include_file(string.h HAVE_STRING_H)
if (NOT HAVE_STRING_H)
    message(FATAL_ERROR "could not find required header")
endif ()

check_include_file(assert.h HAVE_ASSERT_H)
if (NOT HAVE_ASSERT_H)
    message(FATAL_ERROR "could not find required header")
endif ()

check_include_file(stdbool.h HAVE_STDBOOL_H)
if (NOT HAVE_STDBOOL_H)
    message(FATAL_ERROR "could not find required header")
endif ()

# Python is required to generate the amalgamated source files.
find_package(Python3 3.10.0 REQUIRED COMPONENTS Interpreter)

set(GENERATED_FILES
    "${CMAKE_BINARY_DIR}/unicorn.c"
    "${CMAKE_BINARY_DIR}/unicorn.h"
)
add_custom_command(
    OUTPUT ${GENERATED_FILES}
    COMMAND Python3::Interpreter generate.pyz --config "${UNICORN_CONFIG}" --output "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS generate.pyz unicorn.bin ${UNICORN_CONFIG})
add_custom_target(unicorn_generate DEPENDS ${GENERATED_FILES})

include_directories(${CMAKE_BINARY_DIR}) # Ensure the generated header is discoverable.
set_source_files_properties(${CMAKE_BINARY_DIR}/unicorn.c PROPERTIES GENERATED 1)
set_source_files_properties(${CMAKE_BINARY_DIR}/unicorn.h PROPERTIES GENERATED 1)

set(SOURCES
    ${CMAKE_BINARY_DIR}/unicorn.c
    ${CMAKE_BINARY_DIR}/unicorn.h
)

add_library(unicorn STATIC ${SOURCES})
add_dependencies(unicorn unicorn_generate)

# Generate version information.
include(CMakePackageConfigHelpers)
write_basic_package_version_file(${CMAKE_BINARY_DIR}/UnicornConfigVersion.cmake COMPATIBILITY SameMajorVersion)

if (UNICORN_BUILD_ARCHIVES)
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-amalgamation-${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VENDOR "Railgun Labs")
    set(CPACK_PACKAGE_CONTACT "Railgun Labs <https://RailgunLabs.com>")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://RailgunLabs.com")
    set(CPACK_GENERATOR "ZIP;TGZ")
    # These are the files to copy to the amalgamation archive.
    install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION .)
    install(FILES ${CMAKE_BINARY_DIR}/unicorn.c DESTINATION .)
    install(FILES ${CMAKE_BINARY_DIR}/unicorn.h DESTINATION .)
    include(CPack)
else ()
    # Pre-process pkg-config and UnicornConfig.cmake files.
    set(prefix "${CMAKE_INSTALL_PREFIX}")
    set(exec_prefix "\${prefix}")
    set(libdir "\${exec_prefix}/lib")
    set(includedir "\${prefix}/include")
    set(VERSION ${PROJECT_VERSION})
    configure_file("${CMAKE_SOURCE_DIR}/UnicornConfig.cmake.in" "${CMAKE_BINARY_DIR}/UnicornConfig.cmake" @ONLY)
    configure_file("${CMAKE_SOURCE_DIR}/unicorn.pc.in" "${CMAKE_BINARY_DIR}/unicorn.pc" @ONLY)

    # Only install man pages when running "make install" locally.
    # No need to place them in the amalgamated archive.
    add_subdirectory(man)

    # This is the "make install" action.
    set_target_properties(unicorn PROPERTIES PUBLIC_HEADER ${CMAKE_BINARY_DIR}/unicorn.h)
    install(TARGETS unicorn ARCHIVE DESTINATION lib PUBLIC_HEADER DESTINATION include)
    install(FILES ${CMAKE_BINARY_DIR}/UnicornConfig.cmake DESTINATION cmake)
    install(FILES ${CMAKE_BINARY_DIR}/UnicornConfigVersion.cmake DESTINATION cmake)
    install(FILES ${CMAKE_BINARY_DIR}/unicorn.pc DESTINATION lib/pkgconfig)
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif ()
