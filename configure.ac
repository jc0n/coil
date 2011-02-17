
AC_PREREQ(2.60)

AC_INIT([coil], [0.1], [tehjcon@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall])

COIL_MAJOR_VERSION=0
COIL_MINOR_VERSION=1
COIL_RELEASE_VERSION=0
COIL_VERSION="$COIL_MAJOR_VERSION.$COIL_MINOR_VERSION.$COIL_RELEASE_VERSION"

AC_SUBST(COIL_MAJOR_VERSION)
AC_SUBST(COIL_MINOR_VERSION)
AC_SUBST(COIL_RELEASE_VERSION)

AC_SUBST([LIBCOIL_API_VERSION],
         ["$COIL_MAJOR_VERSION.$COIL_MINOR_VERSION"])

AC_SUBST([LIBCOIL_SO_VERSION],
          ["$COIL_MAJOR_VERSION:$COIL_MINOR_VERSION:$COIL_RELEASE_VERSION"])

# TODO: Check for AWK properly

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_SED
AC_PROG_LIBTOOL
AC_PROG_INSTALL

AM_CONFIG_HEADER([config.h])

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_FILES([
    Makefile
    coil/Makefile
    tests/Makefile
    tests/functional/Makefile
    tests/benchmark/Makefile
])

AC_CONFIG_SRCDIR([coil/struct.c])

dnl
dnl Debugging
dnl
COIL_HELP_SEPARATOR(Debugging)

COIL_ARG_ENABLE(debug, whether to enable debugging,
                [Enable debugging functions and assertions], no)

if test "$COIL_DEBUG" = "yes"; then
  AC_DEFINE([COIL_DEBUG], [1], [ ])
fi

dnl
dnl Optimization
dnl
COIL_HELP_SEPARATOR(Optimization)

COIL_ARG_ENABLE(include-caching, whether to cache included files,
                [Enable caching of included files with @file or @include], yes)

if test "$COIL_INCLUDE_CACHING" = "yes"; then
  AC_DEFINE([COIL_INCLUDE_CACHING], [1], [ ])
fi

dnl
dnl Compatibility
dnl
COIL_HELP_SEPARATOR(Compatibility)

COIL_ARG_ENABLE(old-links, whether to use old-style links,
                [Enable old-style link resolution from link entry path
                 instead of container path], yes)

if test "$COIL_OLD_LINKS" = "yes"; then
  AC_DEFINE([COIL_OLD_LINKS], [1], [ ])
fi

dnl
dnl GLIB Dependency
dnl

PKG_CHECK_MODULES(GLIB, [gobject-2.0 >= 2.22],
                  [have_glib=yes], [have_glib=no])

if test "x$have_glib" = "xno"; then
    AC_MSG_ERROR($GLIB_PKG_ERRORS)
fi

AC_CHECK_FUNCS(memcpy mempcpy memset memchr memrchr)

AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)

AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_CFLAGS)

AC_OUTPUT
