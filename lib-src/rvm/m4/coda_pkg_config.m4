#serial 1
dnl ---------------------------------------------
dnl extend the pkg-config search path to also
dnl search in ${prefix}/lib/pkgconfig and
dnl /usr/local/lib/pkgconfig
dnl
AC_DEFUN([CODA_PKG_CONFIG],
    [if test -z "$PKG_CONFIG_PATH" ; then
	PKG_CONFIG_PATH="${libdir}/pkgconfig:/usr/local/lib/pkgconfig"
	export PKG_CONFIG_PATH
    fi])

