# Specify the project and version + the minimum automake version required.
AC_PREREQ(2.66)
AC_INIT([unicorn], [1.3.1])

# Sanity check to verify this is indeed the source directory.
AC_CONFIG_SRCDIR(src/unicorn.c)

# Initialize automake and use "foreign" so it doesn't attmpet to look for or
# create a GNU project structure: COPYING, NEWS, etc...
AM_INIT_AUTOMAKE([foreign])

# Check for the --with-unicorn-config option.
AC_ARG_WITH([unicorn-config],
    [AS_HELP_STRING([--with-unicorn-config=PATH],
                    [Path to a JSON file that configures the Unicorn feature set. (default: features.json)])],
    [CONFIG_JSON="$withval"],
    [CONFIG_JSON="features.json"])
AC_SUBST([CONFIG_JSON])

# Check for the --enable-examples option.
AC_ARG_ENABLE([examples],
    [AS_HELP_STRING([--enable-examples], [Build the example programs (disabled by default)])],
    [enable_examples=$enableval],
    [enable_examples=no]) # Do not build the examples by default.
AM_CONDITIONAL([BUILD_EXAMPLES], [test "x$enable_examples" = "xyes"])

# Checks for toolchain programs.
AC_PROG_CC
AM_PROG_AR
AC_PROG_CPP
AC_PROG_INSTALL
LT_INIT([win32-dll])

# Verify the expected C headers are available.AM_PATH_PYTHON([3.10])
AC_SUBST([PYTHON],[$PYTHON])
AC_CHECK_HEADERS([string.h assert.h stdbool.h], [], [AC_MSG_ERROR([could not find required header])])

# Check for Python (required to build the project).
AM_PATH_PYTHON([3.10])
AC_SUBST([PYTHON],[$PYTHON])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    include/Makefile
    examples/Makefile
    config/Makefile
    man/Makefile
    unicorn.pc
])
AC_OUTPUT
