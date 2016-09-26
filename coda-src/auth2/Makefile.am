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
include $(top_srcdir)/configs/rpc2_rules.mk

noinst_HEADERS		   = auth2.common.h pwdefines.h
libauser_la_SOURCES	   = auser.c auser.h avenus.c avenus.h
libauth2_la_SOURCES	   = avice.c avice.h codatoken.c codatoken.h \
			     pwsupport.c pwsupport.h
clog_SOURCES		   = clog.c tokenfile.c tokenfile.h
tokentool_SOURCES	   = tokentool.c tokenfile.c tokenfile.h

nodist_libauser_la_SOURCES = auth2.client.c auth2.helper.c auth2.h
nodist_libauth2_la_SOURCES = auth2.server.c auth2.helper.c auth2.h

AM_CPPFLAGS = $(RPC2_CFLAGS) \
	      -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/al


LIBBASE = $(top_builddir)/lib-src/base/libbase.la $(RPC2_LIBS)
LIBAUSER = libauser.la \
	   $(top_builddir)/coda-src/kerndep/libkerndep.la \
	   $(top_builddir)/coda-src/util/libutil.la \
	   $(LIBBASE)

LIBAUTH2 = libauth2.la \
	   $(top_builddir)/coda-src/al/libal.la \
	   $(top_builddir)/coda-src/util/libutil.la \
	   $(top_builddir)/lib-src/rwcdb/librwcdb.la \
	   $(LIBBASE)

LDADD = $(LIBAUSER)
tokentool_LDADD = $(LIBAUTH2)
auth2_LDADD = $(LIBAUTH2)
initpw_LDADD = $(LIBBASE)

