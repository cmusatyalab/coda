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

INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
    -DRPC2DEBUG -DERRORTR -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
LDADD = librpc2.la libse.la $(LIBLWP)

pkginclude_HEADERS = sftp.h
noinst_HEADERS = rpc2.private.h test.h
lib_LTLIBRARIES = librpc2.la libse.la
noinst_PROGRAMS = ctest stest rpc2test
EXTRA_DIST = makeinc.pl errordb.txt fxor.s

librpc2_la_SOURCES = rpc2a.c rpc2b.c sl.c packet.c secure.c globals.c se.c \
    lists.c ct.c debug.c conn.c cbuf.c ffs.c host.c multi1.c multi2.c \
    multi3.c mkcall.c stats.c errors.c cbuf.h trace.h dllist.c rpc2_addrinfo.c \
    compat.c
librpc2_la_LDFLAGS = -version-info $(LIBTOOL_VERSION)

libse_la_SOURCES = sftp1.c sftp2.c sftp3.c sftp4.c sftp5.c sftp6.c
libse_la_LDFLAGS = -version-info $(LIBTOOL_VERSION)

CLEANFILES = switchc2s.h switchs2c.h errorsdefs.h
MAINTAINERCLEANFILES = Makefile.in

SUFFIXES = .txt

switchc2s.h: $(srcdir)/errordb.txt
	${PERL} $(srcdir)/makeinc.pl < $(srcdir)/errordb.txt && \
	${CP} errorsdefs.h $(top_builddir)/include/rpc2

errors.lo: switchc2s.h

