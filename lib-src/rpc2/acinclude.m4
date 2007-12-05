dnl ---------------------------------------------
dnl Define library version
dnl
AC_SUBST(LIBTOOL_VERSION)
AC_SUBST(MAJOR_VERSION)
AC_SUBST(LINUX_VERSION)
AC_SUBST(DLL_VERSION)
AC_SUBST(FREEBSD_VERSION)
AC_SUBST(GENERIC_VERSION)
AC_DEFUN([CODA_LIBRARY_VERSION],
  [LIBTOOL_VERSION="$2:$1:$3"; major=`expr $2 - $3`
   MAJOR_VERSION="$major"
   LINUX_VERSION="$major.$3.$1"
   DLL_VERSION="$major-$3-$1"
   FREEBSD_VERSION="$2"
   GENERIC_VERSION="$2.$1"])

dnl ---------------------------------------------
dnl Check if the compiler supports specific flags
dnl
AC_DEFUN([CODA_CC_FEATURE_TEST],
  [AC_CACHE_CHECK(whether the C compiler accepts -$1, coda_cv_cc_$1,
      coda_saved_CFLAGS="$CFLAGS" ; CFLAGS="$CFLAGS -$1" ; AC_LANG_SAVE
      AC_LANG_C
      AC_TRY_COMPILE([], [], coda_cv_cc_$1=yes, coda_cv_cc_$1=no)
      AC_LANG_RESTORE
      CFLAGS="$coda_saved_CFLAGS")
  if test $coda_cv_cc_$1 = yes ; then
      if echo "x $CFLAGS" | grep -qv "$1" ; then
	  CFLAGS="$CFLAGS -$1"
      fi
  fi])

dnl ---------------------------------------------
dnl Find the native C compiler in order to generate a working rp2gen
dnl
AC_SUBST(NATIVECC)
AC_DEFUN([CODA_PROG_NATIVECC],
    if test $cross_compiling = yes ; then
       [AC_MSG_NOTICE([checking for native C compiler on the build host])
	AC_CHECK_PROG(NATIVECC, gcc, gcc)
	if test -z "$NATIVECC" ; then
	    AC_CHECK_PROG(NATIVECC, cc, cc, , , /usr/ucb/cc)
	fi
	test -z "$NATIVECC" && AC_MSG_ERROR([no acceptable cc found in \$PATH])
	AC_MSG_RESULT([	found.])
	dnl Just assume it works.]
    else
	NATIVECC=${CC}
    fi)

dnl ---------------------------------------------
dnl Check which lib provides termcap functionality
dnl
AC_SUBST(LIBTERMCAP)
AC_DEFUN([CODA_CHECK_LIBTERMCAP],
 [saved_LIBS=${LIBS} ; LIBS=
  AC_SEARCH_LIBS(tgetent, [ncurses termcap], [LIBTERMCAP=${LIBS}])
  LIBS=${saved_LIBS}])

dnl ---------------------------------------------
dnl find readline functionality
dnl also test for new functions introduced by readline 4.2
dnl
AC_SUBST(LIBREADLINE)
AC_DEFUN([CODA_CHECK_READLINE],
  [AC_CHECK_LIB(readline, main, [LIBREADLINE=-lreadline], [], [${LIBTERMCAP}])
   AM_CONDITIONAL(HAVE_READLINE, test x${LIBREADLINE} != x)
   AC_CHECK_LIB(readline, rl_completion_matches,
     [AC_DEFINE(HAVE_RL_COMPLETION_MATCHES, 1, [Define if you have readline 4.2 or later])], [], [${LIBTERMCAP}])])

