## Process this file with automake to produce Makefile.in

check_PROGRAMS = unit

LIB_SRC = lib/rvm/rvm.cc lib/lwp/lwp.cc

UTIL_TESTS = util/u_bitmap.cc

unit_SOURCES = main.cc $(UTIL_TESTS) $(LIB_SRC)

GTEST_DIR = $(top_builddir)/external-src/googletest/googletest

unit_LDADD = $(top_builddir)/coda-src/util/libutil.la \
             $(top_builddir)/lib-src/base/libbase.la \
             $(GTEST_DIR)/lib/libgtest.la \
             $(RVM_RPC2_LIBS)

AM_CPPFLAGS = $(RVM_RPC2_CFLAGS) -DVENUS -DTIMING -DVENUSDEBUG \
              -I$(top_srcdir)/test-src/unit/include \
              -I$(top_srcdir)/lib-src/base \
              -I$(top_srcdir)/coda-src \
              -I$(top_srcdir)/coda-src/util \
              -I$(top_builddir)/coda-src \
              -I$(GTEST_DIR)/include

LIBTOOL = libtool --mode=execute
GTEST_FLAGS = --gtest_output="xml:$(top_builddir)/unittest.xml" --gtest_shuffle
VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all
UNIT_BIN = $(top_builddir)/test-src/unit/unit


$(GTEST_DIR)/lib/libgtest.la:
	$(MAKE) -C $(GTEST_DIR) lib/libgtest.la

clean-local:
	if test -f "${GTEST_DIR}/Makefile"; then make -C $(GTEST_DIR) clean; fi

distclean-local:
	if test -f "${GTEST_DIR}/Makefile"; then make -C $(GTEST_DIR) distclean; fi

check-local: unit
	${LIBTOOL} ${UNIT_BIN} ${GTEST_FLAGS} ${GTEST_EXTRA_FLAGS}

memcheck: unit
	${LIBTOOL} valgrind ${VALGRIND_FLAGS} ${UNIT_BIN} ${GTEST_FLAGS} ${GTEST_EXTRA_FLAGS}
