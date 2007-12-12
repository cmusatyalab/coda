## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libseglwp.la
if LIBRVM
lib_LTLIBRARIES += libseg.la
endif

AM_CPPFLAGS = -I$(top_srcdir)/include

libseg_sources = rvm_segutil.c rvm_loadseg.c rvm_createseg.c rvm_releaseseg.c \
		 rvm_segment_private.h

# there really is no reason for 2 separate libraries, except for the fact that
# cygwin won't build DLL's unless it can resolve all symbols at link time. So
# we need to know if we are going to use symbols from librvm or librvmlwp.
libseg_la_CPPFLAGS = $(AM_CPPFLAGS)
libseg_la_SOURCES = $(libseg_sources)
libseg_la_LDFLAGS = $(LIBTOOL_LDFLAGS)
libseg_la_LIBADD = $(top_builddir)/rvm/librvm.la

libseglwp_la_CPPFLAGS = $(AM_CPPFLAGS)
libseglwp_la_SOURCES = $(libseg_sources)
libseglwp_la_LDFLAGS = $(LIBTOOL_LDFLAGS)
libseglwp_la_LIBADD = $(top_builddir)/rvm/librvmlwp.la

MAINTAINERCLEANFILES = Makefile.in

