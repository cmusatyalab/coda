## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = librvmlwp.la
if LIBRVM
lib_LTLIBRARIES += librvm.la
endif
sbin_PROGRAMS = rvmutl

AM_CPPFLAGS = -I$(top_srcdir)/include

librvm_sources = rvm_init.c rvm_map.c rvm_unmap.c rvm_trans.c \
    rvm_logstatus.c rvm_logflush.c rvm_logrecovr.c rvm_utils.c rvm_io.c \
    rvm_status.c rvm_debug.c rvm_printers.c rvm_private.h

librvm_la_CPPFLAGS = $(AM_CPPFLAGS)
librvm_la_SOURCES = $(librvm_sources)
librvm_la_LDFLAGS = $(LIBTOOL_LDFLAGS)

librvmlwp_la_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USELWP $(LWP_CFLAGS)
librvmlwp_la_SOURCES = $(librvm_sources)
librvmlwp_la_LDFLAGS = $(LIBTOOL_LDFLAGS)
librvmlwp_la_LIBADD = $(LWP_LIBS)

rvmutl_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USELWP
rvmutl_LDADD = librvmlwp.la

EXTRA_DIST = rvm_pthread.c README.log1.4 READ_ME
MAINTAINERCLEANFILES = Makefile.in

