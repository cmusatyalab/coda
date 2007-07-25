## Process this file with automake to produce Makefile.in

if BUILD_SERVER
noinst_LTLIBRARIES = libal.la
sbin_PROGRAMS = pdbtool
dist_man_MANS = pdbtool.8
if BUILD_TESTS
noinst_PROGRAMS = altest
endif
endif

noinst_HEADERS = al.h prs.h pdb.h pdbarray.h
libal_la_SOURCES = pdbdb.c pdbpack.c alprocs.c pdb.c pdbdb.c pdbpack.c \
		   pdbprofile.c pdbarray.c
pdbtool_SOURCES = pdbtool.c
altest_SOURCES = altest.c

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/lib-src/rwcdb \
	      -I$(top_srcdir)/coda-src/util

LDADD = libal.la \
	$(top_builddir)/coda-src/util/libutil.la \
	$(top_builddir)/lib-src/rwcdb/librwcdb.la \
	$(top_builddir)/lib-src/base/libbase.la

pdbtool_LDADD = $(LDADD) $(LIBREADLINE) $(LIBTERMCAP)
altest_LDADD  = $(LDADD)

