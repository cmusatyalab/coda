## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = testrvm rvm_basher lwp_basher
INCLUDES = -I$(top_srcdir)/include

testrvm_SOURCES = testrvm.c testrvm.h
testrvm_LDADD = $(top_builddir)/rvm/librvmlwp.la

rvm_basher_CFLAGS = $(AM_CFLAGS)
rvm_basher_LDADD = $(top_builddir)/rds/librds.la

lwp_basher_CPPFLAGS = -DRVM_USELWP
lwp_basher_SOURCES = rvm_basher.c
lwp_basher_LDADD = $(top_builddir)/rds/librdslwp.la

EXTRA_DIST = README basher_parms map_chk_file map_data_file \
    t1_chk_file t3_chk_file t5_chk_file test_data_file
MAINTAINERCLEANFILES = Makefile.in

