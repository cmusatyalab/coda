AC_DEFUN(CODA_CC_FEATURE_TEST,
  [AC_CACHE_CHECK(whether the C compiler accepts $2,
     coda_cv_cc_$1,
     coda_saved_CC="$CC"
     CC="$CC $2"
     AC_LANG_SAVE
     AC_LANG_C
     AC_TRY_COMPILE([], [],
       coda_cv_cc_$1=yes,
       coda_cv_cc_$1=no)
     AC_LANG_RESTORE
     CC="$coda_saved_CC")
   if test $coda_cv_cc_$1 = yes; then
     CC="$CC $2"
   fi])

AC_DEFUN(CODA_CXX_FEATURE_TEST,
  [AC_CACHE_CHECK(whether the C++ compiler accepts $2,
     coda_cv_cxx_$1,
     coda_saved_CXX="$CXX"
     CXX="$CXX $2"
     AC_LANG_SAVE
     AC_LANG_CPLUSPLUS
     AC_TRY_COMPILE([], [],
       coda_cv_cxx_$1=yes,
       coda_cv_cxx_$1=no)
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


dnl Find an installed libdb-1.85
dnl  cygwin and glibc-2.0 have libdb
dnl  BSD systems have it in libc
dnl  glibc-2.1 has libdb1 (and libdb is db2)
dnl  Solaris systems have dbm in libc
dnl All coda-servers _must_ use the same library, otherwise certain databases
dnl cannot be replicated.
AC_SUBST(LIBDB)
AC_DEFUN(CODA_CHECK_LIBDB185,
  AC_CHECK_LIB(db, dbopen, [LIBDB="-ldb"],
    [AC_CHECK_LIB(c, dbopen, [LIBDB=""],
       [AC_CHECK_LIB(c, dbm_open, [],
          [if test $target != i386-pc-djgpp ; then
	     AC_MSG_ERROR("failed to find libdb")
	   fi])])])

  if test "$ac_cv_lib_c_dbm_open" = yes; then
    [AC_MSG_WARN([Found ndbm instead of libdb 1.85. This uses])
     AC_MSG_WARN([an incompatible disk file format and the programs])
     AC_MSG_WARN([will not be able to read replicated shared databases.])
     sleep 5
    AC_DEFINE(HAVE_NDBM)]
  else
    dnl Check if the found libdb is libdb2 using compatibility mode.
    AC_MSG_CHECKING("if $LIBDB is libdb 1.85")
    coda_save_LIBS="$LIBS"
    LIBS="$LIBDB $LIBS"
    AC_TRY_LINK([char db_open();], db_open(),
      [AC_MSG_RESULT("no")
       AC_CHECK_LIB(db1, dbopen, [LIBDB="-ldb1"],
         dnl It probably will compile but it wont be compatible..
         [AC_MSG_WARN([Found libdb2 instead of libdb 1.85. This uses])
          AC_MSG_WARN([an incompatible disk file format and the programs])
          AC_MSG_WARN([will not be able to read replicated shared databases.])])
	  sleep 5
       dnl In any case we should not be using db.h
       AC_DEFINE(HAVE_DB_185_H)],
      [AC_MSG_RESULT("yes")])
    LIBS="$coda_save_LIBS"
  fi)

dnl We cannot test for setpgrp(void) when cross-compiling, let's just assume
dnl we're POSIX-compliant in that case.
AC_DEFUN(CODA_FUNC_SETPGRP,
  if test $target != "i386-pc-cygwin32" -a $target != "i386-pc-djgpp" ; then
    [AC_FUNC_SETPGRP]
  else
    [AC_DEFINE(SETPGRP_VOID)]
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
    AC_DEFINE(HAVE_FCNTL_LOCKING)
  fi

  AC_CACHE_CHECK(for file locking by flock,
    fu_cv_lib_c_flock,
    [AC_TRY_COMPILE([#include <sys/file.h>
#include <stdio.h>], [ int fd; flock(fd, LOCK_SH);],
      fu_cv_lib_c_flock=yes,
      fu_cv_lib_c_flock=no)])
  if test $fu_cv_lib_c_flock = yes; then
    AC_DEFINE(HAVE_FLOCK_LOCKING)
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
    AC_DEFINE(HAVE_BCOPY_IN_STRINGS_H)
  fi)

AC_SUBST(LIBUCB)
AC_DEFUN(CODA_CHECK_LIBUCB, 
  [AC_MSG_CHECKING(if bcopy lives in libucb)
   AC_TRY_LINK([char bcopy();], bcopy(),
     [AC_MSG_RESULT("no")],
     [coda_save_LIBS="$LIBS"
      LIBS="$LIBS /usr/ucblib/libucb.a"
      AC_TRY_LINK([char bcopy();], bcopy(),
        [AC_MSG_RESULT("yes")
	 LIBUCB="/usr/ucblib/libucb.a"]
        [AC_MSG_ERROR("Cannot figure out where bcopy lives")])
      LIBS="$coda_save_LIBS"])
  ])

dnl check wether we have setenv
AC_DEFUN(CODA_CHECK_SETENV, 
  AC_CACHE_CHECK(for setenv,
    fu_cv_lib_c_setenv,  
    [AC_TRY_LINK([#include <stdlib.h>], 
      [ setenv("TEST", "test", 1);],
      fu_cv_lib_c_setenv=yes,
      fu_cv_lib_c_setenv=no)])
  if test $fu_cv_lib_c_setenv = yes; then
    AC_DEFINE(HAVE_SETENV)
  fi)

dnl check wether we have scandir
AC_DEFUN(CODA_CHECK_SCANDIR, 
  AC_CACHE_CHECK(for scandir,
    fu_cv_lib_c_scandir,  
    [AC_TRY_LINK([#include <stdlib.h>
#include <dirent.h>], 
      [ struct dirent **namelist; int n; n = scandir("/", &namelist, NULL, NULL);],
      fu_cv_lib_c_scandir=yes,
      fu_cv_lib_c_scandir=no)])
  if test $fu_cv_lib_c_scandir = yes; then
    AC_DEFINE(HAVE_SCANDIR)
  else
   AC_CACHE_CHECK(for d_namlen in dirent,
    fu_cv_lib_c_dirent_d_namlen,
    [AC_TRY_COMPILE([#include <dirent.h>],
       [ struct dirent d; int n; n = (int) d.d_namlen; ],
       fu_cv_lib_c_dirent_d_namlen=yes,
       fu_cv_lib_c_dirent_d_namlen=no)])
   if test $fu_cv_lib_c_dirent_d_namlen = yes; then
      AC_DEFINE(DIRENT_HAVE_D_NAMLEN)
   else
     AC_CACHE_CHECK(for d_reclen in dirent,
      fu_cv_lib_c_dirent_d_reclen,
      [AC_TRY_COMPILE([#include <dirent.h>],
         [ struct dirent d; int n; n = (int) d.d_reclen; ],
         fu_cv_lib_c_dirent_d_reclen=yes,
         fu_cv_lib_c_dirent_d_reclen=no)])
     if test $fu_cv_lib_c_dirent_d_reclen = yes; then
        AC_DEFINE(DIRENT_HAVE_D_RECLEN)
     fi
   fi
  fi)

dnl ---------------------------------------------
dnl Path leading to the lwp headers and library.

AC_DEFUN(CODA_OPTION_LWP,
  [AC_ARG_WITH(lwp-includes,
    [  --with-lwp-includes	Location of the lwp include files],
    [ CFLAGS="${CFLAGS} -I`(cd ${withval} ; pwd)`"
      CXXFLAGS="${CXXFLAGS} -I`(cd ${withval} ; pwd)`" ])
   AC_ARG_WITH(lwp-library,
    [  --with-lwp-library	Location of the lwp library files],
    [ LDFLAGS="${LDFLAGS} -L`(cd ${withval} ; pwd)`" ])])

dnl ---------------------------------------------
dnl Search for an installed lwp library

AC_DEFUN(CODA_FIND_LIBLWP,
 [AC_CACHE_CHECK(location of liblwp, coda_cv_lwppath,
  [saved_CFLAGS="${CFLAGS}" ; saved_LDFLAGS="${LDFLAGS}" ; saved_LIBS="${LIBS}"
   coda_cv_lwppath=none ; LIBS="-llwp"
   for path in ${prefix} /usr /usr/local /usr/pkg ; do
     CFLAGS="${CFLAGS} -I${path}/include"
     LDFLAGS="${LDFLAGS} -L${path}/lib"
     AC_TRY_LINK([#include <lwp/lwp.h>], [LWP_Init(0,0,0)],
		 [coda_cv_lwppath=${path} ; break])
   done
   CFLAGS="${saved_CFLAGS}" ; LDFLAGS="${saved_LDFLAGS}" ; LIBS="${saved_LIBS}"
  ])
  case $coda_cv_lwppath in
    none) AC_MSG_ERROR("Cannot determine the location of liblwp")
          ;;
    /usr) ;;
    *)    CFLAGS="${CFLAGS} -I${coda_cv_lwppath}/include"
          CXXFLAGS="${CXXFLAGS} -I${coda_cv_lwppath}/include"
          LDFLAGS="${LDFLAGS} -L${coda_cv_lwppath}/lib"
          ;;
  esac])
   
