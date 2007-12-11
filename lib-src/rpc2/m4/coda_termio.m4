#serial 1
dnl ---------------------------------------------
dnl Check which lib provides termcap functionality
dnl
AC_SUBST(LIBTERMCAP)
AC_DEFUN([CODA_CHECK_LIBTERMCAP],
 [saved_LIBS=${LIBS} ; LIBS=
  AC_SEARCH_LIBS(tgetent, [ncurses termcap], [LIBTERMCAP=${LIBS}])
  LIBS=${saved_LIBS}])

dnl ---------------------------------------------
dnl find readline functionality
dnl also test for new functions introduced by readline 4.2
dnl
AC_SUBST(LIBREADLINE)
AC_DEFUN([CODA_CHECK_READLINE],
  [AC_CHECK_LIB(readline, main, [LIBREADLINE=-lreadline], [], [${LIBTERMCAP}])
   AM_CONDITIONAL(HAVE_READLINE, test x${LIBREADLINE} != x)
   AC_CHECK_LIB(readline, rl_completion_matches,
     [AC_DEFINE(HAVE_RL_COMPLETION_MATCHES, 1, [Define if you have readline 4.2 or later])], [], [${LIBTERMCAP}])])

