## Process this file with automake to produce Makefile.in

SUBDIRS = ${subdirs} configs lib-src coda-src tools

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = LICENSE README.BSD README.Solaris README.nt \
	     bootstrap.sh configure README.md

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

DISTCHECK_CONFIGURE_FLAGS =\
       --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)
