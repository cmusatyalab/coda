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
librvm_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)

librvmlwp_la_SOURCES = rvm_init_lwp.c rvm_map_lwp.c rvm_unmap_lwp.c \
    rvm_trans_lwp.c rvm_logstatus_lwp.c rvm_logflush_lwp.c \
    rvm_logrecovr_lwp.c rvm_utils_lwp.c rvm_io_lwp.c rvm_status_lwp.c \
    rvm_debug_lwp.c rvm_printers_lwp.c
librvmlwp_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS) $(LIBLWP)

sbin_PROGRAMS = rvmutl
rvmutl_LDADD = librvm.la

EXTRA_DIST = rvm_pthread.c README.log1.4 READ_ME
MAINTAINERCLEANFILES = Makefile.in

