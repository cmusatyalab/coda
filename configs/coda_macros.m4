dnl Macro to run configure on subprojects before continuing. This way we are
dnl assured that generated files like uninstalled pkg-config files are
dnl present.
AC_DEFUN([CODA_CONFIG_SUBDIRS],
  [_AC_OUTPUT_SUBDIRS()
   no_recursion=yes])

dnl Check for a curses library, and if it needs termcap
AC_SUBST(LIBCURSES)
AC_DEFUN([CODA_CHECK_LIBCURSES],
  [AC_CHECK_LIB(ncurses, main, [LIBCURSES="-lncurses"],
	[AC_CHECK_LIB(curses, main, [LIBCURSES="-lcurses"],
	    [AC_MSG_WARN("failed to find curses library")
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
  fi])


dnl check wether we have flock or fcntl
AC_DEFUN([CODA_CHECK_FILE_LOCKING],
  [AC_CACHE_CHECK(for file locking by fcntl,
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
  fi])

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

dnl ---------------------------------------------
dnl Search for an installed library in:
dnl	 /usr/lib /usr/local/lib /usr/pkg/lib ${prefix}/lib
dnl
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

dnl -----------------
dnl Looks for the fltk library
dnl
AC_SUBST(FLTKFLAGS)
AC_SUBST(FLTKLIBS)
AC_DEFUN([CODA_CHECK_FLTK],
  [case $host_os in
   cygwin*)
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
