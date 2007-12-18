dnl Process this file with autoconf to produce a configure script.

dnl autoconf 2.5 stuff
AC_PREREQ(2.50)
AC_INIT(Coda, 6.9.3~rc1, [bugs@coda.cs.cmu.edu])
AC_CONFIG_SRCDIR(coda-src/librepair/resolve.h)
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS(config.h)

CONFIG_DATE=`date +"%a, %d %b %Y %T %z"`
AC_SUBST(CONFIG_DATE, "$CONFIG_DATE", [Date when configure was last run])

dnl For some reason the default prefix was set to NONE on Solaris.
AC_PREFIX_DEFAULT(/usr/local)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CXX

dnl Load the CODA_ tests.
m4_include(configs/coda_macros.m4)

CODA_OPTION_SUBSYS(lwp)
CODA_OPTION_SUBSYS(rvm)
CODA_OPTION_SUBSYS(rpc2)
CODA_OPTION_LWP_PT

CODA_PROG_NATIVECC

AC_PROG_CPP
AC_PROG_CXXCPP
AC_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_AWK

dnl Now we can derive things like systype/cputype/initdir
m4_include(configs/codaconf.m4)

AC_PATH_PROG(PERL, perl, "")
AC_PATH_PROG(FLUID, fluid, "")
AM_PATH_PYTHON(,,[:])

AC_PATH_PROG(RP2GEN, rp2gen, "", [${PATH}:${prefix}/sbin:/usr/sbin:/usr/local/sbin:/usr/pkg/sbin])
AC_PATH_PROG(RDSINIT, rdsinit, "", [${PATH}:${prefix}/sbin:/usr/sbin:/usr/local/sbin:/usr/pkg/sbin])
AC_PATH_PROG(RVMUTL, rvmutl, "", [${PATH}:${prefix}/sbin:/usr/sbin:/usr/local/sbin:/usr/pkg/sbin])

AC_ARG_ENABLE(client, [  --enable-client	  build Coda client components],
    buildclient="$enableval", buildclient="no")
AC_ARG_ENABLE(server, [  --enable-server	  build Coda server components],
    buildserver="$enableval", buildserver="no")
AC_ARG_WITH(vcodacon, [  --with-vcodacon	  build graphical Coda console],
    buildvcodacon="$enableval", buildvcodacon="no")

CODA_FAIL_IF_MISSING(RP2GEN, "unable to find rp2gen. may need librpc2-dev");
CODA_FAIL_IF_MISSING(RDSINIT, "unable to find rdsinit. may need rvm-tools");
CODA_FAIL_IF_MISSING(RVMUTL, "unable to find rvmutl. may need rvm-tools");

dnl Checks for libraries.
AC_SEARCH_LIBS(bind, socket)
AC_SEARCH_LIBS(inet_ntoa, nsl)
AC_SEARCH_LIBS(gethostbyname, resolv)
AC_SEARCH_LIBS(res_search, resolv)
AC_SEARCH_LIBS(res_9_init, resolv)
AC_SEARCH_LIBS(kvm_openfiles, kvm)

CODA_FIND_LIB(lwp, [#include <lwp/lwp.h>], [LWP_Init(0,0,0)])
CODA_FIND_LIB(rvmlwp, [#include <rvm/rvm.h>], [rvm_initialize(0,0)])
CODA_FIND_LIB(rpc2, [#include <rpc2/rpc2.h>], [RPC2_Init(0,0,0,0,0)], -llwp)
CODA_CHECK_LWP_PT

CODA_REQUIRE_LIB(lwp)
CODA_REQUIRE_LIB(rvmlwp)
CODA_REQUIRE_LIB(rpc2)

AC_SUBST(LIBRVM)
AC_SUBST(LIBRPC2)
LIBRVM="-lrdslwp -lseglwp -lrvmlwp"
LIBRPC2="-lrpc2 -lse"

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
AC_CHECK_HEADERS(arpa/inet.h arpa/nameser.h netinet/in.h sys/un.h osreldate.h)
AC_CHECK_HEADERS(ncurses/ncurses.h byteswap.h sys/bswap.h sys/endian.h)
AC_CHECK_HEADERS(ucred.h)

AC_CHECK_HEADERS(resolv.h, [], [],
[#ifdef HAVE_NETINET_IN_H
#include <sys/types.h>
#include <netinet/in.h>
#endif
])
AC_CHECK_HEADERS(sys/mount.h, [], [],
[#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
])

dnl Checks for typedefs.
AC_CHECK_TYPE(int32_t, int)
AC_CHECK_TYPE(u_int32_t, unsigned int)
CODA_CHECK_TYPE(socklen_t, int, sys/socket.h)
AC_CHECK_TYPES([struct in6_addr, struct sockaddr_in6],,,
	       [#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>])

dnl Checks for structures.

dnl Checks for compiler characteristics.
CODA_CC_FEATURE_TEST(Wall, -Wall)
CODA_CC_FEATURE_TEST(no_exceptions, -fno-exceptions)
CODA_CXX_FEATURE_TEST(Wall, -Wall)
CODA_CXX_FEATURE_TEST(no_exceptions, -fno-exceptions)
CODA_CXX_FEATURE_TEST(check_new, -fcheck-new)
CODA_CHECK_OFFSETOF

dnl Checks for library functions.
AC_CHECK_FUNCS(inet_aton inet_ntoa res_search pread fseeko nmount)
AC_CHECK_FUNCS(select setenv snprintf statfs strerror strtol)
AC_CHECK_FUNCS(getpeereid getpeerucred)
AC_FUNC_MMAP
AC_FUNC_SELECT_ARGTYPES
CODA_CHECK_FILE_LOCKING

dnl Checks for system services.

AC_SUBST(shortsys)

dnl Build conditionals
if test "$buildvcodacon" != no ; then
  CODA_FAIL_IF_MISSING(FLUID, "Unable to find fltk-fluid");
  test "$coda_cv_path_fltk" == none && AC_MSG_ERROR("Unable to find libfltk")
fi
AM_CONDITIONAL(BUILD_VCODACON, [test "$buildvcodacon" != no])

AM_CONDITIONAL(BUILD_CLIENT, [test "$buildclient" != no -o "$buildclient" = no -a "$buildserver" = no])
AM_CONDITIONAL(BUILD_SERVER, [test "$buildserver" != no -o "$buildclient" = no -a "$buildserver" = no])

AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != :])
AM_CONDITIONAL([SYSVINIT], [test "$initstyle" = sysv])
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
AC_CONFIG_FILES([coda-src/scripts/rc.vice],
       [chmod +x coda-src/scripts/rc.vice])
AC_CONFIG_FILES([coda-src/scripts/startserver],
       [chmod +x coda-src/scripts/startserver])
AC_CONFIG_FILES([coda-src/scripts/venus.init],
       [chmod +x coda-src/scripts/venus.init])
AC_CONFIG_FILES([coda-src/scripts/venus-setup],
       [chmod +x coda-src/scripts/venus-setup])
AC_CONFIG_FILES([coda-src/scripts/vice-setup],
       [chmod +x coda-src/scripts/vice-setup])
AC_CONFIG_FILES([coda-src/scripts/vice-setup-rvm],
       [chmod +x coda-src/scripts/vice-setup-rvm])
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

