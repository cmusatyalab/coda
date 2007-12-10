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
INCLUDES = -I$(top_srcdir)/include $(LWP_CFLAGS)

librds_la_SOURCES = rds_coalesce.c rds_free.c rds_init.c rds_malloc.c \
    rds_prealloc.c rds_split.c rds_stats.c rds_start.c rds_util.c rds_zap.c \
    rds_maxblock.c rds_private.h
librds_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)

librdslwp_la_SOURCES = rds_coalesce_lwp.c rds_free_lwp.c rds_init_lwp.c \
    rds_malloc_lwp.c rds_prealloc_lwp.c rds_split_lwp.c rds_stats_lwp.c \
    rds_start_lwp.c rds_util_lwp.c rds_zap_lwp.c rds_maxblock_lwp.c
librdslwp_la_LIBADD = $(LWP_LIBS)
librdslwp_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)

sbin_PROGRAMS = rdsinit
rdsinit_LDADD = librds.la $(top_builddir)/seg/libseg.la $(top_builddir)/rvm/librvm.la

MAINTAINERCLEANFILES = Makefile.in

