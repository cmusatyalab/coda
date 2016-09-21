## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libvenusdep.la
if BUILD_SERVER
noinst_LTLIBRARIES += libvicedep.la libvolutildep.la
endif

noinst_HEADERS = operations.h recov_vollog.h srv.h venusioctl.h voltypes.h
RPC2_FILES = callback.rpc2 cml.rpc2 mond.rpc2 res.rpc2 vcrcommon.rpc2 \
	     vice.rpc2 voldump.rpc2 volutil.rpc2
include $(top_srcdir)/configs/rpc2_rules.mk
EXTRA_DIST += mond.rpc2.new

nodist_libvenusdep_la_SOURCES = vcrcommon.h callback.server.c cml.client.c \
    vice.client.c vice.multi.c callback.helper.c cml.helper.c vice.helper.c vcrcommon.helper.c
nodist_libvicedep_la_SOURCES = vcrcommon.h volutil.h callback.client.c \
    callback.multi.c mond.client.c res.client.c res.multi.c \
    res.server.c vice.server.c voldump.client.c volutil.server.c callback.helper.c \
    cml.helper.c mond.helper.c res.helper.c vice.helper.c voldump.helper.c vcrcommon.helper.c volutil.helper.c
nodist_libvolutildep_la_SOURCES = vcrcommon.h volutil.h voldump.server.c volutil.client.c voldump.helper.c \
    volutil.helper.c vcrcommon.helper.c

AM_CPPFLAGS = $(RPC2_CFLAGS)

