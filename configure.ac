AC_INIT([unicorn], [1.0.1])
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC
AC_PROG_CPP
AC_PROG_RANLIB
AC_CHECK_HEADERS([string.h assert.h stdbool.h], [], [AC_MSG_ERROR([could not find required header])])
AM_PATH_PYTHON([3.6])
AC_SUBST([PYTHON],[$PYTHON])
AC_CONFIG_FILES([
    Makefile
    man/Makefile
    unicorn.pc
])
AC_OUTPUT
