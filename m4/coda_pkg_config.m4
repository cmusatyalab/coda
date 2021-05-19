#serial 3
dnl ---------------------------------------------
dnl Make sure we remember the PKG_CONFIG_PATH environment variable during
dnl automatic reconfiguration and extend the search path to also search in
dnl ${prefix}/lib/pkgconfig and /usr/local/lib/pkgconfig if it wasn't set.
dnl
dnl Also search in local LWP, RPC2 and RVM source trees when they are
dnl found/installed under lib-src/.
dnl
AC_DEFUN([CODA_PKG_CONFIG],
    [AC_ARG_VAR([PKG_CONFIG_PATH], [directories searched by pkg-config.])
    if test -z "${PKG_CONFIG_PATH}" ; then
	PKG_CONFIG_PATH="${libdir}/pkgconfig:/usr/local/lib/pkgconfig"
	export PKG_CONFIG_PATH
    fi
    PKG_CONFIG_PATH="${ac_pwd}/lib-src/lwp:${ac_pwd}/lib-src/rpc2:${ac_pwd}/lib-src/rvm:${PKG_CONFIG_PATH}"
    export PKG_CONFIG_PATH])
