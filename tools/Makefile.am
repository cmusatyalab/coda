## Process this file with automake to produce Makefile.in

dist_noinst_SCRIPTS = make-split-dist.sh maketags pkg-bsd.sh \
	find_unused_symbols.py version_check.sh check-blurb

certificatedir = $(sysconfdir)/ssl
dist_certificate_DATA = Coda_CA.crt

if HAVE_SYSTEMD
dist_modulesload_DATA =
systemdsystemunit_DATA =
if BUILD_CLIENT
dist_modulesload_DATA += coda.conf
systemdsystemunit_DATA += coda-client.service
endif
if BUILD_SERVER
systemdsystemunit_DATA += coda-server.service coda-auth2.service \
        coda-update-master.service coda-update-slave.service
endif
endif

%.service: $(srcdir)/%.tmpl $(srcdir)/Makefile.am $(top_builddir)/config.status
	$(AM_V_GEN)
	$(AM_V_at)$(SED) -e "s|\@SBINDIR\@|@sbindir@|g" -e "s|\@SYSCONFDIR\@|@sysconfdir@|g" $< > $@

CLEANFILES = coda-client.service coda-server.service coda-auth2.service \
	     coda-update-master.service coda-update-slave.service
EXTRA_DIST = coda-server-nt-50x.wse coda-server-nt-52x.wse PACKAGING.README \
	     pkg-readme windows-socket-extension.wse \
	     coda-client.init coda-server.init coda-update.init \
	     coda-client.tmpl coda-server.tmpl coda-auth2.tmpl \
	     coda-update-master.tmpl coda-update-slave.tmpl

CONFIG_CLEAN_FILES =
