include $(top_srcdir)/Makefile.decl

ACLOCAL_AMFLAGS=-I m4

SUBDIRS = coil tests python

pkgconfigdir= $(libdir)/pkgconfig
pkgconfig_DATA=libcoil-0.1.pc

EXTRA_DIST += \
		m4 \
		autogen.sh \
		README-dev \
		INSTALL

check: test

.PHONY: check
