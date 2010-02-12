## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES =
sbin_PROGRAMS = rdsinit
dist_man_MANS = rdsinit.1

AM_CPPFLAGS = -I$(top_srcdir)/include

if LIBRVM
lib_LTLIBRARIES += librds.la
rdsinit_CPPFLAGS = $(AM_CPPFLAGS)
rdsinit_LDADD = librds.la $(top_builddir)/seg/libseg.la $(top_builddir)/rvm/librvm.la
endif
if LIBRVMLWP
lib_LTLIBRARIES += librdslwp.la
rdsinit_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USELWP $(LWP_CFLAGS)
rdsinit_LDADD = librdslwp.la $(top_builddir)/seg/libseglwp.la $(top_builddir)/rvm/librvmlwp.la $(LWP_LIBS)
endif
if LIBRVMPT
lib_LTLIBRARIES += librdspt.la
rdsinit_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USEPT -pthread
rdsinit_LDADD = librdspt.la $(top_builddir)/seg/libsegpt.la $(top_builddir)/rvm/librvmpt.la -lpthread
endif

librds_sources = rds_coalesce.c rds_free.c rds_init.c rds_malloc.c \
    rds_prealloc.c rds_split.c rds_stats.c rds_start.c rds_util.c rds_zap.c \
    rds_maxblock.c rds_private.h

librds_la_CPPFLAGS = $(AM_CPPFLAGS)
librds_la_SOURCES = $(librds_sources)
librds_la_LIBADD = $(top_builddir)/seg/libseg.la $(top_builddir)/rvm/librvm.la

librdslwp_la_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USELWP $(LWP_CFLAGS)
librdslwp_la_SOURCES = $(librds_sources)
librdslwp_la_LIBADD = $(top_builddir)/seg/libseglwp.la $(top_builddir)/rvm/librvmlwp.la $(LWP_LIBS)

librdspt_la_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USEPT -pthread
librdspt_la_SOURCES = $(librds_sources)
librdspt_la_LIBADD = $(top_builddir)/seg/libsegpt.la $(top_builddir)/rvm/librvmpt.la -lpthread

MAINTAINERCLEANFILES = Makefile.in

