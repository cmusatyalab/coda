## Process this file with automake to produce Makefile.in

if BUILD_SERVER
noinst_LTLIBRARIES = libres.la
endif

libres_la_SOURCES = resclient.cc rescoord.cc rescoord.h resforce.cc resforce.h \
    reslock.cc resutil.cc resutil.h rescomm.cc rescomm.h rescomm.private.h \
    timing.cc timing.h rvmrestiming.h weres.cc ops.cc ops.h recle.cc recle.h \
    recov_vollog.cc rsle.cc rsle.h rvmrescoord.cc compops.cc compops.h \
    parselog.cc parselog.h ruconflict.cc ruconflict.h subresphase2.cc \
    subresphase3.cc subresphase34.cc rename.cc subpreres.cc resstats.cc \
    resstats.h resfile.cc resolution.h

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
	      -I$(top_srcdir)/coda-src/vol

