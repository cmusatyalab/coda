## Process this file with automake to produce Makefile.in
check_PROGRAMS = unittest

# Add unittest and venus sources
unittest_SOURCES = venus_symbols.cc u_cachefile.cc u_cachechunk.cc

GTEST_DIR = $(top_builddir)/external-src/googletest/googletest

unittest_LDADD = $(top_builddir)/test-src/unit/.libs/libmain_test.la \
             $(top_builddir)/coda-src/venus/.libs/libvenus.la \
             $(GTEST_DIR)/lib/libgtest.la

AM_CPPFLAGS = $(RVM_RPC2_CFLAGS) -DVENUS -DTIMING -DVENUSDEBUG \
              -I$(top_srcdir)/test-src/unit/include \
              -I$(top_srcdir)/lib-src/base \
              -I$(top_srcdir)/coda-src \
              -I$(top_srcdir)/coda-src/util \
              -I$(top_builddir)/coda-src \
              -I$(top_builddir)/coda-src/vicedep \
              -I$(top_builddir)/coda-src/vv \
              -I$(top_builddir)/coda-src/dir \
              -I$(top_builddir)/coda-src/kerndep \
              -I$(top_srcdir)/coda-src/venus \
              -I$(top_builddir)/coda-src/auth2 \
              -I$(top_builddir)/coda-src/lka \
              -I$(top_builddir)/coda-src/vol \
              -I$(top_builddir)/coda-src/al \
              -I$(top_builddir)/coda-src/librepair \
              -I$(GTEST_DIR)/include

check-local: unittest 
	@LIBTOOL_PREFIX@ ./unittest @GTEST_FLAGS@ 

memcheck: unittest
	@LIBTOOL_PREFIX@ @VALGRIND@ ./unittest @GTEST_FLAGS@ 
