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

INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include
LDADD = libfail.la $(top_builddir)/rpc2-src/librpc2.la \
    $(top_builddir)/rpc2-src/libse.la $(LIBLWP) $(LIBREADLINE) $(LIBTERMCAP)

pkginclude_HEADERS = fail.h 
lib_LTLIBRARIES = libfail.la
bin_PROGRAMS = filcon

libfail_la_SOURCES = filtutil.c fcon.c fail.c delay.c clear.c isolate.c \
    list.c join.c slow.c heal.c partition.c newpartition.c \
    filtutil.h delay.h fail.h ports.h
libfail_la_LIBADD = fcon.client.lo fcon.server.lo
libfail_la_LDFLAGS = -version-info 1:0:0

filcon_SOURCES = filcon.c parser.c parser.h

EXTRA_DIST = fcon.rpc2 newheal.c ft.rpc2 ftclient.c ftserver.c tv.rpc2 \
    tvclient.c tvserver.c

CLEANFILES = fcon.h *.client.c *.server.c *.multi.c *.print.c
MAINTAINERCLEANFILES = Makefile.in

SUFFIXES = .rpc2 .client.c .server.c .multi.c

%.client.c %.server.c %.h: %.rpc2
	$(top_builddir)/rp2gen/rp2gen $< && \
            ${CP} fcon.h $(top_builddir)/include/rpc2
        
# spurious dependency
filtutil.lo: fcon.h

