## Process this file with automake to produce Makefile.in

SUBDIRS = include secure rpc2-src rp2gen scripts

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rpc2.pc

EXTRA_DIST = README.ipv6 bootstrap.sh configure \
    pkgs/rpc2.spec pkgs/mk-cyg-pkg.sh pkgs/setup.hint \
    debian/changelog debian/compat debian/control debian/copyright debian/dirs \
    debian/librpc2-5.docs debian/librpc2-5.install debian/librpc2-dev.docs \
    debian/librpc2-dev.install debian/rules debian/watch

MAINTAINERCLEANFILES = AUTHORS ChangeLog Makefile.in README aclocal.m4 \
    compile config.guess config.h.in config.sub configure install-sh libtool \
    ltconfig ltmain.sh missing mkinstalldirs stamp-h.in debian/changelog

dist-hook:
	if [ -x /afs/cs.cmu.edu ] ; then cp -f /afs/cs.cmu.edu/project/coda-src/cvs/CVSROOT/ChangeLog.rpc2 $(distdir)/ChangeLog ; fi

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

