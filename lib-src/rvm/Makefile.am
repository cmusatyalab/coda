## Process this file with automake to produce Makefile.in

SUBDIRS = include rvm seg rds tests

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rvmlwp.pc
if LIBRVM
pkgconfig_DATA += rvm.pc
endif

EXTRA_DIST = bootstrap.sh configure

MAINTAINERCLEANFILES = AUTHORS Makefile.in README compile \
    config.guess config.h.in config.sub configure install-sh libtool \
    ltconfig ltmain.sh missing mkinstalldirs stamp-h.in

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

