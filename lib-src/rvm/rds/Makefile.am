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
INCLUDES = -I$(top_srcdir)/include

librds_la_SOURCES = rds_coalesce.c rds_free.c rds_init.c rds_malloc.c \
    rds_prealloc.c rds_split.c rds_stats.c rds_start.c rds_util.c rds_zap.c \
    rds_private.h
librds_la_LDFLAGS = -version-info 1:0:0 $(LIBTOOL_LDFLAGS)

librdslwp_la_SOURCES =
librdslwp_la_LIBADD = rds_coalesce.lo rds_free.lo rds_init.lo rds_malloc.lo \
    rds_prealloc.lo rds_split.lo rds_stats.lo rds_start.lo rds_util.lo \
    rds_zap.lo
librdslwp_la_LDFLAGS = -version-info 1:0:0 $(LIBTOOL_LDFLAGS) $(LIBLWP)

sbin_PROGRAMS = rdsinit
rdsinit_LDADD = librds.la $(top_builddir)/seg/libseg.la $(top_builddir)/rvm/librvm.la

MAINTAINERCLEANFILES = Makefile.in

rds_coalesce.lo: $(srcdir)/rds_coalesce.c rds_coalesce_lwp.lo
rds_coalesce_lwp.lo: $(srcdir)/rds_coalesce.c
	$(LTCOMPILE) -DRVM_USELWP -c  $(srcdir)/rds_coalesce.c && \
	mv rds_coalesce.lo rds_coalesce_lwp.lo && \
	mv rds_coalesce.o rds_coalesce_lwp.o

rds_free.lo:  $(srcdir)/rds_free.c rds_free_lwp.lo
rds_free_lwp.lo:  $(srcdir)/rds_free.c
	$(LTCOMPILE) -DRVM_USELWP -c  $(srcdir)/rds_free.c && \
	mv rds_free.lo rds_free_lwp.lo && \
	mv rds_free.o rds_free_lwp.o

rds_init.lo:  $(srcdir)/rds_init.c rds_init_lwp.lo
rds_init_lwp.lo:  $(srcdir)/rds_init.c
	$(LTCOMPILE) -DRVM_USELWP -c  $(srcdir)/rds_init.c && \
	mv rds_init.lo rds_init_lwp.lo && \
	mv rds_init.o rds_init_lwp.o

rds_malloc.lo:  $(srcdir)/rds_malloc.c rds_malloc_lwp.lo
rds_malloc_lwp.lo:  $(srcdir)/rds_malloc.c
	$(LTCOMPILE) -DRVM_USELWP -c  $(srcdir)/rds_malloc.c && \
	mv rds_malloc.lo rds_malloc_lwp.lo && \
	mv rds_malloc.o rds_malloc_lwp.o

rds_prealloc.lo:  $(srcdir)/rds_prealloc.c rds_prealloc_lwp.lo
rds_prealloc_lwp.lo:  $(srcdir)/rds_prealloc.c
	$(LTCOMPILE) -DRVM_USELWP -c  $(srcdir)/rds_prealloc.c && \
	mv rds_prealloc.lo rds_prealloc_lwp.lo && \
	mv rds_prealloc.o rds_prealloc_lwp.o

rds_split.lo:  $(srcdir)/rds_split.c rds_split_lwp.lo
rds_split_lwp.lo:  $(srcdir)/rds_split.c
	$(LTCOMPILE) -DRVM_USELWP -c  $(srcdir)/rds_split.c && \
	mv rds_split.lo rds_split_lwp.lo && \
	mv rds_split.o rds_split_lwp.o

rds_stats.lo:  $(srcdir)/rds_stats.c rds_stats_lwp.lo
rds_stats_lwp.lo:  $(srcdir)/rds_stats.c
	$(LTCOMPILE) -DRVM_USELWP -c  $(srcdir)/rds_stats.c && \
	mv rds_stats.lo rds_stats_lwp.lo && \
	mv rds_stats.o rds_stats_lwp.o

rds_start.lo:  $(srcdir)/rds_start.c rds_start_lwp.lo
rds_start_lwp.lo:  $(srcdir)/rds_start.c
	$(LTCOMPILE) -DRVM_USELWP -c  $(srcdir)/rds_start.c && \
	mv rds_start.lo rds_start_lwp.lo && \
	mv rds_start.o rds_start_lwp.o

rds_util.lo:  $(srcdir)/rds_util.c rds_util_lwp.lo
rds_util_lwp.lo:  $(srcdir)/rds_util.c
	$(LTCOMPILE) -DRVM_USELWP -c  $(srcdir)/rds_util.c && \
	mv rds_util.lo rds_util_lwp.lo && \
	mv rds_util.o rds_util_lwp.o

rds_zap.lo:  $(srcdir)/rds_zap.c rds_zap_lwp.lo
rds_zap_lwp.lo:  $(srcdir)/rds_zap.c
	$(LTCOMPILE) -DRVM_USELWP -c $(srcdir)/rds_zap.c && \
	mv rds_zap.lo rds_zap_lwp.lo && \
	mv rds_zap.o rds_zap_lwp.o

