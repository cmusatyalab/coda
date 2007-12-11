## Process this file with automake to produce Makefile.in

# BLURB lgpl
# 
#                            Coda File System
#                               Release 5
# 
#           Copyright (c) 1987-1999 Carnegie Mellon University
#                   Additional copyrights listed below
# 
# This  code  is  distributed "AS IS" without warranty of any kind under
# the  terms of the  GNU  Library General Public Licence  Version 2,  as
# shown in the file LICENSE. The technical and financial contributors to
# Coda are listed in the file CREDITS.
# 
#                         Additional copyrights
#                            none currently
# 
#*/

lib_LTLIBRARIES = librds.la librdslwp.la
sbin_PROGRAMS = rdsinit

INCLUDES = -I$(top_srcdir)/include $(LWP_CFLAGS)
LDADD = librdslwp.la

librds_sources = rds_coalesce.c rds_free.c rds_init.c rds_malloc.c \
    rds_prealloc.c rds_split.c rds_stats.c rds_start.c rds_util.c rds_zap.c \
    rds_maxblock.c rds_private.h

librds_la_CFLAGS = $(AM_CFLAGS)
librds_la_SOURCES = $(librds_sources)
librds_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)
librds_la_LIBADD = $(top_builddir)/seg/libseg.la $(top_builddir)/rvm/librvm.la

librdslwp_la_CPPFLAGS = -DRVM_USELWP
librdslwp_la_SOURCES = $(librds_sources)
librdslwp_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)
librdslwp_la_LIBADD = $(top_builddir)/seg/libseg.la \
		      $(top_builddir)/rvm/librvmlwp.la $(LWP_LIBS)

MAINTAINERCLEANFILES = Makefile.in

