
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
host=${target}
program_prefix=
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
    ;;

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
dnl Search for an installed library in:
dnl      /usr/lib /usr/local/lib /usr/pkg/lib ${prefix}/lib

AC_DEFUN(CODA_FIND_LIB,
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

