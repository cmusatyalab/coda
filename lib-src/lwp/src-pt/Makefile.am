## Process this file with automake to produce Makefile.in

# BLURB lgpl
# 
#                            Coda File System
#                               Release 5
# 
#           Copyright (c) 1987-1999 Carnegie Mellon University
#                   Additional copyrights listed below
# 
# This  code  is  distributed "AS IS" without warranty of any kind under
# the  terms of the  GNU  Library General Public Licence  Version 2,  as
# shown in the file LICENSE. The technical and financial contributors to
# Coda are listed in the file CREDITS.
# 
#                         Additional copyrights
#                            none currently
# 
#*/

VPATH=$(srcdir):$(top_srcdir)/src

lib_LTLIBRARIES = liblwp_pt.la
INCLUDES = -I$(top_srcdir)/include -D_REENTRANT
liblwp_pt_la_SOURCES = iomgr_pt.c lock_pt.c lwp_pt.c preempt_pt.c \
    lwp.private_pt.h dllist.h
liblwp_pt_la_LIBADD = fasttime_pt.lo timer_pt.lo
liblwp_pt_la_LDFLAGS = -version-info 1:1:0 $(LIBPTHREAD)

noinst_PROGRAMS = cswitch_pt testlwp_pt testlwp-static_pt tdb_pt rw_pt
cswitch_pt_SOURCES =
cswitch_pt_LDADD = cswitch_pt.o liblwp_pt.la
testlwp_pt_SOURCES =
testlwp_pt_LDADD = testlwp_pt.o liblwp_pt.la
testlwp_static_pt_SOURCES =
testlwp_static_pt_LDADD = testlwp_pt.o liblwp_pt.la
testlwp_static_pt_LDFLAGS = -static
tdb_pt_SOURCES =
tdb_pt_LDADD = tdb_pt.o liblwp_pt.la
rw_pt_SOURCES =
rw_pt_LDADD = rw_pt.o liblwp_pt.la

EXTRA_DIST = README.pthreads
MAINTAINERCLEANFILES = Makefile.in

fasttime_pt.lo: fasttime.c
	$(LTCOMPILE) -c $< && \
	mv fasttime.lo fasttime_pt.lo && \
	mv fasttime.o fasttime_pt.o
        
timer_pt.lo: timer.c
	$(LTCOMPILE) -c $< && \
	mv timer.lo timer_pt.lo && \
	mv timer.o timer_pt.o
        
cswitch_pt.o: cswitch.c
	$(COMPILE) -c $< && mv cswitch.o cswitch_pt.o

testlwp_pt.o: testlwp.c
	$(COMPILE) -c $< && mv testlwp.o testlwp_pt.o

tdb_pt.o: tdb.c
	$(COMPILE) -c $< && mv tdb.o tdb_pt.o

rw_pt.o: rw.c
	$(COMPILE) -c $< && mv rw.o rw_pt.o
