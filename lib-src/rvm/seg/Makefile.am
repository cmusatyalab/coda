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

lib_LTLIBRARIES = libseg.la
INCLUDES = -I$(top_srcdir)/include
libseg_la_SOURCES = rvm_segutil.c rvm_loadseg.c rvm_createseg.c \
    rvm_releaseseg.c rvm_segment_private.h
libseg_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)

MAINTAINERCLEANFILES = Makefile.in

