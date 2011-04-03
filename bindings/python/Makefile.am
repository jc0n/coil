include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = $(PYTHON_INCLUDES) \
							$(GLIB_CFLAGS) \
							-I$(top_srcdir)/coil \
							-Wall \
							-Werror \
							-O0 -g \
							-fno-strict-aliasing

pkgpythondir = $(pyexecdir)/coil-@LIBCOIL_API_VERSION@
pkgpyexecdir = $(pyexecdir)/coil-@LIBCOIL_API_VERSION@

coilmoduledir = $(pkgpyexecdir)/coil

coilmodule_LTLIBRARIES =

if BUILD_PYTHON_BINDING
coilmodule_LTLIBRARIES += coilmodule.la
endif

coilmodule_HEADERS = \
										coilmodule.h \
										coilstruct.h

coilmodule_la_SOURCES = \
												coilmodule.c \
												coilstruct.c

coilmodule_la_LIBADD = \
											 @GLIB_LIBS@ \
											$(top_srcdir)/coil/libcoil-@LIBCOIL_API_VERSION@.la

coilmodule_la_LDFLAGS = -module -avoid-version
