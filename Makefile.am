## Process this file with automake to produce Makefile.in

SUBDIRS = ${subdirs} configs lib-src coda-src tools test-src

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = LICENSE README.BSD README.Solaris README.nt \
	     bootstrap.sh configure README.md

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

AM_DISTCHECK_CONFIGURE_FLAGS =\
    --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir) \
    --with-modulesloaddir=$$dc_install_base/$(modulesloaddir)

DISTCLEANFILES = unittest.xml

## Testing related
check-local: all

memcheck: all
	$(MAKE) -C test-src/unit memcheck
