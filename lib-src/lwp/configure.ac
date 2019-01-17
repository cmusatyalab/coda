dnl Process this file with autoconf to produce a configure script

dnl Initialization
AC_PREREQ(2.50)
AC_INIT(lwp, 2.12)
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/lwp.c])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

dnl For library version updates, follow these steps in order.
dnl - when any library source has changed, increment first
dnl - when interfaces were added, removed, or changed, increment
dnl   second and set first to 0
dnl - if any interfaces were added, increment third
dnl - if any interfaces were removed, set third to 0
CODA_LIBRARY_VERSION(1, 3, 1)

CONFIG_DATE=`date +"%a, %d %b %Y %T %z"`
AC_SUBST(CONFIG_DATE, "$CONFIG_DATE", [Date when configure was last run])

dnl Build options.
AC_ARG_ENABLE(pthread,
  [  --enable-pthread        build experimental pthreaded LWP library],
  includepthread="$enableval", includepthread="no")

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AM_PROG_AS
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

dnl pkg-config stuff
CODA_PKG_CONFIG

dnl Checks for libraries.
AC_SUBST(LIBPTHREAD)
AC_CHECK_LIB(pthread, pthread_create, [LIBPTHREAD="-lpthread"])
AC_CHECK_LIB(posix4, sem_init, [LIBPTHREAD="${LIBPTHREAD} -lposix4"])

dnl Checks for typedefs.
AC_CHECK_TYPES(register_t)
AC_CHECK_TYPES(stack_t,,, [#include <signal.h>])

dnl Checks for compiler characteristics
CODA_CC_FEATURE_TEST(Wall)

dnl Checks for library functions.
AC_CHECK_FUNCS(sigaltstack)

dnl AC_FUNC_MMAP checks if mmap exists and works, but that fails
dnl when we run configure on file systems that do not support mmap
AC_CHECK_FUNCS(mmap)


dnl Build conditionals.
AM_CONDITIONAL(build_pthread, test "$includepthread" != "no" -a "x$LIBPTHREAD" != x)

dnl Generate output files
AC_CONFIG_FILES([Makefile include/Makefile src/Makefile src-pt/Makefile
    lwp.pc lwp-uninstalled.pc])
AC_OUTPUT

