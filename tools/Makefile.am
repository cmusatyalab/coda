## Process this file with automake to produce Makefile.in

dist_noinst_SCRIPTS = make-split-dist.sh maketags pkg-bsd.sh find_unused_symbols.py

if HAVE_SYSTEMD
dist_systemdsystemunit_DATA =
if BUILD_CLIENT
dist_systemdsystemunit_DATA += coda-client.service
endif
if BUILD_SERVER
dist_systemdsystemunit_DATA += coda-server.service \
        auth2-master.service auth2-slave.service \
        coda-update-master.service coda-update-slave.service
endif
endif

EXTRA_DIST = coda-server-nt-50x.wse coda-server-nt-52x.wse PACKAGING.README \
	     pkg-readme windows-socket-extension.wse \
	     coda-client.init coda-server.init coda-update.init

CONFIG_CLEAN_FILES =

