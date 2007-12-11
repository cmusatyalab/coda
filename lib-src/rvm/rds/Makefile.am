## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = librdslwp.la
if want_librvm
lib_LTLIBRARIES += librds.la
endif
sbin_PROGRAMS = rdsinit

AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = librdslwp.la

librds_sources = rds_coalesce.c rds_free.c rds_init.c rds_malloc.c \
    rds_prealloc.c rds_split.c rds_stats.c rds_start.c rds_util.c rds_zap.c \
    rds_maxblock.c rds_private.h

librds_la_CPPFLAGS = $(AM_CPPFLAGS)
librds_la_SOURCES = $(librds_sources)
librds_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)
librds_la_LIBADD = $(top_builddir)/seg/libseg.la $(top_builddir)/rvm/librvm.la

librdslwp_la_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USELWP $(LWP_CFLAGS)
librdslwp_la_SOURCES = $(librds_sources)
librdslwp_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)
librdslwp_la_LIBADD = $(top_builddir)/seg/libseg.la \
		      $(top_builddir)/rvm/librvmlwp.la $(LWP_LIBS)

MAINTAINERCLEANFILES = Makefile.in

