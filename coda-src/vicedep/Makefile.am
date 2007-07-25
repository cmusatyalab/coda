## Process this file with automake to produce Makefile.in

noinst_HEADERS = operations.h recov_vollog.h srv.h venusioctl.h voltypes.h
noinst_LTLIBRARIES = libvenusdep.la libvicedep.la libvolutildep.la

RPC2_FILES = callback.rpc2 cml.rpc2 mond.rpc2 res.rpc2 vcrcommon.rpc2 \
	     vice.rpc2 voldump.rpc2 volutil.rpc2

nodist_libvenusdep_la_SOURCES = vcrcommon.h callback.server.c cml.multi.c \
    vice.client.c vice.multi.c
nodist_libvicedep_la_SOURCES = vcrcommon.h callback.client.c \
    callback.multi.c cml.multi.c mond.client.c res.client.c res.multi.c \
    res.server.c vice.server.c voldump.client.c volutil.server.c
nodist_libvolutildep_la_SOURCES = vcrcommon.h voldump.server.c volutil.client.c

BUILT_SOURCES = $(nodist_libvenusdep_la_SOURCES) \
		$(nodist_libvicedep_la_SOURCES) \
		$(nodist_libvolutildep_la_SOURCES)
EXTRA_DIST = ${RPC2_FILES} mond.rpc2.new
CLEANFILES = ${RPC2_FILES:.rpc2=.h} ${RPC2_FILES:.rpc2=.client.c} \
	     ${RPC2_FILES:.rpc2=.server.c} ${RPC2_FILES:.rpc2=.multi.c} \
	     ${RPC2_FILES:.rpc2=.print.c}

SUFFIXES = .rpc2 .client.c .server.c .multi.c
.rpc2.client.c .rpc2.server.c .rpc2.multi.c .rpc2.print.c .rpc2.h:
	$(RP2GEN) -I $(srcdir) $<

