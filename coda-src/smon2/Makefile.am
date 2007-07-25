## Process this file with automake to produce Makefile.in

if BUILD_SERVER
bin_PROGRAMS = getvolinfo rpc2ping rvmsizer smon2
endif

AM_CPPFLAGS = -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep

LDADD = $(top_builddir)/coda-src/vicedep/libvenusdep.la \
	$(top_builddir)/lib-src/base/libbase.la \
	$(LIBRPC2) $(LIBLWP)

rvmsizer_LDADD = $(top_builddir)/lib-src/base/libbase.la

