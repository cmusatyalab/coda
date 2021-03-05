## Process this file with automake to produce Makefile.in

bin_SCRIPTS =
sbin_SCRIPTS =
dist_bin_SCRIPTS =
dist_sbin_SCRIPTS =
dist_man_MANS =
if BUILD_CLIENT
bin_SCRIPTS += coda-sync-acls
sbin_SCRIPTS += coda-client-setup
dist_bin_SCRIPTS += volmunge
dist_man_MANS += coda-client-setup.8 volmunge.1
endif
if BUILD_SERVER
sbin_SCRIPTS += bldvldb.sh createvol_rep purgevol_rep startserver vice-setup \
		vice-setup-rvm
dist_sbin_SCRIPTS += codastart coda-server-logrotate \
		     partial-reinit.sh vice-killvolumes vice-setup-scm \
		     vice-setup-srvdir vice-setup-user
dist_man_MANS += bldvldb.sh.8 createvol_rep.8 maxgroupid.5 purgevol_rep.8 \
		 startserver.8 vice-setup.8 volumelist.5
endif
EXTRA_DIST = findparents.sh volinfo.pl volsizes.pl
CLEANFILES = $(bin_SCRIPTS) $(sbin_SCRIPTS)

