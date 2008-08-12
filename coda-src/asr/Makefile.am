## Process this file with automake to produce Makefile.in

if BUILD_CLIENT
bin_PROGRAMS = parser
endif

parser_SOURCES = resolver_parser.y resolver_lexer.l ruletypes.cc ruletypes.h \
		 resolver.cc wildmat.c path.c asr.h
EXTRA_DIST = resolve.eg

AM_YFLAGS = -d
AM_CPPFLAGS = $(RPC2_CFLAGS) \
	      -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/vv

LDADD = $(top_builddir)/coda-src/util/libutil.la \
	$(top_builddir)/coda-src/kerndep/libkerndep.la \
	$(top_builddir)/lib-src/base/libbase.la

# resolver_parser should be compiled as a C++ file
resolver_parser.o: resolver_parser.c
	$(CXXCOMPILE) -c -o $@ $<

