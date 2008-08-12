## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = basic setupvt createmany deletemany scaninodes

EXTRA_DIST = vicetab

AM_CPPFLAGS = $(LWP_CFLAGS) \
	      -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/partition

LDADD = $(top_builddir)/coda-src/partition/libpartition.la \
	$(top_builddir)/coda-src/util/libutil.la \
	$(LWP_LIBS)

