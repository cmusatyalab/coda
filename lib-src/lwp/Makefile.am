## Process this file with automake to produce Makefile.in

SUBDIRS = include src src-pt

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lwp.pc

EXTRA_DIST = PORTING bootstrap.sh configure

CONFIG_CLEAN_FILES = lwp.pc lwp-uninstalled.pc
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 compile config.guess \
    config.h.in config.sub configure install-sh libtool ltconfig ltmain.sh \
    missing mkinstalldirs stamp-h.in

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

