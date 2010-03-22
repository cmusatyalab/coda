## Process this file with automake to produce Makefile.in

SUBDIRS = include rvm seg rds tests

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rvmlwp.pc
if LIBRVM
pkgconfig_DATA += rvm.pc
endif

EXTRA_DIST = bootstrap.sh configure \
    pkgs/rvm.spec pkgs/mk-cyg-pkg.sh pkgs/setup.hint \
    debian/changelog debian/compat debian/control debian/copyright debian/dirs \
    debian/librvm1.install debian/librvm-dev.install debian/librvm-dev.docs \
    debian/rules debian/rvm-tools.install

MAINTAINERCLEANFILES = AUTHORS ChangeLog Makefile.in README compile \
    config.guess config.h.in config.sub configure install-sh libtool \
    ltconfig ltmain.sh missing mkinstalldirs stamp-h.in debian/changelog

dist-hook:
	if [ -x /afs/cs.cmu.edu ] ; then cp -f /afs/cs.cmu.edu/project/coda-src/cvs/CVSROOT/ChangeLog.rvm $(distdir)/ChangeLog ; fi

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

