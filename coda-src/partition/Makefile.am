## Process this file with automake to produce Makefile.in

if BUILD_SERVER
SUBDIRS = . tests
noinst_LTLIBRARIES = libpartition.la
sbin_PROGRAMS = inoder
dist_man_MANS = vicetab.5
endif

libpartition_la_SOURCES = vicetab.c vicetab.h partition.c partition.h \
			  inodeops.c inodeops.h simpleifs.c simpleifs.h \
			  ftreeifs.c ftreeifs.h backupifs.c viceinode.h
inoder_SOURCES		= inoder.c

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep

LDADD = libpartition.la \
	$(top_builddir)/coda-src/util/libutil.la \
	$(top_builddir)/lib-src/base/libbase.la \
	$(LIBLWP)

