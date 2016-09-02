## Process this file with automake to produce Makefile.in

if BUILD_SERVER
noinst_LTLIBRARIES = libvolutil.la libvolserv.la libdumpstuff.la
sbin_PROGRAMS = volutil codareaddump codamergedump codadump2tar
dist_man_MANS = codadumpfile.5 codamergedump.8 codareaddump.8 volutil.8
endif

libdumpstuff_la_SOURCES = dumpstream.cc dumpstream.h dumpstuff.cc dump.h
libvolutil_la_SOURCES = vol-ancient.cc vol-backup.cc vol-clone.cc \
    vol-create.cc vol-dump.cc vol-dumpvrdb.cc vol-info.cc vol-lock.cc \
    vol-lookup.cc vol-makevldb.cc vol-makevrdb.cc vol-maxid.cc vol-purge.cc \
    vol-restore.cc vol-rvmsize.cc vol-salvage.cc vol-salvage.private.h \
    vol-setvv.cc vol-setlogparms.cc vol-showvnode.cc vol-timing.cc \
    vol-tracerpc.cc vol-printstats.cc vol-getvolumelist.cc \
    vol-showcallbacks.cc vol-rvmtrunc.cc readstuff.cc vvlist.cc vvlist.h \
    volutil.private.h dumpstream.cc dumpstream.h dumpstuff.cc dump.h
libvolserv_la_SOURCES = volutil.cc
volutil_SOURCES = volclient.cc
codareaddump_SOURCES = codareaddump.cc
codamergedump_SOURCES =	codamergedump.cc
codadump2tar_SOURCES = codadump2tar.cc tar-FromRedHatCD.h

AM_CPPFLAGS = $(RVM_RPC2_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
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
	      -I$(top_srcdir)/coda-src/resolution

volutil_LDADD = $(top_builddir)/coda-src/auth2/libauser.la \
		$(top_builddir)/coda-src/vicedep/libvolutildep.la \
		$(top_builddir)/coda-src/util/libutil.la \
		$(top_builddir)/coda-src/kerndep/libkerndep.la \
		$(top_builddir)/lib-src/base/libbase.la \
		$(RPC2_LIBS)

codareaddump_LDADD = libdumpstuff.la \
		 $(top_builddir)/coda-src/vv/libvv.la \
		 $(top_builddir)/coda-src/vicedep/libvicedep.la \
		 $(top_builddir)/coda-src/util/libutil.la \
		 $(top_builddir)/lib-src/base/libbase.la \
		 $(RPC2_LIBS) $(LIBREADLINE) $(LIBTERMCAP)

codamergedump_LDADD = libdumpstuff.la \
	      $(top_builddir)/coda-src/vicedep/libvicedep.la \
	      $(top_builddir)/coda-src/util/libutil.la \
	      $(top_builddir)/lib-src/base/libbase.la \
	      $(RPC2_LIBS)

codadump2tar_LDADD = libdumpstuff.la \
		     $(top_builddir)/coda-src/vol/libvol.la \
		     $(top_builddir)/coda-src/vv/libvv.la \
		     $(top_builddir)/coda-src/dir/libcodadir.la \
		     $(top_builddir)/coda-src/vicedep/libvicedep.la \
		     $(top_builddir)/coda-src/util/libutil.la \
		     $(top_builddir)/lib-src/base/libbase.la \
		     $(RVM_RPC2_LIBS)

