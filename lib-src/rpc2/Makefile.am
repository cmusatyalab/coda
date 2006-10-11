## Process this file with automake to produce Makefile.in
# BLURB lgpl
#
#			Coda File System
#			   Release 5
#
#	Copyright (c) 1987-1999 Carnegie Mellon University
#		Additional copyrights listed below
#
# This  code  is  distributed "AS IS" without warranty of any kind under
# the  terms of the  GNU  Library General Public Licence  Version 2,  as
# shown in the file LICENSE. The technical and financial contributors to
# Coda are listed in the file CREDITS.
#
#			Additional copyrights
#			   none currently
#*/

SUBDIRS = secure rpc2-src rp2gen scripts include pkgs debian

EXTRA_DIST = acinclude.m4 bootstrap.sh configure README.ipv6

MAINTAINERCLEANFILES = AUTHORS ChangeLog Makefile.in README aclocal.m4 \
    config.guess config.h.in config.sub configure install-sh libtool \
    ltconfig ltmain.sh missing mkinstalldirs stamp-h.in

dist-hook:
	if [ -x /afs/cs.cmu.edu ] ; then cp -f /afs/cs.cmu.edu/project/coda-src/cvs/CVSROOT/ChangeLog.rpc2 $(distdir)/ChangeLog ; fi

