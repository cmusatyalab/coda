## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libauser.la
dist_man_MANS =
if BUILD_CLIENT
bin_PROGRAMS = au clog cpasswd ctokens cunlog au
dist_man_MANS += au.1 clog.1 cpasswd.1 ctokens.1 cunlog.1
endif
if BUILD_SERVER
noinst_LTLIBRARIES += libauth2.la
sbin_PROGRAMS = auth2 initpw tokentool
dist_man_MANS += auth2.8 initpw.8 passwd.coda.5
endif

RPC2_FILES = auth2.rpc2

KRBSRC =
LIBKRB =
if WANT_KERBEROS
if WANT_KRB4
KRBSRC += krb4.c
LIBKRB += $(LIBKRB4)
endif
if WANT_KRB5
KRBSRC += krb5.c
LIBKRB += $(LIBKRB5)
endif
KRBSRC += krbcommon.c krbcommon.h
endif

noinst_HEADERS		   = auth2.common.h pwdefines.h
nodist_libauser_la_SOURCES = auth2.client.c
libauser_la_SOURCES	   = auser.c auser.h avenus.c avenus.h $(KRBSRC)
nodist_libauth2_la_SOURCES = auth2.server.c
libauth2_la_SOURCES	   = avice.c avice.h codatoken.c codatoken.h \
			     pwsupport.c pwsupport.h $(KRBSRC)
clog_SOURCES		   = clog.c tokenfile.c tokenfile.h
tokentool_SOURCES	   = tokentool.c tokenfile.c tokenfile.h
EXTRA_DIST = ${RPC2_FILES}

BUILT_SOURCES = $(nodist_libauser_la_SOURCES) $(nodist_libauth2_la_SOURCES)
CLEANFILES = ${RPC2_FILES:.rpc2=.h} ${RPC2_FILES:.rpc2=.client.c} \
	     ${RPC2_FILES:.rpc2=.server.c} ${RPC2_FILES:.rpc2=.multi.c} \
	     ${RPC2_FILES:.rpc2=.print.c}

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/al

libauser_la_LIBADD = $(LIBKRB)
libauth2_la_LIBADD = $(LIBKRB)

LIBBASE = $(top_builddir)/lib-src/base/libbase.la $(LIBRPC2) $(LIBLWP)
LIBAUSER = libauser.la $(top_builddir)/coda-src/kerndep/libkerndep.la $(LIBBASE)
LIBAUTH2 = libauth2.la \
	   $(top_builddir)/coda-src/al/libal.la \
	   $(top_builddir)/coda-src/util/libutil.la \
	   $(top_builddir)/lib-src/rwcdb/librwcdb.la \
	   $(LIBBASE)

LDADD = $(LIBAUSER)
tokentool_LDADD = $(LIBAUTH2)
auth2_LDADD = $(LIBAUTH2)
initpw_LDADD = $(LIBBASE)

SUFFIXES = .rpc2 .client.c .server.c .multi.c
.rpc2.client.c .rpc2.server.c .rpc2.multi.c .rpc2.print.c .rpc2.h:
	$(RP2GEN) $<

