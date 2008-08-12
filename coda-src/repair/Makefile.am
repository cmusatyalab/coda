## Process this file with automake to produce Makefile.in

if BUILD_CLIENT
bin_PROGRAMS = repair
dist_man_MANS = repair.1
endif

repair_SOURCES = repair.cc repair.h

AM_CPPFLAGS = $(RPC2_CFLAGS) \
	      -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/partition \
	      -I$(top_srcdir)/coda-src/auth2 \
	      -I$(top_builddir)/coda-src/auth2 \
	      -I$(top_srcdir)/coda-src/vv \
	      -I$(top_srcdir)/coda-src/librepair

LDADD = $(top_builddir)/coda-src/librepair/libclnrepair.la \
	$(top_builddir)/coda-src/librepair/librepio.la \
	$(top_builddir)/coda-src/vv/libvv.la \
	$(top_builddir)/coda-src/auth2/libauser.la \
	$(top_builddir)/coda-src/kerndep/libkerndep.la \
	$(top_builddir)/lib-src/base/libbase.la \
	$(LIBREADLINE) $(LIBTERMCAP)

