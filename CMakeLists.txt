cmake_minimum_required(VERSION 3.22)
project(unicorn VERSION 1.0.0 LANGUAGES C)

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
    COMMAND Python3::Interpreter amalgamate.pyz --config "${UNICORN_CONFIG}" --output "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS amalgamate.pyz unicorn.bin ${UNICORN_CONFIG})
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

if (UNICORN_BUILD_ARCHIVES)
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-amalgamated-${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VENDOR "Railgun Labs")
    set(CPACK_PACKAGE_CONTACT "Railgun Labs <https://RailgunLabs.com>")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://RailgunLabs.com")
    set(CPACK_GENERATOR "ZIP;TGZ")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" DESTINATION .)
    install(FILES "${CMAKE_BINARY_DIR}/unicorn.c" DESTINATION .)
    install(FILES "${CMAKE_BINARY_DIR}/unicorn.h" DESTINATION .)
    include(CPack)
else ()
    # This is the "make install" action.
    set_target_properties(unicorn PROPERTIES PUBLIC_HEADER ${CMAKE_BINARY_DIR}/unicorn.h)
    install(TARGETS unicorn ARCHIVE
            DESTINATION lib
            PUBLIC_HEADER DESTINATION include)
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif ()
