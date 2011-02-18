include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS=$(GLIB_CFLAGS) $(GOBJECT_CFLAGS) -Wall -Werror

BUILT_SOURCES = \
				marshal.c	\
				marshal.h \
				parser.h \
				parser.c \
				scanner.c \
				scanner.h

EXTRA_DIST += \
				marshal.h	\
				marshal.list

lib_LTLIBRARIES = libcoil-@LIBCOIL_API_VERSION@.la

libcoil_@LIBCOIL_API_VERSION@_la_SOURCES = \
				coil.c \
				error.c \
				expandable.c \
				expression.c \
				include.c \
				link.c \
				list.c \
				marshal.c \
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

libcoil_includedir=$(includedir)/libcoil-$(LIBCOIL_API_VERSION)
libcoil_include_HEADERS=\
				coil.h \
				error.h \
				expandable.h \
				expression.h \
				include.h \
				link.h \
				list.h \
				marshal.h \
				parser.h \
				parser_defs.h \
				path.h \
				scanner.h \
				strings_extra.h \
				struct.h \
				struct_private.h \
				value.h

bin_PROGRAMS = coildebug
coildebug_SOURCES = coildebug.c
coildebug_LDADD = @GLIB_LIBS@ libcoil-@LIBCOIL_API_VERSION@.la

$(libcoil_la_OBJECTS): $(srcdir)/parser.h

# FIXME: actually check for glib-genmarshal and set this in configure.ac
glib_genmarshal=`which glib-genmarshal`
marshal_prefix=coil_cclosure_marshal

marshal.h: marshal.list
	$(glib_genmarshal) --prefix=$(marshal_prefix) $< --header > $@

marshal.c: marshal.list
	$(glib_genmarshal) --prefix=$(marshal_prefix) $< --body > $@

scanner.h: scanner.l
	$(LEX) $(LFLAGS) --header-file=$@ -o/dev/null $<

scanner.c: scanner.l parser.h
	$(LEX) $(LFLAGS) -o $@ -i $<

parser.h: parser.c

clean-local:
	rm -f $(BUILT_SOURCES)
