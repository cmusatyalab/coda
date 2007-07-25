## Process this file with automake to produce Makefile.in

if BUILD_SERVER
noinst_LTLIBRARIES = libvol.la
dist_man_MANS = vrdb.5
endif

libvol_la_SOURCES = dirvnode.cc cvnode.cc cvnode.h volume.cc volume.h vutil.cc \
    vutil.h vldb.cc vldb.h fssync.cc fssync.h index.cc index.h recov.h \
    recova.cc recovb.cc recovc.cc volhash.cc volhash.h dumpcamstorage.cc \
    camprivate.h vrdb.cc vrdb.h vlist.cc vlist.h lockqueue.cc lockqueue.h \
    vollocate.cc vollocate.h vol-debug.cc vicelock.h treeremove.h \
    coda_globals.h voldefs.h volres.h struct.h
EXTRA_DIST = testvrdb.cc

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
	      -I$(top_srcdir)/coda-src/lka

