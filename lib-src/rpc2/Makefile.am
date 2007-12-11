## Process this file with automake to produce Makefile.in

SUBDIRS = secure rpc2-src rp2gen scripts include pkgs debian

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = bootstrap.sh configure README.ipv6

MAINTAINERCLEANFILES = AUTHORS ChangeLog Makefile.in README aclocal.m4 \
    compile config.guess config.h.in config.sub configure install-sh libtool \
    ltconfig ltmain.sh missing mkinstalldirs stamp-h.in

dist-hook:
	if [ -x /afs/cs.cmu.edu ] ; then cp -f /afs/cs.cmu.edu/project/coda-src/cvs/CVSROOT/ChangeLog.rpc2 $(distdir)/ChangeLog ; fi

