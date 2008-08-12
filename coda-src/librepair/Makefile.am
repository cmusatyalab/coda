## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = librepio.la libclnrepair.la

librepio_la_SOURCES = repio.cc repio.h
libclnrepair_la_SOURCES = resolve.cc resolve.h cure.cc cure.h predicate.cc \
			  predicate.h repcmds.cc repcmds.h rvol.cc path.cc

AM_CPPFLAGS = $(RPC2_CFLAGS) \
	      -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/al \
	      -I$(top_srcdir)/coda-src/partition \
	      -I$(top_srcdir)/coda-src/auth2 \
	      -I$(top_builddir)/coda-src/auth2 \
	      -I$(top_srcdir)/coda-src/vv \
	      -I$(top_srcdir)/coda-src/vol

