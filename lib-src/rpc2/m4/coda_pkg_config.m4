#serial 3
dnl ---------------------------------------------
dnl Make sure we remember the PKG_CONFIG_PATH environment variable during
dnl automatic reconfiguration and extend the search path to also search in
dnl ${prefix}/lib/pkgconfig and /usr/local/lib/pkgconfig if it wasn't set.
dnl
dnl Also search in LWP source tree if it is installed adjacent to here.
dnl
AC_DEFUN([CODA_PKG_CONFIG],
    [AC_ARG_VAR([PKG_CONFIG_PATH], [directories searched by pkg-config.])
    if test -z "${PKG_CONFIG_PATH}" ; then
	PKG_CONFIG_PATH="${libdir}/pkgconfig:/usr/local/lib/pkgconfig"
	export PKG_CONFIG_PATH
    fi
    PKG_CONFIG_PATH="${ac_pwd}/../lwp:${PKG_CONFIG_PATH}"
    export PKG_CONFIG_PATH])

