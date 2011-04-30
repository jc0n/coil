include $(top_srcdir)/Makefile.decl

SUBDIRS = tests

AM_CPPFLAGS = $(PYTHON_INCLUDES) \
							$(GLIB_CFLAGS) \
							$(PYGOBJECT_CFLAGS) \
							-I$(top_srcdir)/coil \
							-Wall \
							-Werror \
							-O0 -g \
							-fno-strict-aliasing

pkgpythondir = $(pyexecdir)/coil-@LIBCOIL_API_VERSION@
pkgpyexecdir = $(pyexecdir)/coil-@LIBCOIL_API_VERSION@

ccoildir = $(pkgpyexecdir)/coil

pkgpyexec_LTLIBRARIES =

if BUILD_PYTHON_BINDING
pkgpyexec_LTLIBRARIES += ccoil.la
endif

ccoil_HEADERS = \
								coilmodule.h \
								coilstruct.h

ccoil_la_SOURCES = \
										coillist.c \
			  						coilstruct.c \
	     	  					coilmodule.c

ccoil_la_LIBADD = \
		  						@GLIB_LIBS@ \
									@PYGOBJECT_LIBS@ \
		  						$(top_srcdir)/coil/libcoil-@LIBCOIL_API_VERSION@.la

ccoil_la_LDFLAGS = -module -avoid-version

all: $(pkgpyexec_LTLIBRARIES:.la=.so)

clean-local:
	rm -f $(pkgpyexec_LTLIBRARIES:.la=.so)

cCoil.so: ccoil.so
	$(LN_S) $@ cCoil.so

.la.so:
	$(LN_S) .libs/$@ $@ || true
