## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = librvm.la librvmlwp.la
sbin_PROGRAMS = rvmutl

INCLUDES = -I$(top_srcdir)/include $(LWP_CFLAGS)
LDADD = librvmlwp.la

librvm_sources = rvm_init.c rvm_map.c rvm_unmap.c rvm_trans.c \
    rvm_logstatus.c rvm_logflush.c rvm_logrecovr.c rvm_utils.c rvm_io.c \
    rvm_status.c rvm_debug.c rvm_printers.c rvm_private.h

librvm_la_CFLAGS = $(AM_CFLAGS)
librvm_la_SOURCES = $(librvm_sources)
librvm_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)

librvmlwp_la_CPPFLAGS = -DRVM_USELWP
librvmlwp_la_SOURCES = $(librvm_sources)
librvmlwp_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)
librvmlwp_la_LIBADD = $(LWP_LIBS)

EXTRA_DIST = rvm_pthread.c README.log1.4 READ_ME
MAINTAINERCLEANFILES = Makefile.in

