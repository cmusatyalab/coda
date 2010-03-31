## Process this file with automake to produce Makefile.in

SUBDIRS = ${subdirs} configs lib-src coda-src debian tools

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = LICENSE README.BSD README.Solaris README.kerberos README.nt \
	     bootstrap.sh configure

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'
