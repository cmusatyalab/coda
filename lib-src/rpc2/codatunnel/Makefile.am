## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/include $(LIBUV_CFLAGS)

noinst_LTLIBRARIES = libcodatunnel.la

if CODATUNNEL
CODATUNNEL_SOURCES = codatunnel.c codatunneld.c codatunnel.private.h
else
CODATUNNEL_SOURCES = codatunnel.stub.c
endif
libcodatunnel_la_SOURCES = $(CODATUNNEL_SOURCES) wrapper.h
libcodatunnel_la_LIBADD = $(LIBUV_LIBS)

MAINTAINERCLEANFILES = Makefile.in

