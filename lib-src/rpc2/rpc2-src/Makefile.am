## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include \
    $(LUA_CFLAGS) $(LWP_CFLAGS) \
    -DRPC2DEBUG -DERRORTR -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
LDADD = librpc2.la libse.la

noinst_HEADERS = rpc2.private.h dllist.h test.h \
		 switchc2s.h switchs2c.h switchs2e.h
lib_LTLIBRARIES = librpc2.la libse.la
noinst_PROGRAMS = ctest stest rpc2test
EXTRA_DIST = makeinc.sh errordb.txt fxor.s

librpc2_la_SOURCES = rpc2a.c rpc2b.c sl.c packet.c secure.c globals.c se.c \
    lists.c ct.c debug.c conn.c cbuf.c ffs.c host.c multi1.c multi2.c \
    multi3.c mkcall.c stats.c errors.c cbuf.h trace.h dllist.c rpc2_addrinfo.c \
    compat.c lua.c delay.c
librpc2_la_LDFLAGS = $(LIBTOOL_LDFLAGS)
librpc2_la_LIBADD = $(top_builddir)/secure/libsecure.la $(LUA_LIBS) $(LWP_LIBS)

libse_la_SOURCES = sftp1.c sftp2.c sftp3.c sftp4.c sftp5.c sftp6.c
libse_la_LDFLAGS = $(LIBTOOL_LDFLAGS)
libse_la_LIBADD = librpc2.la $(LWP_LIBS)

MAINTAINERCLEANFILES = Makefile.in

