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
    -DRPC2DEBUG -DERRORTR
LDADD = librpc2.la libse.la $(LIBLWP)

pkginclude_HEADERS = sftp.h rpc2.private.h
noinst_HEADERS = test.h
lib_LTLIBRARIES = librpc2.la libse.la
noinst_PROGRAMS = ctest stest rpc2test
EXTRA_DIST = makeinc.pl errordb.txt fxor.s

librpc2_la_SOURCES = rpc2a.c rpc2b.c sl.c packet.c secure.c globals.c se.c \
    lists.c ct.c debug.c conn.c cbuf.c ffs.c host.c multi1.c multi2.c \
    multi3.c mkcall.c stats.c errors.c cbuf.h trace.h \
    coda_assert.c dllist.c
librpc2_la_LDFLAGS = -version-info 1:0:0

libse_la_SOURCES = sftp1.c sftp2.c sftp3.c sftp4.c sftp5.c sftp6.c
libse_la_LDFLAGS = -version-info 1:0:0

CLEANFILES = switchc2s.h switchs2c.h errorsdefs.h
MAINTAINERCLEANFILES = Makefile.in

SUFFIXES = .txt

errors.c: switchc2s.h switchs2c.h errorsdefs.h
switchc2s.h switchs2c.h errorsdefs.h: errordb.txt
	${PERL} ${srcdir}/makeinc.pl < ${srcdir}/errordb.txt && \
	${CP} errorsdefs.h $(top_srcdir)/include/rpc2

