## Process this file with automake to produce Makefile.in

EXTRA_DIST = changelog compat control copyright rules watch \
    coda-backup.dirs coda-backup.install \
    coda-client.config coda-client.dirs coda-client.docs coda-client.init.d \
    coda-client.install coda-client.modprobe coda-client.modules \
    coda-client.postinst coda-client.postrm coda-client.preinst \
    coda-client.templates \
    coda-server.dirs coda-server.docs coda-server.init.d coda-server.install \
    coda-update.init.d coda-update.install \
    coda-vcodacon.install

# make sure make distclean doesn't remove the changelog file
CONFIG_CLEAN_FILES =
CLEANFILES = files *.substvars *.debhelper
MAINTAINERCLEANFILES = changelog

