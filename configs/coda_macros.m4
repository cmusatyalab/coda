AC_DEFUN(CODA_CC_FEATURE_TEST,
  [AC_CACHE_CHECK(whether the C compiler accepts $2, coda_cv_cc_$1,
     coda_saved_CC="$CC" ; CC="$CC $2" ; AC_LANG_SAVE
     AC_LANG_C
     AC_TRY_COMPILE([], [], coda_cv_cc_$1=yes, coda_cv_cc_$1=no)
     AC_LANG_RESTORE
     CC="$coda_saved_CC")
   if test $coda_cv_cc_$1 = yes; then
     CC="$CC $2"
   fi])

AC_DEFUN(CODA_CXX_FEATURE_TEST,
  [AC_CACHE_CHECK(whether the C++ compiler accepts $2, coda_cv_cxx_$1,
     coda_saved_CXX="$CXX" ; CXX="$CXX $2" ; AC_LANG_SAVE
     AC_LANG_CPLUSPLUS
     AC_TRY_COMPILE([], [], coda_cv_cxx_$1=yes, coda_cv_cxx_$1=no)
     AC_LANG_RESTORE
     CXX="$coda_saved_CXX")
   if test $coda_cv_cxx_$1 = yes; then
     CXX="$CXX $2"
   fi])

