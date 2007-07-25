## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libcodadir.la
if BUILD_TESTS
noinst_PROGRAMS = dirtest
endif

libcodadir_la_SOURCES = fid.c codadir.c codadir.h dirbody.c dirbody.h \
			dirinode.c dhcache.c
dirtest_SOURCES = dirtest.c

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep

LDADD = libcodadir.la \
	$(top_builddir)/coda-src/util/libutil.la \
	$(top_builddir)/lib-src/base/libbase.la \
	$(LIBRVM) $(LIBLWP) $(LIBREADLINE) $(LIBTERMCAP)

