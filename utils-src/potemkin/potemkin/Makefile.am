## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = potemkin

potemkin_SOURCES = potemkin.c potemkin.h

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep \
	      -I$(top_srcdir)/utils-src/potemkin/ds

LDADD = $(top_builddir)/utils-src/potemkin/ds/libds.la

