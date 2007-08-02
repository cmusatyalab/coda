## Process this file with automake to produce Makefile.in

SUBDIRS = configs lib-src coda-src debian tools

EXTRA_DIST = LICENSE README.BSD README.Solaris README.kerberos README.nt \
	     bootstrap.sh configure

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'
