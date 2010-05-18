dnl Process this file with autoconf to produce a configure script.

dnl autoconf 2.5 stuff
AC_PREREQ(2.50)
AC_INIT(Coda, 6.9.5, [bugs@coda.cs.cmu.edu])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([coda-src/librepair/resolve.h])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

CONFIG_DATE=`date +"%a, %d %b %Y %T %z"`
AC_SUBST(CONFIG_DATE, "$CONFIG_DATE", [Date when configure was last run])

dnl For some reason the default prefix was set to NONE on Solaris.
AC_PREFIX_DEFAULT([/usr/local])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CXX

dnl Load the CODA_ tests.
m4_include(configs/coda_macros.m4)

AC_PROG_CPP
AC_PROG_CXXCPP
AC_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_AWK

dnl Now we can derive things like systype/cputype
m4_include(configs/codaconf.m4)

AC_PATH_PROG(PERL, perl, "")
AC_PATH_PROG(FLUID, fluid, "")
AM_PATH_PYTHON(,,[:])

AC_ARG_ENABLE(client, [  --enable-client	  build Coda client components],
    buildclient="$enableval", buildclient="no")
AC_ARG_ENABLE(server, [  --enable-server	  build Coda server components],
    buildserver="$enableval", buildserver="no")
AC_ARG_ENABLE(venus, [  --enable-client-only	  build only Coda client],
    buildvenus="$enableval", buildvenus="no")
AC_ARG_ENABLE(vcodacon, [  --enable-vcodacon	  build graphical Coda console],
    buildvcodacon="$enableval", buildvcodacon="no")

dnl Make sure we configure any sub-projects before checking with pkg-config
test -f "${srcdir}/lib-src/lwp/configure.in" && AC_CONFIG_SUBDIRS([lib-src/lwp])
test -f "${srcdir}/lib-src/rpc2/configure.in" && AC_CONFIG_SUBDIRS([lib-src/rpc2])
test -f "${srcdir}/lib-src/rvm/configure.in" && AC_CONFIG_SUBDIRS([lib-src/rvm])
CODA_CONFIG_SUBDIRS

dnl pkg-config stuff
PKG_CHECK_MODULES([RVM_RPC2], [rvmlwp rpc2])
PKG_CHECK_MODULES([LWP], [lwp >= 2.5])
PKG_CHECK_MODULES([RPC2], [rpc2 >= 2.8])
PKG_CHECK_MODULES([RVM], [rvmlwp])

RP2GEN="`pkg-config --variable=RP2GEN rpc2`"
AC_SUBST(RP2GEN)

RVMUTL="`pkg-config --variable=RVMUTL rvmlwp`"
RDSINIT="`pkg-config --variable=RDSINIT rvmlwp`"
AC_SUBST(RVMUTL)
AC_SUBST(RDSINIT)

dnl Checks for libraries.
AC_SEARCH_LIBS(bind, socket)
AC_SEARCH_LIBS(inet_ntoa, nsl)
AC_SEARCH_LIBS(gethostbyname, resolv)
AC_SEARCH_LIBS(res_search, resolv)
AC_SEARCH_LIBS(res_9_init, resolv)
AC_SEARCH_LIBS(kvm_openfiles, kvm)

CODA_CHECK_LIBTERMCAP
CODA_CHECK_LIBCURSES
CODA_CHECK_READLINE
CODA_CHECK_FLTK

CODA_CHECK_KRB4
CODA_CHECK_KRB5

dnl Checks for header files.
AC_CHECK_HEADERS(search.h sys/resource.h sys/stream.h ncurses.h netdb.h)
AC_CHECK_HEADERS(sys/statvfs.h sys/statfs.h sys/param.h sys/vfs.h fts.h)
AC_CHECK_HEADERS(sys/types.h sys/time.h sys/select.h sys/socket.h sys/ioccom.h)
AC_CHECK_HEADERS(arpa/inet.h arpa/nameser.h netinet/in.h osreldate.h)
AC_CHECK_HEADERS(ncurses/ncurses.h byteswap.h sys/bswap.h sys/endian.h)
AC_CHECK_HEADERS(ucred.h execinfo.h)

