## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include $(LUA_CFLAGS) \
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
    compat.c lua.c delay.c
librpc2_la_LIBADD = $(top_builddir)/secure/libsecure.la $(LIBLWP)
librpc2_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LUA_LIBS)

libse_la_SOURCES = sftp1.c sftp2.c sftp3.c sftp4.c sftp5.c sftp6.c
libse_la_LIBADD = librpc2.la
libse_la_LDFLAGS = -version-info $(LIBTOOL_VERSION)

CLEANFILES = switchc2s.h switchs2c.h errorsdefs.h
MAINTAINERCLEANFILES = Makefile.in

SUFFIXES = .txt

$(srcdir)/errors.c: switchc2s.h switchs2c.h errorsdefs.h
switchc2s.h switchs2c.h errorsdefs.h: errordb.txt
	${PERL} ${srcdir}/makeinc.pl < ${srcdir}/errordb.txt && \
	${CP} errorsdefs.h $(top_builddir)/include/rpc2

