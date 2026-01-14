cmake_minimum_required(VERSION 3.22)
project(unicorn VERSION 1.3.2 LANGUAGES C)

include(CheckIncludeFile)
include(GNUInstallDirs)

option(UNICORN_BUILD_STATIC "Build Unicorn as a static library" ON)
option(UNICORN_BUILD_SHARED "Build Unicorn as a dynamic library" ON)
option(UNICORN_BUILD_EXAMPLES "Build Unicorn code examples (requires the shared library)" OFF)

option(UNICORN_BUILD_TESTS "Build the tests (commercial licensees only)" OFF)
option(UNICORN_BUILD_COOKBOOK "Build the documentation/cookbook programs (commercial licensees only)" OFF)

option(UNICORN_CODE_COVERAGE "Toggle code coverage" OFF)
option(UNICORN_UNDEFINED_BEHAVIOR_SANITIZER "Toggle undefined behavior sanitizer" OFF)
option(UNICORN_ADDRESS_SANITIZER "Toggle address sanitizer" OFF)
option(UNICORN_MEMORY_SANITIZER "Toggle address sanitizer" OFF)

if (NOT UNICORN_BUILD_STATIC AND NOT UNICORN_BUILD_SHARED)
    message(FATAL_ERROR "Please build either the static or shared library.")
endif ()

set(UNICORN_CONFIG "features.json" CACHE STRING "Path to a JSON file that configures the Unicorn feature set.")
if (UNICORN_CONFIG STREQUAL "")
    message(FATAL_ERROR "please specify a unicorn configuration file (must be a JSON file)")
endif()

if (MSVC)
    # The Microsoft compiler assumes source is ANSI-encoded, which depends on the localized version of Windows in use
    # On U.S. and Western European Windows the encoding is assumed to be Windows-1252. Adding the following compiler
    # switch forces UTF-8.
    add_compile_options(/utf-8)
endif ()

if (UNICORN_CODE_COVERAGE)
    if (NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "Code coverage can only be generated with GCC.")
    endif ()
    # You can't have sanitizers enabled with coverage checks.
elseif (UNICORN_UNDEFINED_BEHAVIOR_SANITIZER)
    add_compile_options(-g -fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
elseif (UNICORN_ADDRESS_SANITIZER)
    add_compile_options(-g -fsanitize=address)
    add_link_options(-fsanitize=address)
elseif (UNICORN_MEMORY_SANITIZER)
    add_compile_options(-O0 -g -fsanitize=memory -fsanitize-memory-track-origins)
    add_link_options(-fsanitize=memory -fsanitize-memory-track-origins)
endif ()

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

# Python is required to generate the Unicode data tables.
find_package(Python3 3.10.0 REQUIRED COMPONENTS Interpreter)

# Where the generated source files are built.
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Execute the Python Unicode data generators.
file(GLOB_RECURSE PYTHON_SOURCES "scripts/${PYTHON_SOURCES}*.py")
set(GENERATED_FILES
    "${GENERATED_DIR}/unidata.c"
    "${GENERATED_DIR}/unidata.h"
    "${GENERATED_DIR}/_unicorn.h"
)
add_custom_command(
    OUTPUT ${GENERATED_FILES}
    COMMAND Python3::Interpreter -m scripts --config "${UNICORN_CONFIG}" --output "${GENERATED_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS unicode.bin ${PYTHON_SOURCES} ${UNICORN_CONFIG})
add_custom_target(unicorn_generate DEPENDS ${GENERATED_FILES})

# Mark the following source files as being generated at build time so CMake doesn't try to find them.
set_source_files_properties(${GENERATED_DIR}/unidata.c PROPERTIES GENERATED 1)
set_source_files_properties(${GENERATED_DIR}/unidata.h PROPERTIES GENERATED 1)
set_source_files_properties(${GENERATED_DIR}/_unicorn.h PROPERTIES GENERATED 1)

include_directories(${GENERATED_DIR}) # For finding the generated files.
include_directories(include) # For finding the public header files.
include_directories(src) # For finding internal header files.

# Add the Unicorn library.
add_subdirectory(src)

# Add the examples, if requested.
if (UNICORN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

# Generate version information.
include(CMakePackageConfigHelpers)
write_basic_package_version_file(${CMAKE_BINARY_DIR}/UnicornConfigVersion.cmake COMPATIBILITY SameMajorVersion)

# Pre-process pkg-config and UnicornConfig.cmake files.
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
set(VERSION ${PROJECT_VERSION})
configure_file("${CMAKE_SOURCE_DIR}/UnicornConfig.cmake.in" "${CMAKE_BINARY_DIR}/UnicornConfig.cmake" @ONLY)
configure_file("${CMAKE_SOURCE_DIR}/unicorn.pc.in" "${CMAKE_BINARY_DIR}/unicorn.pc" @ONLY)

# Only install man pages when running "make install" locally.
add_subdirectory(man)

# This is the "make install" action.
install(FILES ${CMAKE_BINARY_DIR}/UnicornConfig.cmake DESTINATION cmake)
install(FILES ${CMAKE_BINARY_DIR}/UnicornConfigVersion.cmake DESTINATION cmake)
install(FILES ${CMAKE_BINARY_DIR}/unicorn.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# Add the unit tests if requested (commerical users only).
if (UNICORN_BUILD_TESTS)
    enable_testing()
    include(CTest)
    add_subdirectory(../tests tests)
endif ()

if (UNICORN_BUILD_COOKBOOK)
    add_subdirectory(../docs docs)
endif ()
