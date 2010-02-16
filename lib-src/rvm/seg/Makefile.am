## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES =
if LIBRVM
lib_LTLIBRARIES += libseg.la
endif
if LIBRVMLWP
lib_LTLIBRARIES += libseglwp.la
endif
if LIBRVMPT
lib_LTLIBRARIES += libsegpt.la
endif

AM_CPPFLAGS = -I$(top_srcdir)/include

libseg_sources = rvm_segutil.c rvm_loadseg.c rvm_createseg.c rvm_releaseseg.c \
		 rvm_segment_private.h

# there really is no reason for separate libraries, except for the fact that
# cygwin won't build DLL's unless it can resolve all symbols at link time. So
# we need to know if we are going to use symbols from librvm, librvmlwp or
# librvmpt.
libseg_la_CPPFLAGS = $(AM_CPPFLAGS)
libseg_la_SOURCES = $(libseg_sources)
libseg_la_LIBADD = $(top_builddir)/rvm/librvm.la

libseglwp_la_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_RVMLWP $(LWP_CFLAGS)
libseglwp_la_SOURCES = $(libseg_sources)
libseglwp_la_LIBADD = $(top_builddir)/rvm/librvmlwp.la $(LWP_LIBS)

libsegpt_la_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_RVMPT $(PTHREAD_CFLAGS)
libsegpt_la_SOURCES = $(libseg_sources)
libsegpt_la_LIBADD = $(top_builddir)/rvm/librvmpt.la $(PTHREAD_LIBS)

MAINTAINERCLEANFILES = Makefile.in

