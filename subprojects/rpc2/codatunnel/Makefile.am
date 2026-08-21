## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/include $(LIBUV_CFLAGS) $(GNUTLS_CFLAGS) $(LWP_CFLAGS)

noinst_LTLIBRARIES = libcodatunnel.la

if CODATUNNEL
CODATUNNEL_SOURCES = codatunnel.c codatunneld.c codatunneld.private.h remotedest.c \
    cookie.c cookie.h codatunnel.private.h ctp.h
else
CODATUNNEL_SOURCES = codatunnel.stub.c
endif
libcodatunnel_la_SOURCES = $(CODATUNNEL_SOURCES)
libcodatunnel_la_LIBADD = $(LIBUV_LIBS) $(GNUTLS_LIBS) $(LWP_LIBS)

EXTRA_DIST = meson.build

MAINTAINERCLEANFILES = Makefile.in
