## Process this file with automake to produce Makefile.in

SUBDIRS = include src src-pt pkgs debian

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = PORTING bootstrap.sh configure

MAINTAINERCLEANFILES = ChangeLog Makefile.in aclocal.m4 compile config.guess \
    config.h.in config.sub configure install-sh libtool ltconfig ltmain.sh \
    missing mkinstalldirs stamp-h.in

dist-hook:
	if [ -x /afs/cs.cmu.edu ] ; then cp -f /afs/cs.cmu.edu/project/coda-src/cvs/CVSROOT/ChangeLog.lwp $(distdir)/ChangeLog ; fi

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

