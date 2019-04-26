## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libutil.la

libutil_la_SOURCES = arrlist.h arrlist.cc bitmap.h bitmap.cc bitvect.h \
    bitvect.c bstree.h bstree.cc daemonizer.h daemonizer.c dhash.h dhash.cc \
    dict.h dict.cc dlist.h dlist.cc getsecret.h getsecret.cc histo.h histo.c \
    mkpath.h mkpath.c ohash.h ohash.cc olist.h olist.cc rec_bstree.h \
    rec_bstree.cc rec_dhash.h rec_dhash.cc rec_dlist.h rec_dlist.cc \
    rec_ohash.h rec_ohash.cc rec_olist.h rec_olist.cc rec_smolist.h \
    rec_smolist.cc recvarl.h recvarl.cc rvmlib.h rvmlib.c rvmtesting.h \
    rvmtesting.cc util.h util.c vice_file.h vice_file.c vmindex.h vmindex.cc \
    bitmap7.h bitmap7.cc

AM_CPPFLAGS = $(RVM_RPC2_CFLAGS) -I$(top_srcdir)/lib-src/base

