## Process this file with automake to produce Makefile.in

VPATH=$(srcdir):$(top_srcdir)/src

if build_pthread
BUILD_LIB = liblwp_pt.la
BUILD_BIN = cswitch_pt testlwp_pt testlwp-static_pt tdb_pt rw_pt
endif

lib_LTLIBRARIES = $(BUILD_LIB)

AM_CPPFLAGS = -I$(top_srcdir)/include -D_REENTRANT

liblwp_pt_la_SOURCES = iomgr_pt.c lock_pt.c lwp_pt.c preempt_pt.c \
    lwp.private_pt.h dllist.h
liblwp_pt_la_LIBADD = fasttime_pt.lo timer_pt.lo
liblwp_pt_la_LDFLAGS = -version-info $(LIBTOOL_VERSION) $(LIBPTHREAD)

noinst_PROGRAMS = $(BUILD_BIN)
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
	mv fasttime.lo fasttime_pt.lo

timer_pt.lo: timer.c
	$(LTCOMPILE) -c $< && \
	mv timer.lo timer_pt.lo

cswitch_pt.o: cswitch.c
	$(COMPILE) -c $< && mv cswitch.o cswitch_pt.o

testlwp_pt.o: testlwp.c
	$(COMPILE) -c $< && mv testlwp.o testlwp_pt.o

tdb_pt.o: tdb.c
	$(COMPILE) -c $< && mv tdb.o tdb_pt.o

rw_pt.o: rw.c
	$(COMPILE) -c $< && mv rw.o rw_pt.o
