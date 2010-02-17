#serial 1
dnl ---------------------------------------------
dnl
dnl Check for pthread support
dnl
dnl from: http://www.mail-archive.com/autoconf@gnu.org/msg12865.html
dnl
AC_DEFUN([CODA_PTHREAD_FLAGS],
  [ PTHREAD_CFLAGS=error
    PTHREAD_LIBS=error

    dnl If it's error, then the user didn't 
    dnl define it.
    if test "x$PTHREAD_LIBS" = xerror; then
	AC_CHECK_LIB(pthread, pthread_attr_init, [
		     PTHREAD_CFLAGS="-D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS"
		     PTHREAD_LIBS="-lpthread" ])
    fi

    if test "x$PTHREAD_LIBS" = xerror; then
	AC_CHECK_LIB(pthreads, pthread_attr_init, [
		     PTHREAD_CFLAGS="-D_THREAD_SAFE"
		     PTHREAD_LIBS="-lpthreads" ])
    fi

    if test "x$PTHREAD_LIBS" = xerror; then
	AC_CHECK_LIB(c_r, pthread_attr_init, [
		     PTHREAD_CFLAGS="-D_THREAD_SAFE -pthread"
		     PTHREAD_LIBS="-pthread" ])
    fi

    if test "x$PTHREAD_LIBS" = xerror; then
	AC_CHECK_FUNC(pthread_attr_init, [
		      PTHREAD_CFLAGS="-D_REENTRANT"
		      PTHREAD_LIBS="-lpthread" ])
    fi

    if test $PTHREAD_LIBS = "error"; then
	AC_MSG_WARN(pthread library NOT found: guessing and hoping for the best....)
	PTHREAD_CFLAGS="-D_REENTRANT"
	PTHREAD_LIBS="-lpthread"
    fi
    AC_SUBST(PTHREAD_CFLAGS)
    AC_SUBST(PTHREAD_LIBS)
])
