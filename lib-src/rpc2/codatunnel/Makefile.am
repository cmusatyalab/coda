## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/include $(LWP_CFLAGS)
LDADD = libfakeudp.la $(LWP_LIBS)

noinst_LTLIBRARIES = libfakeudp.la


libfakeudp_la_SOURCES = codatunneld.c fakeudp.c 
#libfakeudp_la_LDFLAGS = $(LIBTOOL_LDFLAGS)
libfakeudp_la_LIBADD = $(LWP_LIBS)

MAINTAINERCLEANFILES = Makefile.in

