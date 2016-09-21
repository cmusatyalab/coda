## Process this file with automake to produce Makefile.in

if BUILD_SERVER
sbin_PROGRAMS = updateclnt updatefetch updatesrv
dist_man_MANS = updateclnt.8 updatesrv.8
endif

RPC2_FILES = update.rpc2
include $(top_srcdir)/configs/rpc2_rules.mk

updateclnt_SOURCES = updateclnt.cc
updatefetch_SOURCES = updatefetch.cc
updatesrv_SOURCES = updatesrv.cc

nodist_updateclnt_SOURCES = update.client.c update.helper.c update.h
nodist_updatefetch_SOURCES = update.client.c update.helper.c update.h
nodist_updatesrv_SOURCES = update.server.c update.helper.c update.h

AM_CPPFLAGS = $(RPC2_CFLAGS) \
	      -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep

LDADD = $(top_builddir)/coda-src/vice/libviceerror.la \
	$(top_builddir)/coda-src/vicedep/libvolutildep.la \
	$(top_builddir)/coda-src/util/libutil.la \
	$(top_builddir)/lib-src/base/libbase.la \
	$(RPC2_LIBS)

