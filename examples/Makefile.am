AUTOMAKE_OPTIONS = subdir-objects

EXTRA_DIST = CMakeLists.txt

if BUILD_EXAMPLES

PROG_LDADD = ../src/libunicorn.la
PROG_CFLAGS = -I$(top_srcdir)/include -I$(abs_top_builddir)/src

bin_PROGRAMS = caseconv casefold charprops collate compare compress decode-utf8 decode-utf16 decode-utf32 encode-char segmentation

caseconv_SOURCES = case_convert.c
caseconv_LDADD = $(PROG_LDADD)
caseconv_CFLAGS = $(PROG_CFLAGS)

casefold_SOURCES = case_fold.c
casefold_LDADD = $(PROG_LDADD)
casefold_CFLAGS = $(PROG_CFLAGS)

charprops_SOURCES = character_properties.c
charprops_LDADD = $(PROG_LDADD)
charprops_CFLAGS = $(PROG_CFLAGS)

collate_SOURCES = collate_text.c
collate_LDADD = $(PROG_LDADD)
collate_CFLAGS = $(PROG_CFLAGS)

compare_SOURCES = compare_text.c
compare_LDADD = $(PROG_LDADD)
compare_CFLAGS = $(PROG_CFLAGS)

compress_SOURCES = compress_text.c
compress_LDADD = $(PROG_LDADD)
compress_CFLAGS = $(PROG_CFLAGS)

decode_utf8_SOURCES = decode_utf8.c
decode_utf8_LDADD = $(PROG_LDADD)
decode_utf8_CFLAGS = $(PROG_CFLAGS)

decode_utf16_SOURCES = decode_utf16.c
decode_utf16_LDADD = $(PROG_LDADD)
decode_utf16_CFLAGS = $(PROG_CFLAGS)

decode_utf32_SOURCES = decode_utf32.c
decode_utf32_LDADD = $(PROG_LDADD)
decode_utf32_CFLAGS = $(PROG_CFLAGS)

encode_char_SOURCES = encode_character.c
encode_char_LDADD = $(PROG_LDADD)
encode_char_CFLAGS = $(PROG_CFLAGS)

segmentation_SOURCES = segment_text.c
segmentation_LDADD = $(PROG_LDADD)
segmentation_CFLAGS = $(PROG_CFLAGS)

endif
