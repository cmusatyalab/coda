#serial 1
dnl ---------------------------------------------
dnl Find the native C compiler in order to generate a working rp2gen
dnl
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
