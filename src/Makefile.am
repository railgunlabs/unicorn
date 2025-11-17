AUTOMAKE_OPTIONS = subdir-objects

config_json = @CONFIG_JSON@

include_HEADERS = _unicorn.h

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
	buffer.c \
	buffer.h \
	charbuf.c \
	charbuf.h \
	unidata.c \
	unidata.h \
	_unicorn.h

libunicorn_la_CFLAGS = -I$(top_srcdir)/include
libunicorn_la_LDFLAGS = -no-undefined -version-info 0:0:0

unidata.c:
	cd $(top_srcdir) && $(PYTHON) -m scripts --config $(config_json) --output $(abs_top_builddir)/src

unidata.h: unidata.c
 
_unicorn.h: unidata.c

noinst_HEADERS = unidata.h

BUILT_SOURCES = \
	unidata.c \
	unidata.h \
	_unicorn.h

CLEANFILES = \
	unidata.c \
	unidata.h \
	_unicorn.h
