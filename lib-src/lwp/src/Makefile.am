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
liblwp_la_SOURCES = fasttime.c iomgr.c lock.c lwp.c timer.c process.S \
    lwp.private.h
liblwp_la_CFLAGS = -DLWP_DEBUG
liblwp_la_LDFLAGS = -version-info 1:0:0 $(LIBTOOL_LDFLAGS)

noinst_PROGRAMS = cswitch testlwp testlwp-static tdb rw
LDADD = liblwp.la
testlwp_static_SOURCES = testlwp.c
testlwp_static_LDFLAGS = -static

MAINTAINERCLEANFILES = Makefile.in

