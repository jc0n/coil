
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

AC_DEFINE_UNQUOTED([COIL_MAJOR_VERSION], $COIL_MAJOR_VERSION, [ ])
AC_DEFINE_UNQUOTED([COIL_MINOR_VERSION], $COIL_MINOR_VERSION, [ ])
AC_DEFINE_UNQUOTED([COIL_RELEASE_VERSION], $COIL_RELEASE_VERSION, [ ])

# TODO: Check for AWK properly

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_LEX

dnl Use faster scanner representation
if test "x$LEX" == "xflex"; then
    AC_MSG_CHECKING(if faster scanner representation is available)
    FLEX_VERSION="`${LEX} --version | cut -f2 -d' '`"
    AC_SUBST(FLEX_VERSION)
    case "${FLEX_VERSION}" in
        2*)
        AC_MSG_RESULT(yes)
        LFLAGS="${LFLAGS} -F"
        ;;
        *)
        AC_MSG_RESULT(no)
        ;;
    esac
fi

AC_PROG_YACC
AC_PROG_SED
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_C_BIGENDIAN
PKG_PROG_PKG_CONFIG(0.16)

AM_CONFIG_HEADER([config.h])

AC_CONFIG_MACRO_DIR([m4])

AM_SILENT_RULES([yes])

AC_CONFIG_FILES([
    libcoil-0.1.pc
    Makefile
    coil/Makefile
    python/Makefile
    python/tests/Makefile
    tests/Makefile
    tests/functional/Makefile
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
  CFLAGS="${CFLAGS} -O0 -g"
else
  AC_DEFINE([COIL_DEBUG], [0], [ ])
fi

dnl
dnl Optimization
dnl
COIL_HELP_SEPARATOR(Optimization)

COIL_ARG_ENABLE(include-caching, whether to cache included files,
                [Enable caching of included files with @file or @include], yes)

if test "$COIL_INCLUDE_CACHING" = "yes"; then
  AC_DEFINE([COIL_INCLUDE_CACHING], [1], [ ])
else
  AC_DEFINE([COIL_INCLUDE_CACHING], [0], [ ])
fi

dnl
dnl Compatibility
dnl
COIL_HELP_SEPARATOR(Compatibility)

COIL_ARG_ENABLE(strict-context, whether to enable strict context,
                [Enable strict context rules in struct expansions.], no)

if test "$COIL_STRICT_CONTEXT" = "yes"; then
  AC_DEFINE([COIL_STRICT_CONTEXT], [1], [ ])
else
  AC_DEFINE([COIL_STRICT_CONTEXT], [0], [ ])
fi

COIL_ARG_ENABLE(strict-file-context, whether to enable strict file context,
                [Enable strict file context rules. Included files are
                 expanded before being processed.], no)

if test "$COIL_STRICT_FILE_CONTEXT" = "yes"; then
  AC_DEFINE([COIL_STRICT_FILE_CONTEXT], [1], [ ])
else
  AC_DEFINE([COIL_STRICT_FILE_CONTEXT], [0], [ ])
fi

dnl
dnl GLIB Dependency
dnl

PKG_CHECK_MODULES(GLIB, [gobject-2.0 >= 2.22],
                  [have_glib=yes], [have_glib=no])

if test "x$have_glib" = "xno"; then
    AC_MSG_ERROR($GLIB_PKG_ERRORS)
fi

dnl
dnl Language Bindings
dnl

COIL_ARG_WITH(python, whether to build python support,
              [Build python binding],  yes)

if test "$COIL_PYTHON" = "yes"; then
  AM_CHECK_PYTHON_HEADERS(,[AC_MSG_ERROR(could not find Python headers)])
  AM_PATH_PYTHON([2.6])

  PKG_CHECK_MODULES(PYGOBJECT, [pygobject-2.0 >= 2.0],
                    [have_pygobject=yes], [have_pygobject=no])

  if test "x$have_pygobject" = "xno"; then
    AC_MSG_ERROR($PYGOBJECT_PKG_ERRORS)
  fi
  AM_CONDITIONAL(BUILD_PYTHON_BINDING, true)
  AC_SUBST(PYTHON)
else
  AM_CONDITIONAL(BUILD_PYTHON_BINDING, false)
fi

dnl
dnl GLIB Dependency
dnl

PKG_CHECK_MODULES(GLIB, [gobject-2.0 >= 2.22],
                  [have_glib=yes], [have_glib=no])

if test "x$have_glib" = "xno"; then
    AC_MSG_ERROR($GLIB_PKG_ERRORS)
fi

AC_CHECK_FUNCS(strcmp strcpy strlen)

AC_CHECK_FUNCS(memcpy mempcpy memset memchr memrchr memmem)
AC_CHECK_FUNCS(stat lstat)

AC_HEADER_TIME
AC_CHECK_MEMBERS([struct stat.st_mtime])

AC_SUBST(LFLAGS)

AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)

AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_CFLAGS)

AC_OUTPUT
