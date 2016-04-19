## Process this file with automake to produce Makefile.in

dist_noinst_SCRIPTS = make-split-dist.sh maketags pkg-bsd.sh

if HAVE_SYSTEMD
systemdsystemunit_DATA = coda-client.service coda-server.service \
        auth2-master.service auth2-slave.service \
        coda-update-master.service coda-update-slave.service
endif

EXTRA_DIST = coda-server-nt-50x.wse coda-server-nt-52x.wse PACKAGING.README \
	     pkg-readme windows-socket-extension.wse \
	     coda-client.init coda-server.init coda-update.init

CONFIG_CLEAN_FILES =

