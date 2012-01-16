
test: check

if BUILD_PYTHON_BINDING
EXTRA_DIST = $(tests) \
			 common.py \
			 runtests.py \
			 simple.coil \
			 example.coil \
			 example2.coil \
			 example3.coil

tests = test_struct.py test_parser.py test_listproxy.py

# stupid hack to make tests work without library installed
RUN_TESTS = LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:$(top_builddir)/coil/.libs" \
			$(PYTHON) $(srcdir)/runtests.py $(top_buildir) $(top_srcdir)

check-local:
	@test $(SHELL) = "/bin/bash" \
		&& $(RUN_TESTS) 2>&1 | \
		$(SED) -e 's/[oO][kK]$$/\r\t\t\t\t\t\t\t\t\t\[\o033[0;32m\U\0\E\o033[0m\]/g' \
				   -e 's/\(FAIL\(ED\)\?\|ERROR\)$$/\r\t\t\t\t\t\t\t\t\t\[\o033[0;31m\U\0\E\o033[0m\]/g' \
				   -e 's/\.\(FAIL\(ED\)\?\)/\r\t\t\t\t\t\t\t\t\t\[\0\]/g'; \
		exit $${PIPESTATUS[0]:-1}
	@test $(SHELL) = "/bin/bash" || $(RUN_TESTS)
	@find $(srcdir) -type f -name "*.py[co]" -exec rm -f '{}' ';'
endif
