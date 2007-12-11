## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libseg.la
INCLUDES = -I$(top_srcdir)/include
libseg_la_SOURCES = rvm_segutil.c rvm_loadseg.c rvm_createseg.c \
    rvm_releaseseg.c rvm_segment_private.h
libseg_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBTOOL_LDFLAGS)

MAINTAINERCLEANFILES = Makefile.in

