#serial 2
dnl ---------------------------------------------
dnl Check if the compiler supports specific flags
dnl takes 2 arguments, the option and the m4 macro name (can't contain '-')
dnl
AC_DEFUN([CODA_CXX_FEATURE_TEST],
  [AC_CACHE_CHECK(whether the C++ compiler accepts -$1, coda_cv_cxx_$2,
      coda_saved_CXXFLAGS="$CXXFLAGS" ; CXXFLAGS="$CXXFLAGS -$1"
      AC_LANG_PUSH([C++])
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([[]])],
		        coda_cv_cxx_$2=yes, coda_cv_cxx_$2=no)
      AC_LANG_POP([C++])
      CXXFLAGS="$coda_saved_CXXFLAGS")
  if test $coda_cv_cxx_$2 = yes ; then
      if echo "x $CXXFLAGS" | grep -qv "$1" ; then
	  CXXFLAGS="$CXXFLAGS -$1"
      fi
  fi])
