include $(top_srcdir)/Makefile.decl

EXTRA_DIST += cases

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)/coil

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += run_functional_tests
run_functional_tests_SOURCES = run_functional_tests.c
run_functional_tests_LDADD = $(test_libs)
