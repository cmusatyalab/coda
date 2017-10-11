dnl Process this file with autoconf to produce a configure script

dnl Initialization
AC_PREREQ(2.50)
AC_INIT(rpc2, 2.16)
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([rpc2-src/rpc2.private.h])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

dnl For library version updates, follow these steps in order.
dnl - when any library source has changed, increment first
dnl - when interfaces were added/removed changed, increment second and set
dnl   first to 0
dnl - if any interfaces were added, increment third
dnl - if any interfaces were removed, set third to 0
CODA_LIBRARY_VERSION(0, 12, 7)

CONFIG_DATE=`date +"%a, %d %b %Y %T %z"`
AC_SUBST(CONFIG_DATE, "$CONFIG_DATE", [Date when configure was last run])

dnl Checks for programs.
AC_PROG_CC
CODA_PROG_NATIVECC
AC_PROG_CPP
AM_PROG_LEX
if test -z "${ac_cv_prog_LEX}" ; then
    AC_MSG_ERROR([Build requires flex or lex lexical analyzer generator])
fi
AC_PROG_YACC
if test -z "${ac_cv_prog_YACC}" ; then
    AC_MSG_ERROR([Build requires bison, byacc or yacc parser generator])
fi
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_CHECK_PROG(CP, cp, cp)

dnl pkg-config stuff
CODA_PKG_CONFIG
PKG_CHECK_MODULES([LWP], [lwp])

AC_ARG_WITH([libuv],
    [AS_HELP_STRING([--without-libuv],
      [disable support for codatunneld])],
    [], [with_libuv=yes])
AS_IF([test "x$with_libuv" != "xno"],
  [PKG_CHECK_EXISTS([libuv >= 1.4.0],
     [PKG_CHECK_MODULES([LIBUV], [libuv])
      AC_DEFINE(USE_CODATUNNEL, 1, [Define this when linking against libuv])],
     [AC_MSG_WARN([LIBUV not found, disabling codatunneld build])
      with_libuv=no])])

AC_ARG_WITH(lua,
  [AS_HELP_STRING([--with-lua], [embed Lua interpreter])],
  [PKG_CHECK_EXISTS([lua5.1], [ PKG_CHECK_MODULES([LUA], [lua5.1]) ],
			       [ PKG_CHECK_MODULES([LUA], [lua]) ])
   AC_DEFINE(USE_LUA, 1, [Define this when linking against Lua])
   AC_SUBST(LUA_LIBADD, -lm, [Lua binding depends on libm])
   include_lua=true ],
  [include_lua=false ])

dnl Checks for libraries.
CODA_CHECK_LIBTERMCAP
CODA_CHECK_READLINE
AC_SEARCH_LIBS(bind, socket)
AC_SEARCH_LIBS(inet_ntoa, nsl)
AC_SEARCH_LIBS(gethostbyname, resolv)

dnl Checks for header files.
AC_CHECK_HEADERS(sys/stream.h arpa/inet.h netdb.h)

dnl Checks for types.
AC_CHECK_TYPES([struct sockaddr_storage, struct sockaddr_in6, socklen_t],,,
    [#include <sys/types.h>
     #include <sys/socket.h>
     #include <netinet/in.h>])

dnl Checks for compiler characteristics.
CODA_CC_FEATURE_TEST(Wall)

dnl Checks for library functions.
AC_CHECK_FUNCS(ffs iopen getaddrinfo gai_strerror getipnodebyname)
AC_CHECK_FUNCS(inet_aton inet_ntoa inet_pton inet_ntop)
AC_FUNC_SELECT_ARGTYPES

dnl Checks for system services.
AC_C_BIGENDIAN()

dnl Build options.
AM_CONDITIONAL(CODATUNNEL, test x$with_libuv != xno)
AM_CONDITIONAL(LUA, test x$include_lua = xtrue)

dnl Generate output files
AC_CONFIG_FILES([Makefile include/Makefile secure/Makefile rpc2-src/Makefile
    rp2gen/Makefile codatunnel/Makefile scripts/Makefile rpc2.pc rpc2-uninstalled.pc])
AC_OUTPUT

