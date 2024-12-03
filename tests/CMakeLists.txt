# The purpose of this unit test is to verify Unicorn can be linked.
# As-in all functions are available and accessible.
# The _actual_ unit tests that verify the functionality is correct
# are closed source with access being granted to commercial licenses.

add_executable(test_unicorn tests.c ${SOURCES}) # Sources compiled into executable.
add_dependencies(test_unicorn unicorn_generate)

add_executable(test_unicorn_linked tests.c) # Unicorn linked as a static library.
target_link_libraries(test_unicorn_linked unicorn)

add_test(CompiledAndLinked test_unicorn)
add_test(StaticLibrary test_unicorn_linked)
