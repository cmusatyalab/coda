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

lib_LTLIBRARIES = librvm.la librvmlwp.la
INCLUDES = -I$(top_srcdir)/include
librvm_la_SOURCES = rvm_init.c rvm_map.c rvm_unmap.c rvm_trans.c \
    rvm_logstatus.c rvm_logflush.c rvm_logrecovr.c rvm_utils.c rvm_io.c \
    rvm_status.c rvm_debug.c rvm_printers.c rvm_private.h
librvm_la_LDFLAGS = -version-info 1:0:0

librvmlwp_la_SOURCES =
librvmlwp_la_LIBADD = rvm_init_lwp.lo rvm_map_lwp.lo rvm_unmap_lwp.lo \
    rvm_trans_lwp.lo rvm_logstatus_lwp.lo rvm_logflush_lwp.lo \
    rvm_logrecovr_lwp.lo rvm_utils_lwp.lo rvm_io_lwp.lo rvm_status_lwp.lo \
    rvm_debug_lwp.lo rvm_printers_lwp.lo
librvmlwp_la_LDFLAGS = -version-info 1:0:0

sbin_PROGRAMS = rvmutl
rvmutl_LDADD = librvm.la

EXTRA_DIST = rvm_pthread.c
MAINTAINERCLEANFILES = Makefile.in

rvm_init.lo: rvm_init.c rvm_init_lwp.lo
rvm_init_lwp.lo: rvm_init.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_init.lo rvm_init_lwp.lo && \
	mv rvm_init.o rvm_init_lwp.o

rvm_map.lo: rvm_map.c rvm_map_lwp.lo
rvm_map_lwp.lo: rvm_map.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_map.lo rvm_map_lwp.lo && \
	mv rvm_map.o rvm_map_lwp.o

rvm_unmap.lo: rvm_unmap.c rvm_unmap_lwp.lo
rvm_unmap_lwp.lo: rvm_unmap.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_unmap.lo rvm_unmap_lwp.lo && \
	mv rvm_unmap.o rvm_unmap_lwp.o

rvm_trans.lo: rvm_trans.c rvm_trans_lwp.lo
rvm_trans_lwp.lo: rvm_trans.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_trans.lo rvm_trans_lwp.lo && \
	mv rvm_trans.o rvm_trans_lwp.o

rvm_logstatus.lo: rvm_logstatus.c rvm_logstatus_lwp.lo
rvm_logstatus_lwp.lo: rvm_logstatus.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_logstatus.lo rvm_logstatus_lwp.lo && \
	mv rvm_logstatus.o rvm_logstatus_lwp.o

rvm_logflush.lo: rvm_logflush.c rvm_logflush_lwp.lo
rvm_logflush_lwp.lo: rvm_logflush.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_logflush.lo rvm_logflush_lwp.lo && \
	mv rvm_logflush.o rvm_logflush_lwp.o

rvm_logrecovr.lo: rvm_logrecovr.c rvm_logrecovr_lwp.lo
rvm_logrecovr_lwp.lo: rvm_logrecovr.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_logrecovr.lo rvm_logrecovr_lwp.lo && \
	mv rvm_logrecovr.o rvm_logrecovr_lwp.o

rvm_utils.lo: rvm_utils.c rvm_utils_lwp.lo
rvm_utils_lwp.lo: rvm_utils.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_utils.lo rvm_utils_lwp.lo && \
	mv rvm_utils.o rvm_utils_lwp.o

rvm_io.lo: rvm_io.c rvm_io_lwp.lo
rvm_io_lwp.lo: rvm_io.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_io.lo rvm_io_lwp.lo && \
	mv rvm_io.o rvm_io_lwp.o

rvm_status.lo: rvm_status.c rvm_status_lwp.lo
rvm_status_lwp.lo: rvm_status.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_status.lo rvm_status_lwp.lo && \
	mv rvm_status.o rvm_status_lwp.o

rvm_debug.lo: rvm_debug.c rvm_debug_lwp.lo
rvm_debug_lwp.lo: rvm_debug.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_debug.lo rvm_debug_lwp.lo && \
	mv rvm_debug.o rvm_debug_lwp.o

rvm_printers.lo: rvm_printers.c rvm_printers_lwp.lo
rvm_printers_lwp.lo: rvm_printers.c
	$(LTCOMPILE) -DRVM_USELWP @LWPINCLUDES@ -c $< && \
	mv rvm_printers.lo rvm_printers_lwp.lo && \
	mv rvm_printers.o rvm_printers_lwp.o

