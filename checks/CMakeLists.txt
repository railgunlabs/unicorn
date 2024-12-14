# The purpose of these executables is to verify Unicorn can be linked.
# The _actual_ unit tests that verify the functionality are closed
# source with access granted only to commercial licensees.

add_executable(test_unicorn check_linking.c ${SOURCES}) # Sources compiled into executable.
add_dependencies(test_unicorn unicorn_generate)

add_executable(test_unicorn_linked check_linking.c) # Unicorn linked as a static library.
target_link_libraries(test_unicorn_linked unicorn)

add_test(CompiledAndLinked test_unicorn)
add_test(StaticLibrary test_unicorn_linked)
