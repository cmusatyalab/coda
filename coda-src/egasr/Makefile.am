## Process this file with automake to produce Makefile.in

if BUILD_CLIENT
bin_PROGRAMS = filerepair removeinc
dist_bin_SCRIPTS = xfrepair xaskuser
endif

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/dir \
	      -I$(top_srcdir)/coda-src/vv

filerepair_LDADD = $(top_builddir)/coda-src/kerndep/libkerndep.la \
		   $(top_builddir)/lib-src/base/libbase.la

removeinc_LDADD = $(top_builddir)/coda-src/dir/libcodadir.la \
		  $(top_builddir)/coda-src/kerndep/libkerndep.la \
		   $(top_builddir)/lib-src/base/libbase.la

