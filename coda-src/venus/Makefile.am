## Process this file with automake to produce Makefile.in

if BUILD_CLIENT
sbin_PROGRAMS = venus vutil
dist_man_MANS = venus.8 vutil.8
dist_sysconf_DATA = venus.conf.ex realms
endif

if BUILD_TESTS
noinst_PROGRAMS = testtallyent
endif

venus_SOURCES = binding.cc binding.h comm.cc comm.h comm_daemon.cc daemon.cc \
    fso.h fso0.cc fso1.cc fso_cachefile.cc fso_cfscalls0.cc fso_cfscalls1.cc \
    fso_cfscalls2.cc fso_daemon.cc fso_dir.cc hdb.cc hdb.h hdb_daemon.cc \
    local.h local_cml.cc local_fake.cc local_fso.cc local_repair.cc \
    local_vol.cc mariner.cc mariner.h mgrp.cc mgrp.h venus.private.h venus.cc \
    venuscb.cc venuscb.h venusfid.h venusrecov.cc venusrecov.h venusstats.h \
    venusutil.cc venusvol.cc venusvol.h vol_daemon.cc vol_cml.cc \
    vol_reintegrate.cc vol_repair.cc vol_resolve.cc vol_vcb.cc vol_COP2.cc \
    vproc.cc vproc.h vproc_pathname.cc vproc_pioctl.cc vproc_vfscalls.cc \
    vsg.cc vsg.h worker.cc worker.h sighand.cc sighand.h spool.cc tallyent.cc \
    tallyent.h user.cc user.h nt_util.cc nt_util.h realmdb.cc realmdb.h \
    realm.cc realm.h rec_dllist.h refcounted.h
vutil_SOURCES = vutil.cc
nodist_testtallyent_SOURCES = tallyent.cc

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/dir \
	      -I$(top_srcdir)/coda-src/al \
	      -I$(top_srcdir)/coda-src/auth2 \
	      -I$(top_builddir)/coda-src/auth2 \
	      -I$(top_srcdir)/coda-src/vv \
	      -I$(top_srcdir)/coda-src/lka \
	      -I$(top_srcdir)/coda-src/vol \
	      -I$(top_srcdir)/coda-src/librepair \
	      -DVENUS -DTIMING -DVENUSDEBUG

venus_LDADD = $(top_builddir)/coda-src/librepair/librepio.la \
	      $(top_builddir)/coda-src/lka/liblka.la \
	      $(top_builddir)/coda-src/vv/libvv.la \
	      $(top_builddir)/coda-src/dir/libcodadir.la \
	      $(top_builddir)/coda-src/vicedep/libvenusdep.la \
	      $(top_builddir)/coda-src/util/libutil.la \
	      $(top_builddir)/lib-src/rwcdb/librwcdb.la \
	      $(top_builddir)/lib-src/base/libbase.la \
	      $(LIBRVM) $(LIBRPC2) $(LIBLWP)

vutil_LDADD = $(top_builddir)/lib-src/base/libbase.la

testtallyent_CPPFLAGS = -DTESTING \
			-I$(top_srcdir)/lib-src/base \
			-I$(top_srcdir)/coda-src/util \
			-I$(top_srcdir)/coda-src/vicedep \
			-I$(top_builddir)/coda-src/vicedep \
			-I$(top_srcdir)/coda-src/vol

testtallyent_LDADD = $(top_builddir)/coda-src/util/libutil.la \
		     $(top_builddir)/lib-src/base/libbase.la
