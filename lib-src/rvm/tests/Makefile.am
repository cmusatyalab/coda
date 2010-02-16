## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = testrvm

if LIBRVM
noinst_PROGRAMS += rvm_basher
testrvm_LDADD = $(top_builddir)/rvm/librvm.la
endif
if LIBRVMLWP
noinst_PROGRAMS += lwp_basher
testrvm_LDADD = $(top_builddir)/rvm/librvmlwp.la
endif
if LIBRVMPT
noinst_PROGRAMS += pt_basher
testrvm_LDADD = $(top_builddir)/rvm/librvmpt.la
endif

AM_CPPFLAGS = -I$(top_srcdir)/include

testrvm_SOURCES = testrvm.c testrvm.h

rvm_basher_CPPFLAGS = $(AM_CPPFLAGS)
rvm_basher_LDADD = $(top_builddir)/rds/librds.la \
		   $(top_builddir)/seg/libseg.la \
		   $(top_builddir)/rvm/librvm.la

lwp_basher_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USELWP $(LWP_CFLAGS)
lwp_basher_SOURCES = rvm_basher.c
lwp_basher_LDADD = $(top_builddir)/rds/librdslwp.la \
		   $(top_builddir)/seg/libseglwp.la \
		   $(top_builddir)/rvm/librvmlwp.la \
		   $(LWP_LIBS)

pt_basher_CPPFLAGS = $(AM_CPPFLAGS) -DRVM_USEPT $(PTHREAD_CFLAGS)
pt_basher_SOURCES = rvm_basher.c
pt_basher_LDADD = $(top_builddir)/rds/librdspt.la \
		  $(top_builddir)/seg/libsegpt.la \
		  $(top_builddir)/rvm/librvmpt.la \
		  $(PTHREAD_LIBS)

EXTRA_DIST = README basher_parms map_chk_file map_data_file \
    t1_chk_file t3_chk_file t5_chk_file test_data_file
MAINTAINERCLEANFILES = Makefile.in