AC_SUBST(NATIVECC)
AC_DEFUN(CODA_PROG_NATIVECC,
    if test $cross_compiling = yes ; then
       [AC_CHECKING([for native C compiler on the build host])
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

dnl Check for typedefs in unusual headers
dnl Most of this function is identical to AC_CHECK_TYPE in autoconf-2.13
AC_DEFUN(CODA_CHECK_TYPE,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_type_$1,
[AC_EGREP_CPP(dnl
changequote(<<,>>)dnl
<<(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]>>dnl
changequote([,]), [#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#include <$3>], ac_cv_type_$1=yes, ac_cv_type_$1=no)])dnl
AC_MSG_RESULT($ac_cv_type_$1)
if test $ac_cv_type_$1 = no; then
  AC_DEFINE($1, $2, [Definition for $1 if the type is missing])
fi
])

dnl Check which lib provides termcap functionality
AC_SUBST(LIBTERMCAP)
AC_DEFUN(CODA_CHECK_LIBTERMCAP,
  AC_CHECK_LIB(termcap, main, [LIBTERMCAP="-ltermcap"],
    [AC_CHECK_LIB(ncurses, tgetent, [LIBTERMCAP="-lncurses"])]))

dnl Check for a curses library, and if it needs termcap
AC_SUBST(LIBCURSES)
AC_DEFUN(CODA_CHECK_LIBCURSES,
  if test $target != "i386-pc-cygwin32" -a $target != "i386-pc-djgpp" ; then
    AC_CHECK_LIB(ncurses, main, [LIBCURSES="-lncurses"],
	[AC_CHECK_LIB(curses, main, [LIBCURSES="-lcurses"],
	    [AC_MSG_ERROR("failed to find curses library")
	    ], $LIBTERMCAP)
	], $LIBTERMCAP)
    AC_CACHE_CHECK("if curses library requires -ltermcap",
	coda_cv_curses_needs_termcap,
	[coda_save_LIBS="$LIBS"
	LIBS="$LIBCURSES $LIBS"
	AC_TRY_LINK([],[],
	coda_cv_curses_needs_termcap=no,
	coda_cv_curses_needs_termcap=yes)
	LIBS="$coda_save_LIBS"])
    if test $coda_cv_curses_needs_termcap = yes; then
	LIBCURSES="$LIBCURSES $LIBTERMCAP"
    fi
  fi)


dnl check wether we have flock or fcntl
AC_DEFUN(CODA_CHECK_FILE_LOCKING,
  AC_CACHE_CHECK(for file locking by fcntl,
    fu_cv_lib_c_fcntl,
    [AC_TRY_COMPILE([#include <fcntl.h>
#include <stdio.h>], [ int fd; struct flock lk; fcntl(fd, F_SETLK, &lk);],
      fu_cv_lib_c_fcntl=yes,
      fu_cv_lib_c_fcntl=no)])
  if test $fu_cv_lib_c_fcntl = yes; then
    AC_DEFINE(HAVE_FCNTL_LOCKING, 1, [Define if you have fcntl file locking])
  fi

  AC_CACHE_CHECK(for file locking by flock,
    fu_cv_lib_c_flock,
    [AC_TRY_COMPILE([#include <sys/file.h>
#include <stdio.h>], [ int fd; flock(fd, LOCK_SH);],
      fu_cv_lib_c_flock=yes,
      fu_cv_lib_c_flock=no)])
  if test $fu_cv_lib_c_flock = yes; then
    AC_DEFINE(HAVE_FLOCK_LOCKING, 1, [Define if you have flock file locking])
  fi

  if test $fu_cv_lib_c_flock = no -a $fu_cv_lib_c_fcntl = no; then
         AC_MSG_ERROR("failed to find flock or fcntl")
  fi)

dnl check wether bcopy is defined in strings.h
AC_DEFUN(CODA_CHECK_BCOPY, 
  AC_CACHE_CHECK(for bcopy in strings.h,
    fu_cv_lib_c_bcopy,  
    [AC_TRY_COMPILE([#include <stdlib.h>
#include <strings.h>], 
      [ char *str; str = (char *) malloc(5); (void) bcopy("test", str, 5);],
      fu_cv_lib_c_bcopy=yes,
      fu_cv_lib_c_bcopy=no)])
  if test $fu_cv_lib_c_bcopy = yes; then
    AC_DEFINE(HAVE_BCOPY_IN_STRINGS_H, 1,
      [Define if bcopy et al are defined in the <strings.h> header file])
  fi)

dnl check for library providing md5 checksumming
AC_SUBST(LIBMD5)
AC_SUBST(MD5C)
AC_DEFUN(CODA_CHECK_MD5,
  [if test "${CRYPTO}" = "yes" ; then
    AC_CHECK_HEADERS(openssl/md5.h)
    AC_SEARCH_LIBS(MD5_Init, crypto,
      [test "$ac_cv_search_MD5_Init" = "none required" || LIBMD5="$ac_cv_search_MD5_Init"
       LIBS="$ac_func_search_save_LIBS"])
  else
    ac_cv_search_MD5_Init="no"
  fi
  if test "$ac_cv_search_MD5_Init" = "no"; then
      MD5C="md5c.o"
  fi])

AC_SUBST(LIBKRB4)
AC_SUBST(LIBKRB5)
AC_DEFUN(CODA_CHECK_KERBEROS,
  [if test "${CRYPTO}" = "yes" ; then
    AC_CHECK_HEADERS(krb5.h com_err.h)
    if test "$ac_cv_header_krb5_h" = yes -a "$ac_cv_header_com_err_h" = yes ; then
	AC_SEARCH_LIBS(krb5_init_context, krb5,
	    [LIBKRB5="$ac_cv_search_krb5_init_context"
	     LIBS="$ac_func_search_save_LIBS"], , -lk5crypto -lcom_err)
	AC_SEARCH_LIBS(krb5_encrypt, k5crypto,
	    [LIBKRB5="${LIBKRB5} $ac_cv_search_krb5_encrypt"
	     LIBS="$ac_func_search_save_LIBS"])
	AC_SEARCH_LIBS(com_err, com_err,
	    [LIBKRB5="${LIBKRB5} $ac_cv_search_com_err"
	     LIBS="$ac_func_search_save_LIBS"])
	if test -n "${LIBKRB5}" ; then
	    AC_DEFINE(HAVE_KRB5, 1, [Define if kerberos 5 is available])
	fi
    else
	AC_MSG_WARN([Couldn't find krb5.h and com_err.h headers, not using kerberos 5])
    fi

    AC_CHECK_HEADERS(krb.h des.h)
    if test "$ac_cv_header_krb_h" = yes -a "$ac_cv_header_des_h" = yes ; then
	AC_SEARCH_LIBS(krb_get_lrealm, krb4,
	    [LIBKRB4="$ac_cv_search_krb_get_lrealm"
	     LIBS="$ac_func_search_save_LIBS"])
	if test -n "${LIBKRB4}" ; then
	    AC_DEFINE(HAVE_KRB4, 1, [Define if kerberos 4 is available])
	fi
    else
	AC_MSG_WARN([Couldn't find krb.h and des.h headers, not using kerberos 4])
    fi
   fi])

dnl check whether we have scandir
AC_DEFUN(CODA_CHECK_DIRENT, 
  AC_CACHE_CHECK(for d_namlen in dirent, fu_cv_lib_c_dirent_d_namlen,
    [AC_TRY_COMPILE([#include <dirent.h>],
       [ struct dirent d; int n; n = (int) d.d_namlen; ],
       fu_cv_lib_c_dirent_d_namlen=yes,
       fu_cv_lib_c_dirent_d_namlen=no)])
  if test $fu_cv_lib_c_dirent_d_namlen = yes; then
    AC_DEFINE(DIRENT_HAVE_D_NAMLEN, 1,
      [Define if you have d_namlen in struct dirent])
  fi
  AC_CACHE_CHECK(for d_reclen in dirent, fu_cv_lib_c_dirent_d_reclen,
    [AC_TRY_COMPILE([#include <dirent.h>],
       [ struct dirent d; int n; n = (int) d.d_reclen; ],
       fu_cv_lib_c_dirent_d_reclen=yes,
       fu_cv_lib_c_dirent_d_reclen=no)])
  if test $fu_cv_lib_c_dirent_d_reclen = yes; then
    AC_DEFINE(DIRENT_HAVE_D_RECLEN, 1,
      [Define if you have d_reclen in struct dirent])
  fi)

dnl ---------------------------------------------
dnl Accept user defined path leading to libraries and headers.

AC_DEFUN(CODA_OPTION_SUBSYS,
  [AC_ARG_WITH($1,
    [  --with-$1=DIR	$1 was installed in DIR],
    [ pfx="`(cd ${withval} ; pwd)`"
      CPPFLAGS="${CPPFLAGS} -I${pfx}/include"
      LDFLAGS="${LDFLAGS} -L${pfx}/lib"
      if test x$RFLAG != x ; then
	LDFLAGS="${LDFLAGS} -R${pfx}/lib"
      fi
      PATH="${PATH}:${pfx}/bin:${pfx}/sbin"])
    ])

AC_DEFUN(CODA_OPTION_LIBRARY,
  [AC_ARG_WITH($1-includes,
    [  --with-$1-includes=DIR	$1 include files are in DIR],
    [ pfx="`(cd ${withval} ; pwd)`"
      CPPFLAGS="${CPPFLAGS} -I${pfx}"])
   AC_ARG_WITH($1-libraries,
    [  --with-$1-libraries=DIR	$1 library files are in DIR],
    [ pfx="`(cd ${withval} ; pwd)`"
      LDFLAGS="${LDFLAGS} -L${pfx}"
      if test x$RFLAG != x ; then
            LDFLAGS="${LDFLAGS} -R${pfx}"
      fi]) ])

AC_DEFUN(CODA_OPTION_CRYPTO,
  [AC_ARG_WITH(crypto,
    [  --with-crypto		Use openssl and/or kerberos libraries],
    [CRYPTO=${withval}], [CRYPTO=no])])

dnl ---------------------------------------------
dnl Search for an installed library in:
dnl	 /usr/lib /usr/local/lib /usr/pkg/lib ${prefix}/lib

AC_DEFUN(CODA_FIND_LIB,
 [AC_CACHE_CHECK(location of lib$1, coda_cv_path_$1,
  [saved_CFLAGS="${CFLAGS}" ; saved_LDFLAGS="${LDFLAGS}" ; saved_LIBS="${LIBS}"
   coda_cv_path_$1=none ; LIBS="-l$1 $4"
   for path in default /usr /usr/local /usr/pkg ${prefix} ; do
     if test ${path} != default ; then
       CFLAGS="${CFLAGS} -I${path}/include"
       LDFLAGS="${LDFLAGS} -L${path}/lib"
     fi
     AC_TRY_LINK([$2], [$3], [coda_cv_path_$1=${path} ; break])
     CFLAGS="${saved_CFLAGS}" ; LDFLAGS="${saved_LDFLAGS}"
   done
   LIBS="${saved_LIBS}"
  ])
  case ${coda_cv_path_$1} in
    none) AC_MSG_ERROR("Cannot determine the location of lib$1")
          ;;
    default)
	  ;;
    *)    CPPFLAGS="-I${coda_cv_path_$1}/include ${CPPFLAGS}"
          LDFLAGS="${LDFLAGS} -L${coda_cv_path_$1}/lib" 
	  if test x$RFLAG != x ; then
            LDFLAGS="${LDFLAGS} -R${coda_cv_path_$1}/lib "
	  fi
          ;;
  esac])

dnl ---------------------------------------------
dnl Fail if we haven't been able to find some required component
dnl
AC_DEFUN(CODA_FAIL_IF_MISSING, [ test -z "${$1}" && AC_MSG_ERROR($2) ])

AC_DEFUN(CODA_RFLAG,
  [ dnl put in standard -R flag if needed
    if test x${RFLAG} != x ; then
      if test x${libdir} != xNONE ; then
        LDFLAGS="${LDFLAGS} -R${libdir}"
      elif test x${prefix} != xNONE ; then
        LDFLAGS="${LDFLAGS} -R${prefix}/lib"
      else 
        LDFLAGS="${LDFLAGS} -R${ac_default_prefix}/lib"
      fi
    fi])

dnl ---------------------------------------------
dnl Mac OS X has a weird compiler toolchain.
dnl
dnl Long story: The compiler tool chain in the Mac OS X Developer Tools is a
dnl strange beast. The compiler is based on the gcc 2.95.2 suite, with
dnl modifications to support the Objective C language and some Darwin quirks.
dnl The preprocessor (cpp) is available in two versions. One is the standard
dnl precompiler (from gcc 2.95.2), the other one is a special precompiler
dnl written by Apple, with support for precompiled headers. The latter one is
dnl used by default, because it is faster. However, some code doesn't compile
dnl with Apple's precompiler, so you must use the -traditional-cpp option to
dnl get the standard precompiler.
dnl
dnl (taken from http://fink.sourceforge.net/darwin/porting.php)

AC_DEFUN(CODA_DARWIN_BROKEN_CPP_WORKAROUND,
  [case "$build" in
    powerpc-apple-darwin*)
	CPPFLAGS="$CPPFLAGS -traditional-cpp"
	;;
   esac])

dnl ---------------------------------------------
dnl find readline functionality
dnl also test for new functions introduced by readline 4.2

AC_SUBST(LIBREADLINE)
AC_DEFUN(CODA_CHECK_READLINE,
  [AC_CHECK_LIB(readline, main, [LIBREADLINE=-lreadline], [], $LIBTERMCAP)
   AC_CHECK_LIB(readline, rl_completion_matches,
     [AC_DEFINE(HAVE_RL_COMPLETION_MATCHES, 1, [Define if you have readline 4.2 or later])], [], $LIBTERMCAP)])

