## Process this file with automake to produce Makefile.in

if BUILD_SERVER
noinst_LTLIBRARIES = libnorton.la
sbin_PROGRAMS = norton norton-reinit
dist_bin_SCRIPTS = reinit
dist_man_MANS = norton.8
endif

libnorton_la_SOURCES  = norton-setup.cc commands.cc norton-volume.cc \
			norton-recov.cc norton-vnode.cc norton-dir.cc \
			norton-rds.cc
norton_SOURCES	      = norton.cc norton.h dummy.cc
norton_reinit_SOURCES = norton-reinit.cc dummy.cc

AM_CPPFLAGS = $(RVM_RPC2_CFLAGS) \
	      -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/dir \
	      -I$(top_srcdir)/coda-src/al \
	      -I$(top_srcdir)/coda-src/partition \
	      -I$(top_srcdir)/coda-src/auth2 \
	      -I$(top_builddir)/coda-src/auth2 \
	      -I$(top_srcdir)/coda-src/vv \
	      -I$(top_srcdir)/coda-src/lka \
	      -I$(top_srcdir)/coda-src/vol \
	      -I$(top_srcdir)/coda-src/resolution \
	      -I$(top_srcdir)/coda-src/volutil
	      -I$(top_builddir)/coda-src/volutil

LDADD = libnorton.la \
	$(top_builddir)/coda-src/resolution/libres.la \
	$(top_builddir)/coda-src/vol/libvol.la \
	$(top_builddir)/coda-src/volutil/libvolutil.la \
	$(top_builddir)/coda-src/vv/libvv.la \
	$(top_builddir)/coda-src/partition/libpartition.la \
	$(top_builddir)/coda-src/al/libal.la \
	$(top_builddir)/coda-src/dir/libcodadir.la \
	$(top_builddir)/coda-src/util/libutil.la \
	$(top_builddir)/lib-src/rwcdb/librwcdb.la \
	$(top_builddir)/lib-src/base/libbase.la \
	$(RVM_RPC2_LIBS) $(LIBREADLINE) $(LIBTERMCAP)

