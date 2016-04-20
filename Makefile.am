## Process this file with automake to produce Makefile.in

DIST_SUBDIRS = configs lib-src coda-src debian tools
SUBDIRS = ${subdirs} ${DIST_SUBDIRS}

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = LICENSE README.BSD README.Solaris README.kerberos README.nt \
	     bootstrap.sh configure README.md

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'
