#serial 2
dnl ---------------------------------------------
dnl Make sure we remember the PKG_CONFIG_PATH environment variable during
dnl automatic reconfiguation extend the search path to also search in
dnl ${prefix}/lib/pkgconfig and /usr/local/lib/pkgconfig if it wasn't set
dnl
AC_DEFUN([CODA_PKG_CONFIG],
    [AC_ARG_VAR([PKG_CONFIG_PATH], [directories searched by pkg-config.])
    if test -z "${PKG_CONFIG_PATH}" ; then
	PKG_CONFIG_PATH="${libdir}/pkgconfig:/usr/local/lib/pkgconfig"
	export PKG_CONFIG_PATH
    fi])

