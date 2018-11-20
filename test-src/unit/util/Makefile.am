## Process this file with automake to produce Makefile.in

check_PROGRAMS = unittest

unittest_SOURCES = u_bitmap.cc u_test_rvm.cc

GTEST_DIR = $(top_builddir)/external-src/googletest/googletest

unittest_LDADD = $(top_builddir)/coda-src/util/libutil.la \
             $(top_builddir)/lib-src/base/libbase.la \
             $(top_builddir)/test-src/unit/.libs/libmain_test.la \
             $(GTEST_DIR)/lib/libgtest.la \
             $(top_builddir)/test-src/unit/.libs/librecov.la \
             $(RVM_RPC2_LIBS)

AM_CPPFLAGS = $(RVM_RPC2_CFLAGS) -DVENUS -DTIMING -DVENUSDEBUG \
              -I$(top_srcdir)/test-src/unit/include \
              -I$(top_srcdir)/lib-src/base \
              -I$(top_srcdir)/coda-src \
              -I$(top_srcdir)/coda-src/util \
              -I$(top_builddir)/coda-src \
              -I$(GTEST_DIR)/include

check-local: unittest
	@LIBTOOL_PREFIX@ ./unittest @GTEST_FLAGS@

memcheck: unittest
	@LIBTOOL_PREFIX@ @VALGRIND@ ./unittest @GTEST_FLAGS@
