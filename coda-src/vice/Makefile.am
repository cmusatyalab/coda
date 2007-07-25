## Process this file with automake to produce Makefile.in

if BUILD_SERVER
noinst_LTLIBRARIES = libviceerror.la
sbin_PROGRAMS = codasrv printvrdb
dist_man_MANS = codasrv.8 servers.5
dist_sysconf_DATA = server.conf.ex
endif

libviceerror_la_SOURCES = ViceErrorMsg.c
codasrv_SOURCES = srv.cc srvproc.cc srvproc2.cc coppend.cc coppend.h \
		  codaproc.cc codaproc.h codaproc2.cc clientproc.cc vicecb.cc \
		  smon.cc timecalls.h vice.private.h
printvrdb_SOURCES = printvrdb.cc

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/dir \
	      -I$(top_srcdir)/coda-src/al \
	      -I$(top_srcdir)/coda-src/partition \
	      -I$(top_srcdir)/coda-src/auth2 \
	      -I$(top_builddir)/coda-src/auth2 \
	      -I$(top_srcdir)/coda-src/vv \
	      -I$(top_srcdir)/coda-src/lka \
	      -I$(top_srcdir)/coda-src/vol \
	      -I$(top_srcdir)/coda-src/librepair \
	      -I$(top_srcdir)/coda-src/resolution

codasrv_LDADD = libviceerror.la \
		$(top_builddir)/coda-src/vicedep/libvicedep.la \
		$(top_builddir)/coda-src/volutil/libvolutil.la \
		$(top_builddir)/coda-src/volutil/libvolserv.la \
		$(top_builddir)/coda-src/vicedep/voldump.client.lo \
		$(top_builddir)/coda-src/vicedep/volutil.server.lo \
		$(top_builddir)/coda-src/resolution/libres.la \
		$(top_builddir)/coda-src/librepair/librepio.la \
		$(top_builddir)/coda-src/vol/libvol.la \
		$(top_builddir)/coda-src/lka/liblka.la \
		$(top_builddir)/coda-src/vv/libvv.la \
		$(top_builddir)/coda-src/auth2/libauth2.la \
		$(top_builddir)/coda-src/partition/libpartition.la \
		$(top_builddir)/coda-src/al/libal.la \
		$(top_builddir)/coda-src/dir/libcodadir.la \
		$(top_builddir)/coda-src/util/libutil.la \
		$(top_builddir)/lib-src/rwcdb/librwcdb.la \
		$(top_builddir)/lib-src/base/libbase.la \
		$(LIBRVM) $(LIBRPC2) $(LIBLWP)

printvrdb_LDADD = $(top_builddir)/coda-src/util/libutil.la \
		  $(top_builddir)/lib-src/base/libbase.la

