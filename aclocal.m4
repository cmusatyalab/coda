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
     CXX="$coda_saved_CC")
   if test $coda_cv_cxx_$1 = yes; then
     CXX="$CXX $2"
   fi])

dnl Check which lib provides termcap functionality
AC_SUBST(LIBTERMCAP)
AC_DEFUN(CODA_CHECK_LIBTERMCAP,
  AC_CHECK_LIB(termcap, main, [LIBTERMCAP="-ltermcap"],
    [AC_CHECK_LIB(ncurses, tgetent, [LIBTERMCAP="-lncurses"])]))

dnl Check for a curses library, and if it needs termcap
AC_SUBST(LIBCURSES)
AC_DEFUN(CODA_CHECK_LIBCURSES,
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
  fi)

dnl Find an installed libdb-1.85
dnl  cygwin and glibc-2.0 have libdb
dnl  BSD systems have it in libc
dnl  glibc-2.1 has libdb1 (and libdb is db2)
AC_SUBST(LIBDB)
AC_DEFUN(CODA_CHECK_LIBDB185,
  AC_CHECK_LIB(db, dbopen, [LIBDB="-ldb"],
    [AC_CHECK_LIB(c, dbopen, [LIBDB=""],
       [AC_MSG_ERROR("failed to find libdb")])])

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
        AC_MSG_WARN([will not be able to read existing databases.])])
	sleep 20
     dnl In any case we should not be using db.h
     AC_DEFINE(HAVE_DB_185_H)],
    [AC_MSG_RESULT("yes")])
  LIBS="$coda_save_LIBS")

