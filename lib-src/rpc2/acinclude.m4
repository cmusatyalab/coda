
dnl ---------------------------------------------
dnl translate easy to remember target names into recognizable gnu variants and
dnl test the cross compilation platform and adjust default settings

AC_DEFUN(CODA_SETUP_BUILD,
[AC_SUBST(LIBTOOL_LDFLAGS)
case ${target} in
  djgpp | win95 | dos )  target=i386-pc-msdos ;;
  cygwin* | winnt | nt ) target=i386-pc-cygwin ;;
esac
AC_CANONICAL_SYSTEM
if test ${build} != ${target} ; then
  case ${target} in
   i386-pc-msdos )
    dnl shared libraries don't work here
    AM_DISABLE_SHARED
    CC="dos-gcc -bmmap"
    CXX="dos-gcc -bmmap"
    AR="dos-ar"
    RANLIB="true"
    AS="dos-as"
    NM="dos-nm"

    dnl We have to override some things the configure script tends to
    dnl get wrong as it tests the build platform feature
    ac_cv_func_mmap_fixed_mapped=yes
    ;;
   i386-pc-cygwin )
    dnl -D__CYGWIN32__ should be defined but sometimes isn't (wasn't?)
    host=i386-pc-cygwin
    CC="gnuwin32gcc -D__CYGWIN32__"
    CXX="gnuwin32g++"
    AR="gnuwin32ar"
    RANLIB="gnuwin32ranlib"
    AS="gnuwin32as"
    NM="gnuwin32nm"
    DLLTOOL="gnuwin32dlltool"
    OBJDUMP="gnuwin32objdump"

    LDFLAGS="-L/usr/gnuwin32/lib"

    dnl We seem to need these to get a dll built
    libtool_flags="--enable-win32-dll"
    LIBTOOL_LDFLAGS="-no-undefined"
    ;;
 esac
fi])

dnl ---------------------------------------------
dnl Specify paths to the lwp includes and libraries

AC_DEFUN(CODA_OPTION_LWP,
 [AC_ARG_WITH(lwp-includes,
    [  --with-lwp-includes     Location of the the lwp include files],
    [ CPPFLAGS="${CPPFLAGS} -I`(cd ${withval} ; pwd)`" ])
  AC_ARG_WITH(lwp-library,
    [  --with-lwp-library      Location of the lwp library files],
    [ LDFLAGS="${LDFLAGS} -L`(cd ${withval} ; pwd)`" ])
  AC_ARG_WITH(lwp-pt,
    [  --with-lwp-pt           Link against *experimental* lwp_pt library],
    [ with_LWP_PT=yes ; DEFS="${DEFS} -D_REENTRANT" ],
    [ with_LWP_PT=no ])
 ]) 

dnl ---------------------------------------------
dnl Search for an installed lwp library

AC_DEFUN(CODA_FIND_LIBLWP,
 [AC_CACHE_CHECK(location of liblwp, coda_cv_lwppath,
  [saved_LDFLAGS="${LDFLAGS}" ; saved_LIBS="${LIBS}"
   coda_cv_lwppath=none ; LIBS="-llwp"
   for path in ${prefix} /usr /usr/local /usr/pkg ; do
     LDFLAGS="${LDFLAGS} -L${path}/lib"
     AC_TRY_LINK([], [int main(){return 0;}],
                 [coda_cv_lwppath=${path} ; break])
   done
   LDFLAGS="${saved_LDFLAGS}" ; LIBS="${saved_LIBS}"])
 case $coda_cv_lwppath in
   none) AC_MSG_ERROR("Cannot determine the location of liblwp") ;;
   /usr) ;;
   *)    CPPFLAGS="${CPPFLAGS} -I${coda_cv_lwppath}/include"
         LDFLAGS="${LDFLAGS} -L${coda_cv_lwppath}/lib" ;;
 esac])

dnl ---------------------------------------------
dnl Find the native C compiler in order to generate a working rp2gen

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

dnl ---------------------------------------------
dnl Check which lib provides termcap functionality

AC_SUBST(LIBTERMCAP)
AC_DEFUN(CODA_CHECK_LIBTERMCAP,
 [saved_LIBS=${LIBS} ; LIBS=
  AC_SEARCH_LIBS(tgetent, [ncurses termcap], [LIBTERMCAP=${LIBS}])
  LIBS=${saved_LIBS}])

dnl ---------------------------------------------
dnl Check whether bcopy is defined in strings.h

AC_DEFUN(CODA_CHECK_BCOPY, 
  AC_CACHE_CHECK(for bcopy in strings.h,
    fu_cv_lib_c_bcopy,  
    [AC_TRY_COMPILE([#include <stdlib.h>
#include <strings.h>], 
      [ char *str; str = (char *) malloc(5); (void) bcopy("test", str, 5);],
      fu_cv_lib_c_bcopy=yes,
      fu_cv_lib_c_bcopy=no)])
  if test $fu_cv_lib_c_bcopy = yes; then
    AC_DEFINE(HAVE_BCOPY_IN_STRINGS_H)
  fi)

dnl ---------------------------------------------
dnl Check whether we have inet_aton

AC_DEFUN(CODA_CHECK_INET_ATON, 
  AC_CACHE_CHECK(for inet_aton,
    fu_cv_lib_c_inet_aton,  
    [AC_TRY_LINK([#include <stdlib.h>
#include <arpa/inet.h>], 
      [ struct in_addr res; (void) inet_aton("255.255.255.255", &res);],
      fu_cv_lib_c_inet_aton=yes,
      fu_cv_lib_c_inet_aton=no)])
  if test $fu_cv_lib_c_inet_aton = yes; then
    AC_DEFINE(HAVE_INET_ATON)
  fi)

dnl ---------------------------------------------
dnl Check whether we have inet_ntoa

AC_DEFUN(CODA_CHECK_INET_NTOA, 
  AC_CACHE_CHECK(for inet_ntoa,
    fu_cv_lib_c_inet_ntoa,  
    [AC_TRY_LINK([#include <stdlib.h>
#include <arpa/inet.h>], 
      [ struct in_addr res; (void) inet_ntoa(res);],
      fu_cv_lib_c_inet_ntoa=yes,
      fu_cv_lib_c_inet_ntoa=no)])
  if test $fu_cv_lib_c_inet_ntoa = yes; then
    AC_DEFINE(HAVE_INET_NTOA)
  fi)

