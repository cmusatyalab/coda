## Process this file with automake to produce Makefile.in

SUBDIRS = include src src-pt

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lwp.pc

EXTRA_DIST = PORTING bootstrap.sh configure \
    pkgs/lwp.spec pkgs/mk-cyg-pkg.sh pkgs/setup.hint \
    debian/changelog debian/compat debian/control debian/copyright debian/dirs \
    debian/liblwp2.docs debian/liblwp2.install debian/liblwp-dev.docs \
    debian/liblwp-dev.install debian/rules

CONFIG_CLEAN_FILES = lwp.pc lwp-uninstalled.pc pkgs/pkg-bsd.sh
MAINTAINERCLEANFILES = ChangeLog Makefile.in aclocal.m4 compile config.guess \
    config.h.in config.sub configure install-sh libtool ltconfig ltmain.sh \
    missing mkinstalldirs stamp-h.in debian/changelog pkgs/lwp.spec

dist-hook:
	if [ -x /afs/cs.cmu.edu ] ; then cp -f /afs/cs.cmu.edu/project/coda-src/cvs/CVSROOT/ChangeLog.lwp $(distdir)/ChangeLog ; fi

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

