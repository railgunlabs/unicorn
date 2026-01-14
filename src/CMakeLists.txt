set(PUBLIC_HEADERS ../include/unicorn.h ${GENERATED_DIR}/_unicorn.h)

set(SOURCES
    ../include/unicorn.h
    unicorn.c
    logger.c
    memory.c
    common.h
    byteswap.h
    encoding.c
    segmentation.c
    normalize.c
    normalize.h
    collation.c
    ducet.c
    ducet.h
    compression.c
    properties.c
    caseconv.c
    casefold.c
    charbuf.c
    charbuf.h
    charvec.c
    charvec.h
    ${GENERATED_DIR}/unidata.c
    ${GENERATED_DIR}/unidata.h
    ${GENERATED_DIR}/_unicorn.h
)

if (UNICORN_BUILD_STATIC)
    add_library(unicorn_static STATIC ${SOURCES})
    target_compile_definitions(unicorn_static PUBLIC UNICORN_STATIC)
    set_property(TARGET unicorn_static PROPERTY C_STANDARD 99)
    set_target_properties(unicorn_static PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADERS}")
    add_dependencies(unicorn_static unicorn_generate)
    install(TARGETS unicorn_static ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif ()

if (UNICORN_BUILD_SHARED OR UNICORN_BUILD_EXAMPLES)
    add_library(unicorn SHARED ${SOURCES})
    target_compile_definitions(unicorn PRIVATE DLL_EXPORT)
    set_property(TARGET unicorn PROPERTY C_STANDARD 99)
    set_target_properties(unicorn PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADERS}")
    add_dependencies(unicorn unicorn_generate)

    if (CMAKE_C_COMPILER_ID MATCHES "Clang" OR
        CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(unicorn PRIVATE -pedantic)
    endif ()

    if (CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(unicorn PRIVATE -Wall -Wextra -Wconversion -Wno-missing-field-initializers
            -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion)
    endif ()

    install(TARGETS unicorn LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif ()

# Define a special build of Unicorn specifically for unit testing.
# This build is either the same as the static library build OR its
# a special dynamic library build that optionally adds code
# coverage instrumentation.
#
# The static build is neccessary on Windows to ensure the tests
# have access to internal/private symbols e.g. those not exported
# with DLL linkage. The coverage build is only ever run on *nix
# systems and shared objects on those platforms don't have those
# linker issues.
if (UNICORN_BUILD_TESTS OR UNICORN_BUILD_COOKBOOK)
     if (UNICORN_CODE_COVERAGE)
        add_library(unicorn_internal SHARED ${SOURCES})
        target_compile_options(unicorn_internal PRIVATE -g -fprofile-arcs -ftest-coverage -O0)
        target_link_options(unicorn_internal PRIVATE -fprofile-arcs -ftest-coverage -fbranch-probabilities -O0)
        target_compile_definitions(unicorn_internal PRIVATE DLL_EXPORT)
    else ()
        add_library(unicorn_internal STATIC ${SOURCES})
        target_compile_definitions(unicorn_internal PUBLIC UNICORN_STATIC)
    endif ()
    set_property(TARGET unicorn_internal PROPERTY C_STANDARD 99)
    add_dependencies(unicorn_internal unicorn_generate)
endif ()
