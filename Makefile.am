SUBDIRS = man

EXTRA_DIST = generate.pyz unicorn.bin features.json autogen.sh LICENSE CMakeLists.txt UnicornConfig.cmake.in README.md

AUTOMAKE_OPTIONS = subdir-objects

nodist_include_HEADERS = unicorn.h

lib_LIBRARIES = libunicorn.a
nodist_libunicorn_a_SOURCES = $(abs_top_builddir)/unicorn.c $(abs_top_builddir)/unicorn.h

unicorn.c unicorn.h:
	cd $(abs_top_srcdir) && $(PYTHON) generate.pyz --output $(abs_top_builddir)

BUILT_SOURCES = unicorn.c unicorn.h
CLEANFILES = unicorn.c unicorn.h
