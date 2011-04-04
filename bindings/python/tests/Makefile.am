
EXTRA_DIST = $(tests) common.py runtests.py

tests = test_struct.py

test: check
# sed -e 's/\(ok\)\$/\[\033[0;32m\]\1\[\033[0;47m\]/g'
RUN_TESTS = $(PYTHON) $(srcdir)/runtests.py $(top_buildir) $(top_srcdir)

check-local:
	@test $(SHELL) = "/bin/bash" \
		&& $(RUN_TESTS) 2>&1 | \
		$(SED) -e 's/ok$$/\o033[0;32m\0\o033[0m/g' \
				   -e 's/\(FAIL\(ED\)\?\)/\o033[0;31m\0\o033[0m/g'; \
		exit $${PIPESTATUS[0]:-1}
	@test $(SHELL) = "/bin/bash" || $(RUN_TESTS)
	@rm -fr *.pyc
