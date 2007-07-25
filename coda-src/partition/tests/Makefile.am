## Process this file with automake to produce Makefile.in

if BUILD_BROKEN
noinst_PROGRAMS = basic setupvt createmany deletemany scaninodes
endif

EXTRA_DIST = vicetab

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/partition

LDADD = $(top_builddir)/coda-src/partition/libpartition.la \
	$(top_builddir)/coda-src/util/libutil.la \
	$(LIBLWP)

