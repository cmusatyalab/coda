## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = liblwp.la
noinst_PROGRAMS = cswitch testlwp testlwp-static tdb rw

AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = liblwp.la

liblwp_la_SOURCES = fasttime.c iomgr.c lock.c lwp.c timer.c process.S \
		    lwp_ucontext.c lwp_ucontext.h lwp_stacktrace.c \
		    lwp_stacktrace.h lwp.private.h valgrind.h
liblwp_la_CPPFLAGS = $(AM_CPPFLAGS) -DLWPDEBUG
liblwp_la_LDFLAGS = $(LIBTOOL_LDFLAGS)

testlwp_static_SOURCES = testlwp.c
testlwp_static_LDFLAGS = -static

MAINTAINERCLEANFILES = Makefile.in

# Needed for mips, hopefully doesn't affect other platforms
.S.o:
	$(COMPILE) -DPIC -fPIC -c $<

.S.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -DPIC -fPIC -c $<

