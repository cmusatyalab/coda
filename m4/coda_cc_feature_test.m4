#serial 2
dnl ---------------------------------------------
dnl Check if the compiler supports specific flags
dnl takes 2 arguments, the option and the m4 macro name (can't contain '-')
dnl
AC_DEFUN([CODA_CC_FEATURE_TEST],
  [AC_CACHE_CHECK(whether the C compiler accepts -$1, coda_cv_cc_$2,
      coda_saved_CFLAGS="$CFLAGS" ; CFLAGS="$CFLAGS -$1"
      AC_LANG_PUSH([C])
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([[]])],
		        coda_cv_cc_$2=yes, coda_cv_cc_$2=no)
      AC_LANG_POP([C])
      CFLAGS="$coda_saved_CFLAGS")
  if test $coda_cv_cc_$2 = yes ; then
      if echo "x $CFLAGS" | grep -qv "$1" ; then
	  CFLAGS="$CFLAGS -$1"
      fi
  fi])
