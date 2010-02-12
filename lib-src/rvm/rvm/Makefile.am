## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES =
sbin_PROGRAMS = rvmutl
dist_man_MANS = rvmutl.1

AM_CPPFLAGS = -I$(top_srcdir)/include

if LIBRVM
lib_LTLIBRARIES += librvm.la
rvmutl_CPPFLAGS = $(AM_CPPFLAGS)
rvmutl_LDADD = librvm.la
endif
if LIBRVMLWP
lib_LTLIBRARIES += librvmlwp.la
rvmutl_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USELWP $(LWP_CFLAGS)
rvmutl_LDADD = librvmlwp.la $(LWP_LIBS)
endif
if LIBRVMPT
lib_LTLIBRARIES += librvmpt.la
rvmutl_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USEPT -pthread
rvmutl_LDADD = librvmpt.la -lpthread
endif

librvm_sources = rvm_init.c rvm_map.c rvm_unmap.c rvm_trans.c \
    rvm_logstatus.c rvm_logflush.c rvm_logrecovr.c rvm_utils.c rvm_io.c \
    rvm_status.c rvm_debug.c rvm_printers.c rvm_private.h

librvm_la_CPPFLAGS = $(AM_CPPFLAGS)
librvm_la_SOURCES = $(librvm_sources)

librvmlwp_la_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USELWP $(LWP_CFLAGS)
librvmlwp_la_SOURCES = $(librvm_sources)
librvmlwp_la_LIBADD = $(LWP_LIBS)

librvmpt_la_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USEPT -pthread
librvmpt_la_SOURCES = $(librvm_sources) rvm_pthread.c
librvmpt_la_LDFLAGS = -pthread
librvmpt_la_LIBADD = -lpthread

EXTRA_DIST = README.log1.4 READ_ME
MAINTAINERCLEANFILES = Makefile.in

