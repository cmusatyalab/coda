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

lib_LTLIBRARIES = liblwp.la
INCLUDES = -I$(top_srcdir)/include
liblwp_la_SOURCES = fasttime.c iomgr.c lock.c lwp.c preempt.c timer.c \
    process.ss lwp.private.h
liblwp_la_LIBADD = process.lo
liblwp_la_CFLAGS = -DLWP_DEBUG
liblwp_la_LDFLAGS = -version-info 1:0:0 $(LIBTOOL_LDFLAGS)

noinst_PROGRAMS = cswitch test test-static tdb rw
LDADD = liblwp.la
test_static_SOURCES = test.c
test_static_LDFLAGS = -static

DISTCLEANFILES = process.s
MAINTAINERCLEANFILES = Makefile.in

SUFFIXES = .ss

.ss.s:
	$(CPP) -x c-header -P -D__ASSEMBLY__ $(AFLAGS) $< > $*.s

