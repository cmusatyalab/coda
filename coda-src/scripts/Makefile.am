## Process this file with automake to produce Makefile.in

sbin_SCRIPTS =
dist_sbin_SCRIPTS =
dist_init_SCRIPTS =
dist_man_MANS =
if BUILD_CLIENT
sbin_SCRIPTS += venus-setup
dist_sbin_SCRIPTS += volmunge
dist_man_MANS += venus-setup.8 volmunge.8
if SYSVINIT
dist_init_SCRIPTS += venus.init
endif
endif
if BUILD_SERVER
sbin_SCRIPTS += bldvldb.sh createvol_rep purgevol_rep startserver vice-setup \
		vice-setup-rvm
dist_sbin_SCRIPTS += backup.sh codastart coda-server-logrotate \
		     partial-reinit.sh tape.pl vice-killvolumes vice-setup-scm \
		     vice-setup-srvdir vice-setup-user
dist_man_MANS += bldvldb.sh.8 createvol_rep.8 maxgroupid.5 purgevol_rep.8 \
		 startserver.8 vice-setup.8 volumelist.5
if SYSVINIT
dist_init_SCRIPTS += auth2.init codasrv.init update.init
else
dist_init_SCRIPTS += rc.vice
endif
endif
EXTRA_DIST = findparents.sh volinfo.pl volsizes.pl

