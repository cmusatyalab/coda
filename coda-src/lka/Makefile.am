## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = liblka.la

if BUILD_CLIENT
bin_PROGRAMS = mklka
endif

liblka_la_SOURCES = lka.c lka.h shaprocs.c lka_private.h

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/lib-src/rwcdb

LDADD = liblka.la \
	$(top_builddir)/lib-src/rwcdb/librwcdb.la \
	$(top_builddir)/lib-src/base/libbase.la

