include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS=$(GLIB_CFLAGS) $(GOBJECT_CFLAGS) -Wall -Werror

BUILT_SOURCES = \
				parser.h \
				parser.c \
				scanner.c \
				scanner.h

lib_LTLIBRARIES = libcoil-@LIBCOIL_API_VERSION@.la

libcoil_@LIBCOIL_API_VERSION@_la_SOURCES = \
				coil.c \
				error.c \
				expression.c \
				include.c \
				link.c \
				list.c \
				object.c \
				parser.y \
				path.c \
				scanner.l \
				strings_extra.c \
				struct.c \
				struct_table.c \
				value.c

libcoil_@LIBCOIL_API_VERSION@_la_LIBADD = @LEXLIB@ @GLIB_LIBS@
libcoil_@LIBCOIL_API_VERSION@_la_LDFLAG = -version-info $(LIBCOIL_SO_VERSION)

# ignore unused yy functions
libcoil_@LIBCOIL_API_VERSION@_la_CPPFLAGS=$(AM_CPPFLAGS) -Wno-unused-function

libcoil_includedir=$(includedir)/coil-$(LIBCOIL_API_VERSION)
libcoil_include_HEADERS=\
				coil.h \
				common.h \
				error.h \
				expression.h \
				format.h \
				include.h \
				link.h \
				list.h \
				object.h \
				parser.h \
				parser_defs.h \
				path.h \
				scanner.h \
				strings_extra.h \
				struct.h \
				struct_table.h \
				value.h

bin_PROGRAMS = coildump

coildump_LDADD = @GLIB_LIBS@ libcoil-@LIBCOIL_API_VERSION@.la

$(libcoil_la_OBJECTS): $(srcdir)/parser.h

scanner.h: scanner.l
	$(LEX) $(LFLAGS) --header-file=$@ -o/dev/null $<

scanner.c: scanner.l parser.h
	$(LEX) $(LFLAGS) -o $@ -i $<

parser.h: parser.c

clean-local:
	rm -f $(BUILT_SOURCES)
