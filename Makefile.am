## Process this file with automake to produce Makefile.in

SUBDIRS = ${subdirs} configs lib-src coda-src python tools test-src

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = LICENSE LICENSE.lgpl README.BSD README.Solaris README.nt \
	     bootstrap.sh configure README.md \
	     mkdocs.yml docs docs-meta

distcleancheck_listfiles = \
       find -type f -exec sh -c 'test -f $(srcdir)/{} || echo {}' ';'

AM_DISTCHECK_CONFIGURE_FLAGS =\
    --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir) \
    --with-modulesloaddir=$$dc_install_base/$(modulesloaddir)

DISTCLEANFILES = unittest.xml

## Testing related
memcheck:
	$(MAKE) -C test-src/unit memcheck

.PHONY: doc doc-dev
doc:
	[ -z "$(MKDOCS)" ] || mkdocs build

clean-local:
	rm -rf site
