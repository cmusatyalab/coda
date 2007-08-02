AC_DEFUN([CODA_CC_FEATURE_TEST],
  [AC_CACHE_CHECK(whether the C compiler accepts $2, coda_cv_cc_$1,
     coda_saved_CC="$CC" ; CC="$CC $2" ; AC_LANG_SAVE
     AC_LANG_C
     AC_TRY_COMPILE([], [], coda_cv_cc_$1=yes, coda_cv_cc_$1=no)
     AC_LANG_RESTORE
     CC="$coda_saved_CC")
   if test $coda_cv_cc_$1 = yes; then
     CC="$CC $2"
   fi])

AC_DEFUN([CODA_CXX_FEATURE_TEST],
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
AC_DEFUN([CODA_PROG_NATIVECC],
    if test $cross_compiling = yes ; then
       [AC_MSG_CHECKING([for native C compiler on the build host])
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
AC_DEFUN([CODA_CHECK_TYPE],
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
AC_DEFUN([CODA_CHECK_LIBTERMCAP],
  AC_CHECK_LIB(termcap, main, [LIBTERMCAP="-ltermcap"],
    [AC_CHECK_LIB(ncurses, tgetent, [LIBTERMCAP="-lncurses"])]))

dnl Check for a curses library, and if it needs termcap
AC_SUBST(LIBCURSES)
AC_DEFUN([CODA_CHECK_LIBCURSES],
  if test "$target" != "i386-pc-cygwin32" ; then
    AC_CHECK_LIB(ncurses, main, [LIBCURSES="-lncurses"],
	[AC_CHECK_LIB(curses, main, [LIBCURSES="-lcurses"],
	    [AC_MSG_ERROR("failed to find curses library")
	    ], $LIBTERMCAP)
	], $LIBTERMCAP)
    AC_CACHE_CHECK([if curses library requires -ltermcap],
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
AC_DEFUN([CODA_CHECK_FILE_LOCKING],
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

AC_DEFUN([CODA_CHECK_LIBCOMERR],
 [AC_CHECK_HEADERS(com_err.h)
  AC_SEARCH_LIBS(com_err, com_err, [test "$ac_cv_search_com_err" = "none required" || LIBCOMERR="$ac_cv_search_com_err"])])

AC_SUBST(LIBKRB4)
AC_DEFUN([CODA_CHECK_KRB4],
 [CODA_OPTION_LIBRARY(krb4)
  AC_ARG_WITH(krb4,
   [  --with-krb4			Link against kerberos4 libraries],
   [CODA_CHECK_LIBCOMERR
    AC_CHECK_HEADERS(krb.h des.h)
    coda_save_LIBS="$LIBS"
    LIBS="$LIBCOMERR $LIBS"
    AC_SEARCH_LIBS(krb_get_lrealm, krb4 krb,
      [test "$ac_cv_search_krb_get_lrealm" = "none required" || LIBKRB4="$ac_cv_search_krb_get_lrealm ${LIBKRB4}"])
    LIBS="$coda_save_LIBS"
    if test "$ac_cv_search_krb_get_lrealm" != no ; then
	AC_DEFINE(HAVE_KRB4, 1, [Define if kerberos 4 is available])
    fi])])

AC_SUBST(LIBKRB5)
AC_DEFUN([CODA_CHECK_KRB5],
 [CODA_OPTION_LIBRARY(krb5)
  AC_ARG_WITH(krb5,
   [  --with-krb5			Link against kerberos5 libraries],
   [CODA_CHECK_LIBCOMERR
    AC_CHECK_HEADERS(krb5.h)
    if test "$ac_cv_header_krb5_h" = yes -a "$ac_cv_header_com_err_h" = yes;then
	coda_save_LIBS="$LIBS"
	LIBS="$LIBCOMERR $LIBS"
	dnl is this MIT-krb5 or Heimdal
	MITKRB5="no"
	AC_SEARCH_LIBS(krb5_encrypt, k5crypto,
	    [MITKRB5="yes" ; LIBKRB5="$ac_cv_search_krb5_encrypt ${LIBKRB5}"])
	if test "$MITKRB5" = no ; then
	    AC_SEARCH_LIBS(crypt, crypt,
		[test "$ac_cv_search_crypt" = "none required" || LIBKRB5="$ac_cv_search_crypt ${LIBKRB5}"])
	    dnl Heimdal cygwin might need libdes
	    dnl AC_SEARCH_LIBS(unknown_symbol, des,
	    dnl     [test "$ac_cv_search_unknown_symbol" = "none required" || LIBKRB5="$ac_cv_search_unknown_symbol ${LIBKRB5}"])
	    AC_SEARCH_LIBS(roken_concat, roken,
		[test "$ac_cv_search_roken_concat" = "none required" || LIBKRB5="$ac_cv_search_roken_concat ${LIBKRB5}"])
	    AC_SEARCH_LIBS(copy_PrincipalName, asn1,
		[test "$ac_cv_search_copy_Principal" = "none required" || LIBKRB5="$ac_cv_search_copy_PrincipalName ${LIBKRB5}"])
	fi
	dnl Everyone wants libkrb5
	AC_SEARCH_LIBS(krb5_init_context, krb5,
	    [test "$ac_cv_search_krb5_init_context" = "none required" || LIBKRB5="$ac_cv_search_krb5_init_context ${LIBKRB5}"])
	LIBS="$coda_save_LIBS"
	if test "$ac_cv_search_krb5_init_context" != no ; then
	    AC_DEFINE(HAVE_KRB5, 1, [Define if kerberos 5 is available])
	    if test "$MITKRB5" = no ; then
		AC_DEFINE(HAVE_HEIMDAL, 1, [Define if using heimdal as kerberos5 library])
	    fi
	else
	    LIBKRB5=""
	fi
    else
	AC_MSG_WARN([Couldn't find krb5.h and com_err.h headers, not using kerberos 5])
    fi])])


dnl ---------------------------------------------
dnl Test for possible offsetof implementation that doesn't give compile
dnl warnings or errors
dnl
AC_DEFUN([CODA_TEST_OFFSETOF],
   [AC_MSG_CHECKING(offsetof using $2)
    offsetof=no
    AC_LANG_PUSH(C++)
    saved_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -Werror"
    AC_RUN_IFELSE([
	AC_LANG_PROGRAM([[
	    #include <sys/types.h>
	    #include <stddef.h>
	    struct b { struct b *C; };
	    class A {public: A& operator=(const A& x){return *this;}; struct b B;};
	    #define off(type,member) $2]],
	    [[A *a=NULL; if (((char *)&a->B - off(A,B)) != (char *)a) return 1]])],
	[AC_DEFINE(CODA_OFFSETOF_$1, 1, [offsetof works with $2])
	 offsetof=yes], [], [true])
    CPPFLAGS="$saved_CPPFLAGS"
    AC_LANG_POP(C++)
    AC_MSG_RESULT($offsetof)])

AC_DEFUN([CODA_CHECK_OFFSETOF],
   [CODA_TEST_OFFSETOF(OFFSETOF, [offsetof(type,member)])
    if test "$offsetof" != yes ; then
    CODA_TEST_OFFSETOF(PTR_TO_MEMBER, [((size_t)(&type::member))])
    if test "$offsetof" != yes ; then
    CODA_TEST_OFFSETOF(REINTERPRET_CAST, [((size_t)(&(reinterpret_cast<type*>(__alignof__(type*)))->member)-__alignof__(type*))])
    fi ; fi])

dnl ---------------------------------------------
dnl Accept user defined path leading to libraries and headers.

AC_DEFUN([CODA_OPTION_SUBSYS],
  [AC_ARG_WITH($1,
    [  --with-$1=DIR	$1 was installed in DIR],
    [ pfx="`(cd ${withval} ; pwd)`"
      CPPFLAGS="${CPPFLAGS} -I${pfx}/include"
      LDFLAGS="${LDFLAGS} -L${pfx}/lib"
      PATH="${PATH}:${pfx}/bin:${pfx}/sbin"])
    ])

AC_DEFUN([CODA_OPTION_LIBRARY],
  [AC_ARG_WITH($1-includes,
    [  --with-$1-includes=DIR	$1 include files are in DIR],
    [ pfx="`(cd ${withval} ; pwd)`"
      CPPFLAGS="${CPPFLAGS} -I${pfx}"])
   AC_ARG_WITH($1-libraries,
    [  --with-$1-libraries=DIR	$1 library files are in DIR],
    [ pfx="`(cd ${withval} ; pwd)`"
      LDFLAGS="${LDFLAGS} -L${pfx}"
    ]) ])

AC_DEFUN([CODA_OPTION_LWP_PT],
  [AC_ARG_WITH(lwp-pt,
    [  --with-lwp-pt		Link against *experimental* lwp_pt library],
    [with_LWP_PT=${withval}], [with_LWP_PT=no])])

AC_SUBST(LIBLWP)
AC_SUBST(LIBPTHREAD)
AC_DEFUN([CODA_CHECK_LWP_PT],
   [if test ${with_LWP_PT}p = nop ; then
     AC_CHECK_LIB(lwp, LWP_Init, [LIBLWP="-llwp"])
    else
     AC_CHECK_LIB(pthread, pthread_create, [LIBPTHREAD="-lpthread"])
     AC_CHECK_LIB(lwp_pt, LWP_Init, [LIBLWP="-llwp_pt"],
	 [AC_MSG_ERROR("Failed to locate liblwp_pt")], [${LIBPTHREAD}])
     CPPFLAGS="-D_REENTRANT ${CPPFLAGS}"
   fi])

dnl ---------------------------------------------
dnl Search for an installed library in:
dnl	 /usr/lib /usr/local/lib /usr/pkg/lib ${prefix}/lib

AC_DEFUN([CODA_FIND_LIB],
 [AC_CACHE_CHECK(location of lib$1, coda_cv_path_$1,
  [saved_CFLAGS="${CFLAGS}" ; saved_LDFLAGS="${LDFLAGS}" ; saved_LIBS="${LIBS}"
   coda_cv_path_$1=none ; LIBS="-l$1 $4"
   for path in default /usr /usr/local /usr/pkg /usr/X11R6 /usr/X11 /usr/openwin ${prefix} ; do
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
    none) ;;
    default) ;;
    *)    CPPFLAGS="-I${coda_cv_path_$1}/include ${CPPFLAGS}"
	  LDFLAGS="${LDFLAGS} -L${coda_cv_path_$1}/lib"
	  ;;
  esac])

AC_DEFUN([CODA_REQUIRE_LIB],
  [if test "${coda_cv_path_$1}" = none ; then
	AC_MSG_ERROR("Cannot determine the location of lib$1")
   fi])

dnl ---------------------------------------------
dnl Fail if we haven't been able to find some required component
dnl
AC_DEFUN([CODA_FAIL_IF_MISSING], [ test -z "${$1}" && AC_MSG_ERROR($2) ])

dnl ---------------------------------------------
dnl find readline functionality
dnl also test for new functions introduced by readline 4.2

AC_SUBST(LIBREADLINE)
AC_DEFUN([CODA_CHECK_READLINE],
  [CODA_FIND_LIB(readline, [], rl_initialize(), $LIBTERMCAP)
   CODA_REQUIRE_LIB(readline)
   AC_CHECK_LIB(readline, rl_completion_matches,
     [AC_DEFINE(HAVE_RL_COMPLETION_MATCHES, 1, [Define if you have readline 4.2 or later])], [], $LIBTERMCAP)
   LIBREADLINE=-lreadline])

dnl -----------------
dnl Looks for the fltk library
dnl 
AC_SUBST(FLTKFLAGS)
AC_SUBST(FLTKLIBS)
AC_DEFUN([CODA_CHECK_FLTK],
  [case $target in
   *pc-cygwin*)
     FLTKFLAGS="-fno-exceptions -mwindows"
     FLTKLIBS="-lole32 -luuid -lcomctl32 -lwsock32"
     ;;
   *)
     CODA_FIND_LIB(X11, [], XFlush(), "$LIBS")
     FLTKLIBS="-lX11 $LIBS -lm"
     ;;
   esac
   AC_LANG_SAVE
   AC_LANG_CPLUSPLUS
   CODA_FIND_LIB(fltk, [#include <Fl/Fl.H>], Fl::run(), "$FLTKLIBS")
   AC_LANG_RESTORE
   AC_MSG_CHECKING([if we can build vcodacon])
   if test "${coda_cv_path_fltk}" != none ; then
     FLTKLIBS="-lfltk $FLTKLIBS"
     AC_MSG_RESULT([ yes])
   else
     AC_MSG_RESULT([ couldn't find suitable libfltk])
   fi])


dnl -----------------
dnl Looks for the libkvm library (for FreeBSD/NetBSD)
dnl 
AC_SUBST(LIBKVM)
AC_DEFUN([CODA_CHECK_LIBKVM],
  [CODA_FIND_LIB(kvm, [], kvm_openfiles())
  if test "${coda_cv_path_kvm}" != none ; then
    LIBKVM="-lkvm"
  fi])

