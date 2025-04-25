#serial 1
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
