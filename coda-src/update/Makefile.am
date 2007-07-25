## Process this file with automake to produce Makefile.in

if BUILD_SERVER
sbin_PROGRAMS = updateclnt updatefetch updatesrv
dist_man_MANS = updateclnt.8 updatesrv.8
endif

RPC2_FILES = update.rpc2

updateclnt_SOURCES = updateclnt.cc update.client.c update.h
updatefetch_SOURCES = updatefetch.cc update.client.c update.h
updatesrv_SOURCES = updatesrv.cc update.server.c update.h
EXTRA_DIST = ${RPC2_FILES}

BUILT_SOURCES = update.h update.client.c update.server.c
CLEANFILES = ${RPC2_FILES:.rpc2=.h} ${RPC2_FILES:.rpc2=.client.c} \
	     ${RPC2_FILES:.rpc2=.server.c} ${RPC2_FILES:.rpc2=.multi.c} \
	     ${RPC2_FILES:.rpc2=.print.c}

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep

LDADD = $(top_builddir)/coda-src/vice/libviceerror.la \
	$(top_builddir)/coda-src/vicedep/libvolutildep.la \
	$(top_builddir)/coda-src/util/libutil.la \
	$(top_builddir)/lib-src/base/libbase.la \
	$(LIBRPC2) $(LIBLWP)

SUFFIXES = .rpc2 .client.c .server.c .multi.c
.rpc2.client.c .rpc2.server.c .rpc2.multi.c .rpc2.print.c .rpc2.h:
	$(RP2GEN) $<

