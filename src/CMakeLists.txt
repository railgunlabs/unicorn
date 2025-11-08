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
    buffer.c
    buffer.h
    charbuf.c
    charbuf.h
    ${CMAKE_BINARY_DIR}/unidata.c
    ${CMAKE_BINARY_DIR}/unidata.h
)

if (UNICORN_BUILD_STATIC)
    add_library(unicorn_static STATIC ${SOURCES})
    set_property(TARGET unicorn_static PROPERTY C_STANDARD 99)
    set_target_properties(unicorn_static PROPERTIES PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/unicorn.h)
    set_target_properties(unicorn_static PROPERTIES PUBLIC_HEADER ${CMAKE_BINARY_DIR}/_unicorn.h)
    add_dependencies(unicorn_static unicorn_generate)
    install(TARGETS unicorn_static ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif ()

if (UNICORN_BUILD_SHARED)
    add_library(unicorn SHARED ${SOURCES})
    set_property(TARGET unicorn PROPERTY C_STANDARD 99)
    set_target_properties(unicorn PROPERTIES PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/unicorn.h)
    set_target_properties(unicorn PROPERTIES PUBLIC_HEADER ${CMAKE_BINARY_DIR}/_unicorn.h)
    add_dependencies(unicorn unicorn_generate)

    if (UNICORN_CODE_COVERAGE)
        target_compile_options(unicorn PRIVATE -g -fprofile-arcs -ftest-coverage -O0)
        target_link_options(unicorn PRIVATE -fprofile-arcs -ftest-coverage -fbranch-probabilities -O0)
    endif ()

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
