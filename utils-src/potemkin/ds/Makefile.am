## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES= libds.la

noinst_HEADERS = ds_hash.h ds_list.h ds_log.h ds_rrlist.h ds_safeq.h odytypes.h
libds_la_SOURCES = ds_hash.c ds_hash.private.h ds_list.c ds_list.private.h \
		   ds_log.c ds_log.private.h ds_rrlist.c ds_rrlist.private.h \
		   ds_safeq.c ds_safeq.private.h

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base

