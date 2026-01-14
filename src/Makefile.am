AUTOMAKE_OPTIONS = subdir-objects

config_json = @CONFIG_JSON@

gen = $(top_builddir)/src-gen

nodist_include_HEADERS = $(gen)/_unicorn.h

lib_LTLIBRARIES = libunicorn.la

libunicorn_la_SOURCES = \
	$(top_srcdir)/include/unicorn.h \
	unicorn.c \
	logger.c \
	memory.c \
	common.h \
	byteswap.h \
	encoding.c \
	segmentation.c \
	normalize.c \
	normalize.h \
	collation.c \
	ducet.c \
	ducet.h \
	compression.c \
	properties.c \
	caseconv.c \
	casefold.c \
	charbuf.c \
	charbuf.h \
	charvec.c \
	charvec.h

nodist_libunicorn_la_SOURCES = \
	$(gen)/unidata.c \
	$(gen)/unidata.h \
	$(gen)/_unicorn.h

libunicorn_la_CFLAGS = -I$(top_srcdir)/include -I$(gen)
libunicorn_la_LDFLAGS = -no-undefined -version-info 0:0:0

$(gen):
	mkdir -p $@

$(gen)/unidata.c:
	cd $(top_srcdir) && $(PYTHON) -m scripts --config $(config_json) --output $(abs_top_builddir)/src-gen

$(gen)/unidata.h: $(gen)/unidata.c
 
$(gen)/_unicorn.h: $(gen)/unidata.c

clean-local:
	rm -rf $(gen)

noinst_HEADERS = $(gen)/unidata.h

BUILT_SOURCES = \
	$(gen)/unidata.c \
	$(gen)/unidata.h \
	$(gen)/_unicorn.h

CLEANFILES = \
	$(gen)/unidata.c \
	$(gen)/unidata.h \
	$(gen)/_unicorn.h
