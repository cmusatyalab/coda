## Process this file with automake to produce Makefile.in

SUBDIRS = include rvm seg rds tests pkgs debian

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = bootstrap.sh configure

MAINTAINERCLEANFILES = AUTHORS ChangeLog Makefile.in README compile \
    config.guess config.h.in config.sub configure install-sh libtool \
    ltconfig ltmain.sh missing mkinstalldirs stamp-h.in

dist-hook:
	if [ -x /afs/cs.cmu.edu ] ; then cp -f /afs/cs.cmu.edu/project/coda-src/cvs/CVSROOT/ChangeLog.rvm $(distdir)/ChangeLog ; fi
