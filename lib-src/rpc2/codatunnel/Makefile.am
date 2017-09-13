## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/include $(LIBUV_CFLAGS)

noinst_LTLIBRARIES = libcodatunnel.la

libcodatunnel_la_SOURCES = codatunneld.c codatunnel.c codatunnel.private.h wrapper.h
libcodatunnel_la_LIBADD = $(LIBUV_LIBS)

MAINTAINERCLEANFILES = Makefile.in