AC_CHECK_HEADERS(sys/un.h resolv.h, [], [],
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif])
AC_CHECK_HEADERS(sys/mount.h, [], [],
[#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif])

dnl Checks for typedefs.
AC_TYPE_INT32_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_CHECK_TYPES([socklen_t, struct in6_addr, struct sockaddr_in6],,,
	       [#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>])
AC_CHECK_TYPES([ns_type, ns_class],,, [#include <arpa/nameser.h>])

dnl Checks for structures.

dnl Checks for compiler characteristics.
CODA_CC_FEATURE_TEST([Wall], wall)
CODA_CC_FEATURE_TEST([fno-exceptions], fnoexceptions)
CODA_CC_FEATURE_TEST([rdynamic], rdynamic)
CODA_CXX_FEATURE_TEST([Wall], wall)
CODA_CXX_FEATURE_TEST([fno-exceptions], fnoexceptions)
CODA_CXX_FEATURE_TEST([fcheck-new], fchecknew)
CODA_CXX_FEATURE_TEST([rdynamic], rdynamic)
CODA_CHECK_OFFSETOF

dnl Checks for library functions.
AC_CHECK_FUNCS(inet_aton inet_ntoa res_search pread fseeko nmount)
AC_CHECK_FUNCS(select setenv snprintf statfs strerror strtol)
AC_CHECK_FUNCS(getpeereid getpeerucred backtrace)
AC_FUNC_MMAP
AC_FUNC_SELECT_ARGTYPES
CODA_CHECK_FILE_LOCKING

dnl Checks for system services.

AC_SUBST(shortsys)

dnl Build conditionals
if test "$buildvcodacon" != no ; then
  test -z "${FLUID}" && AC_MSG_ERROR("Unable to find fltk-fluid")
  test "$coda_cv_path_fltk" == none && AC_MSG_ERROR("Unable to find libfltk")
fi
AM_CONDITIONAL(BUILD_VCODACON, [test "$buildvcodacon" != no])

if test "$buildclient" = no -a "$buildserver" = no -a "$buildvenus" = no ; then
  buildclient=yes
  buildserver=yes
fi
AM_CONDITIONAL(BUILD_CLIENT, [test "$buildclient" != no])
AM_CONDITIONAL(BUILD_SERVER, [test "$buildserver" != no])
AM_CONDITIONAL(BUILD_VENUS, [test "$buildclient" != no -o "$buildvenus" != no])

AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != :])
AM_CONDITIONAL([WANT_FTS], [test "$ac_cv_header_fts_h" = no])
AM_CONDITIONAL([WANT_KERBEROS], [test -n "$LIBKRB4" -o -n "$LIBKRB5"])
AM_CONDITIONAL([WANT_KRB4], [test -n "$LIBKRB4"])
AM_CONDITIONAL([WANT_KRB5], [test -n "$LIBKRB5"])

AC_CONFIG_FILES([coda-src/scripts/bldvldb.sh],
       [chmod +x coda-src/scripts/bldvldb.sh])
AC_CONFIG_FILES([coda-src/scripts/createvol_rep],
       [chmod +x coda-src/scripts/createvol_rep])
AC_CONFIG_FILES([coda-src/scripts/purgevol_rep],
       [chmod +x coda-src/scripts/purgevol_rep])
AC_CONFIG_FILES([coda-src/scripts/startserver],
       [chmod +x coda-src/scripts/startserver])
AC_CONFIG_FILES([coda-src/scripts/venus-setup],
       [chmod +x coda-src/scripts/venus-setup])
AC_CONFIG_FILES([coda-src/scripts/vice-setup],
       [chmod +x coda-src/scripts/vice-setup])
AC_CONFIG_FILES([coda-src/scripts/vice-setup-rvm],
       [chmod +x coda-src/scripts/vice-setup-rvm])
AC_CONFIG_FILES([coda-src/scripts/vice-setup-scm],
       [chmod +x coda-src/scripts/vice-setup-scm])
AC_CONFIG_FILES([coda-src/scripts/vice-setup-srvdir],
       [chmod +x coda-src/scripts/vice-setup-srvdir])
AC_CONFIG_FILES([coda-src/scripts/vice-setup-user],
       [chmod +x coda-src/scripts/vice-setup-user])
AC_CONFIG_FILES([coda-src/vtools/gcodacon],
       [chmod +x coda-src/vtools/gcodacon])
AC_CONFIG_FILES([
Makefile
coda_config.h
configs/Makefile
debian/Makefile
debian/changelog
tools/Makefile
tools/coda.spec
lib-src/Makefile
lib-src/base/Makefile
lib-src/rwcdb/Makefile
coda-src/Makefile
coda-src/scripts/Makefile
coda-src/kerndep/Makefile
coda-src/partition/Makefile
coda-src/dir/Makefile
coda-src/util/Makefile
coda-src/al/Makefile
coda-src/vicedep/Makefile
coda-src/auth2/Makefile
coda-src/vv/Makefile
coda-src/lka/Makefile
coda-src/vol/Makefile
coda-src/librepair/Makefile
coda-src/venus/Makefile
coda-src/repair/Makefile
coda-src/resolution/Makefile
coda-src/volutil/Makefile
coda-src/vtools/Makefile
coda-src/vice/Makefile
coda-src/update/Makefile
coda-src/norton/Makefile
coda-src/asr/Makefile
coda-src/egasr/Makefile
coda-src/asrlauncher/Makefile
coda-src/vcodacon/Makefile
coda-src/smon2/Makefile])
AC_OUTPUT

