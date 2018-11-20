## Process this file with automake to produce Makefile.in

check_LTLIBRARIES = libmain_test.la librecov.la

SUBDIRS = . util venus

LIB_SRC = lib/rvm/rvm.cc lib/lwp/lwp.cc

libmain_test_la_SOURCES = main.cc $(LIB_SRC)
librecov_la_SOURCES = lib/rvm/recov.cc

GTEST_DIR = $(top_builddir)/external-src/googletest/googletest

AM_CPPFLAGS = $(RVM_RPC2_CFLAGS) -DVENUS -DTIMING -DVENUSDEBUG \
              -I$(top_srcdir)/test-src/unit/include \
              -I$(top_srcdir)/lib-src/base \
              -I$(top_srcdir)/coda-src \
              -I$(top_srcdir)/coda-src/util \
              -I$(top_builddir)/coda-src \
              -I$(GTEST_DIR)/include

clean-local:
	if test -f "${GTEST_DIR}/Makefile"; then make -C $(GTEST_DIR) clean; fi

distclean-local:
	if test -f "${GTEST_DIR}/Makefile"; then make -C $(GTEST_DIR) distclean; fi


memcheck:
	$(MAKE) -C util memcheck
	$(MAKE) -C venus memcheck

check-local:
	$(MAKE) -C $(GTEST_DIR) lib/libgtest.la
