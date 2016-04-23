## Process this file with automake to produce Makefile.in

SUBDIRS = include secure rpc2-src rp2gen scripts

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rpc2.pc

EXTRA_DIST = README.ipv6 bootstrap.sh configure

MAINTAINERCLEANFILES = AUTHORS Makefile.in README aclocal.m4 \
    compile config.guess config.h.in config.sub configure install-sh libtool \
    ltconfig ltmain.sh missing mkinstalldirs stamp-h.in

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

