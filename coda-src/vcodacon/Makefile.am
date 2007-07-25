## Process this file with automake to produce Makefile.in

if HAVE_FLTK
if BUILD_CLIENT
bin_PROGRAMS = vcodacon
#dist_man1_SMANS = vcodacon.1
BUILT_SOURCES = $(nodist_vcodacon_SOURCES)
endif
endif

vcodacon_SOURCES = Inet.cc Inet.h monitor.cc monitor.h util.cc util.h
nodist_vcodacon_SOURCES = vcodacon.cc vcodacon.h
EXTRA_DIST = vcodacon.fl

CLEANFILES = $(BUILT_SOURCES)

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base -DDEBUG $(FLTKFLAGS)
LDADD = $(top_builddir)/lib-src/base/libbase.la $(FLTKLIBS)

vcodacon.cc vcodacon.h: vcodacon.fl
	$(FLUID) -c $(srcdir)/vcodacon.fl

