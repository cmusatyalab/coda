dnl Process this file with autoconf to produce a configure script

dnl Initialization
AC_PREREQ(2.50)
AC_INIT(rvm, 1.29)
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([rvm/rvm_private.h])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

dnl For library version updates, follow these steps in order.
dnl - when any library source has changed, increment first
dnl - when interfaces were added, removed, or changed, increment
dnl   second and set first to 0
dnl - if any interfaces were added, increment third
dnl - if any interfaces were removed, set third to 0
CODA_LIBRARY_VERSION(9, 3, 2)

CONFIG_DATE=`date +"%a, %d %b %Y %T %z"`
AC_SUBST(CONFIG_DATE, "$CONFIG_DATE", [Date when configure was last run])

dnl Build options.
AC_ARG_ENABLE([librvm],
  [  --enable-librvm	  build non-threaded rvm and rds libraries],
  [librvm="${enableval}"], [librvm="no"])
AC_ARG_ENABLE([librvmlwp],
  [  --enable-librvmlwp	  build LWP threaded rvm and rds libraries],
  [librvmlwp="${enableval}"], [librvmlwp="yes"])
AC_ARG_ENABLE([librvmpt],
  [  --enable-librvmpt	  build pthread threaded rvm and rds libraries],
  [librvmpt="${enableval}"], [librvmpt="no"])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_LIBTOOL

dnl pkg-config stuff
if test $librvmlwp != "no" ; then
CODA_PKG_CONFIG
PKG_CHECK_MODULES([LWP], [lwp])
fi
if test $librvmpt != "no" ; then
CODA_PTHREAD_FLAGS
fi

dnl Checks for libraries.
AC_SEARCH_LIBS(fdatasync, rt)

dnl Checks for header files.
dnl Checks for typedefs.

dnl Checks for compiler characteristics.
CODA_CC_FEATURE_TEST(Wall)

dnl Checks for library functions.
AC_CHECK_FUNCS(strerror fdatasync)

dnl AC_FUNC_MMAP checks if mmap exists and works, but that fails
dnl when we run configure on file systems that do not support mmap
AC_CHECK_FUNCS(mmap getpagesize)

dnl Checks for system services.

dnl Build conditionals
AM_CONDITIONAL([LIBRVM], [test "$librvm" != no])
AM_CONDITIONAL([LIBRVMLWP], [test "$librvmlwp" != no])
AM_CONDITIONAL([LIBRVMPT], [test "$librvmpt" != no])

dnl Generate output files
AC_CONFIG_FILES([Makefile include/Makefile rvm/Makefile seg/Makefile
    rds/Makefile tests/Makefile rvm.pc rvm-uninstalled.pc rvmlwp.pc
    rvmlwp-uninstalled.pc])
AC_OUTPUT
