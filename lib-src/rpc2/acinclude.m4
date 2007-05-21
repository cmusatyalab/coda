
dnl ---------------------------------------------
dnl translate easy to remember target names into recognizable gnu variants and
dnl test the cross compilation platform and adjust default settings

AC_DEFUN([CODA_SETUP_BUILD],
[AC_SUBST(LIBTOOL_LDFLAGS)
case ${host_alias} in
  cygwin* | winnt | nt ) host_alias=i386-pc-cygwin ;;
  arm ) host_alias=arm-unknown-linux-gnuelf ;;
esac
AC_CANONICAL_HOST
if test ${cross_compiling} = yes ; then
  case ${host} in
   i386-pc-cygwin )
    dnl -D__CYGWIN32__ should be defined but sometimes isn't (wasn't?)
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
   arm-unknown-linux-gnuelf )
    CROSS_COMPILE="arm-unknown-linuxelf-"
    ;;
 esac
fi
if test "${CROSS_COMPILE}" ; then
  CC=${CROSS_COMPILE}gcc
  CXX=${CROSS_COMPILE}g++
  CPP="${CC} -E"
  AS=${CROSS_COMPILE}as
  LD=${CROSS_COMPILE}ld
  AR=${CROSS_COMPILE}ar
  RANLIB=${CROSS_COMPILE}ranlib
  NM=${CROSS_COMPILE}nm
  OBJDUMP=${CROSS_COMPILE}objdump
  DLLTOOL=${CROSS_COMPILE}dlltool
fi])

dnl ---------------------------------------------
dnl Specify paths to the lwp includes and libraries

AC_DEFUN([CODA_OPTION_LWP],
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
dnl Search for an installed library in:
dnl      /usr/lib /usr/local/lib /usr/pkg/lib ${prefix}/lib

AC_DEFUN([CODA_FIND_LIB],
 [AC_CACHE_CHECK(location of lib$1, coda_cv_path_$1,
  [saved_CFLAGS="${CFLAGS}" ; saved_LDFLAGS="${LDFLAGS}" ; saved_LIBS="${LIBS}"
   coda_cv_path_$1=none ; LIBS="-l$1"
   for path in default /usr /usr/local /usr/pkg ${prefix} ; do
     if test ${path} != default ; then
       CFLAGS="-I${path}/include ${CFLAGS}"
       LDFLAGS="-L${path}/lib ${LDFLAGS}"
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
    *)    CFLAGS="-I${coda_cv_path_$1}/include ${CFLAGS}"
          CXXFLAGS="-I${coda_cv_path_$1}/include ${CXXFLAGS}"
          LDFLAGS="-L${coda_cv_path_$1}/lib ${LDFLAGS}"
          ;;
  esac])

dnl ---------------------------------------------
dnl Find the native C compiler in order to generate a working rp2gen

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

AC_SUBST(LIBTERMCAP)
AC_DEFUN([CODA_CHECK_LIBTERMCAP],
 [saved_LIBS=${LIBS} ; LIBS=
  AC_SEARCH_LIBS(tgetent, [ncurses termcap], [LIBTERMCAP=${LIBS}])
  LIBS=${saved_LIBS}])

dnl ---------------------------------------------
dnl Define library version

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
dnl find readline functionality
dnl also test for new functions introduced by readline 4.2

AC_SUBST(LIBREADLINE)
AC_DEFUN([CODA_CHECK_READLINE],
  [AC_CHECK_LIB(readline, main, [LIBREADLINE=-lreadline], [], [${LIBTERMCAP}])
   AM_CONDITIONAL(HAVE_READLINE, test x${LIBREADLINE} != x)
   AC_CHECK_LIB(readline, rl_completion_matches,
     [AC_DEFINE(HAVE_RL_COMPLETION_MATCHES, 1, [Define if you have readline 4.2 or later])], [], [${LIBTERMCAP}])])

