## Process this file with automake to produce Makefile.in

if BUILD_SERVER
noinst_LTLIBRARIES = libvolutil.la libvolserv.la libdumpstuff.la
sbin_PROGRAMS = volutil parserecdump backup readdump merge codadump2tar
dist_man_MANS = backup.8 backuplogs.5 dumpfile.5 dumplist.5 merge.8 readdump.8 \
		volutil.8
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
backup_SOURCES = backup.cc
readdump_SOURCES = readdump.cc
merge_SOURCES =	merge.cc
codadump2tar_SOURCES = codadump2tar.cc tar-FromRedHatCD.h
parserecdump_SOURCES = parserecdump.cc vol-dumprecstore.h

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
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
	      -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

volutil_LDADD = $(top_builddir)/coda-src/auth2/libauser.la \
		$(top_builddir)/coda-src/vicedep/libvolutildep.la \
		$(top_builddir)/coda-src/util/libutil.la \
		$(top_builddir)/coda-src/kerndep/libkerndep.la \
		$(top_builddir)/lib-src/base/libbase.la \
		$(LIBRPC2) $(LIBLWP)

backup_LDADD = $(top_builddir)/coda-src/vol/libvol.la \
	       $(top_builddir)/coda-src/partition/libpartition.la \
	       $(top_builddir)/coda-src/vicedep/libvolutildep.la \
	       $(top_builddir)/coda-src/util/libutil.la \
	       $(top_builddir)/lib-src/base/libbase.la \
	       $(LIBRPC2) $(LIBLWP)

readdump_LDADD = libdumpstuff.la \
		 $(top_builddir)/coda-src/vicedep/libvicedep.la \
		 $(top_builddir)/coda-src/util/libutil.la \
		 $(top_builddir)/lib-src/base/libbase.la \
		 $(LIBRPC2) $(LIBLWP) $(LIBREADLINE) $(LIBTERMCAP)

merge_LDADD = libdumpstuff.la \
	      $(top_builddir)/coda-src/vicedep/libvicedep.la \
	      $(top_builddir)/coda-src/util/libutil.la \
	      $(top_builddir)/lib-src/base/libbase.la \
	      $(LIBRPC2) $(LIBLWP)

codadump2tar_LDADD = libdumpstuff.la \
		     $(top_builddir)/coda-src/vol/libvol.la \
		     $(top_builddir)/coda-src/vv/libvv.la \
		     $(top_builddir)/coda-src/dir/libcodadir.la \
		     $(top_builddir)/coda-src/vicedep/libvicedep.la \
		     $(top_builddir)/coda-src/util/libutil.la \
		     $(top_builddir)/lib-src/base/libbase.la \
		     $(LIBRVM) $(LIBRPC2) $(LIBLWP)

