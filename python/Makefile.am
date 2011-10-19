include $(top_srcdir)/Makefile.decl

SUBDIRS = tests

MODULES =

if BUILD_PYTHON_BINDING
MODULES += coilmodule
endif

all: $(MODULES)

COIL_LIBS = $(top_srcdir)/coil/libcoil-@LIBCOIL_API_VERSION@.la

coilmodule: buildconf.py $(COIL_LIBS)
	$(PYTHON) $(srcdir)/setup.py build

install-exec-hook:
	$(PYTHON) $(srcdir)/setup.py install

buildconf.py: $(top_srcdir)/config.h
	@$(SED) -e '/^#/!d' \
	 	   -e 's/#define //' \
	 	   -e 's/[^=] /\0= /' $^ > $(srcdir)/$@

if BUILD_PYTHON_BINDING
clean-local:
	test -z "${PYTHON}" && !-d $(srcdir)/build || \
		$(PYTHON) $(srcdir)/setup.py clean
	find . -type f -name "*.py[co]" -exec rm -f '{}' ';'
endif
