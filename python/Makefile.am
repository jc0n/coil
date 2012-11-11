include $(top_srcdir)/Makefile.decl

SUBDIRS = tests

MODULES =
if BUILD_PYTHON_BINDING
MODULES += coilmodule
endif

COIL_LIBS = $(top_srcdir)/coil/libcoil-@LIBCOIL_API_VERSION@.la

all: $(MODULES)

# XXX: this is a stupid hack
# should figure out how to do this with distutils
#
coilmodule: buildconf.py $(COIL_LIBS)
	$(PYTHON) $(srcdir)/setup.py build
	find $(srcdir)/build -type f -name "ccoil.so" \
		-exec cp '{}' $(srcdir)/ccoil.so ';'

install-exec-hook:
	$(PYTHON) $(srcdir)/setup.py install

buildconf.py: $(top_srcdir)/config.h
	@$(SED) -e '/^#/!d' \
	 	   -e 's/#define //' \
	 	   -e 's/[^=] /\0= /' $^ > $(srcdir)/$@

clean-local:
	test -z "${PYTHON}" && !-d $(srcdir)/build || \
		$(PYTHON) $(srcdir)/setup.py clean
	test ! -d $(srcdir)/build || rm -rf $(srcdir)/build
	rm -f ccoil.so
	find . -type f -name "*.py[co]" -exec rm -f '{}' ';'
