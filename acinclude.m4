
dnl
dnl COIL_HELP_SEPARATOR(title)
dnl
AC_DEFUN([COIL_HELP_SEPARATOR],[
AC_ARG_ENABLE([],[
$1
], [])
])

dnl
dnl COIL_ARG_ENABLE(arg-name, check message, help[, default-val])
dnl
AC_DEFUN([COIL_ARG_ENABLE], [
coil_enable_[]translit($1,A-Z0-9-,a-z0-9_)=ifelse($4,,no,$4)
COIL_REAL_ARG_ENABLE([$1], [$2], [$3], [$4],
                     COIL_[]translit($1,a-z0-9-,A-Z0-9_))
])

dnl
dnl COIL_REAL_ARG_ENABLE
dnl internal
AC_DEFUN([COIL_REAL_ARG_ENABLE], [
ifelse([$2],,,[AC_MSG_CHECKING([$2])])
AC_ARG_ENABLE($1,
  AS_HELP_STRING([--enable-$1], [$3 [default=$4]]),
   [
     $5=[$]enableval
     AC_MSG_RESULT([$enableval])
   ],
   [
     $5=ifelse($4,,no,$4)
     AC_MSG_RESULT([$]$5)
   ])
])

