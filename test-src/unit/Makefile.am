## Process this file with automake to produce Makefile.in

check_PROGRAMS = unit

unit_SOURCES = main.cc

unit_LDADD = $(top_builddir)/coda-src/util/libutil.la \
             $(top_builddir)/lib-src/base/libbase.la \
             $(top_builddir)/external-src/googletest/googletest/lib/libgtest.la \
             $(RVM_RPC2_LIBS)

AM_CPPFLAGS = $(RVM_RPC2_CFLAGS) -DVENUS -DTIMING -DVENUSDEBUG \
              -I$(top_srcdir)/lib-src/base \
              -I$(top_srcdir)/coda-src \
              -I$(top_builddir)/coda-src

LIBTOOL = libtool --mode=execute
GTEST_FLAGS = --gtest_output="xml:$(top_builddir)/unittest.xml" --gtest_shuffle
VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all
UNIT_BIN = $(top_builddir)/test-src/unit/unit

gtest:
	$(MAKE) -C $(top_builddir)/external-src/googletest/googletest lib/libgtest.la

$(top_builddir)/external-src/googletest/googletest/libgtest.la: gtest

check-local: unit
	${LIBTOOL} ${UNIT_BIN} ${GTEST_FLAGS} ${GTEST_EXTRA_FLAGS}

memcheck: unit
	${LIBTOOL} valgrind ${VALGRIND_FLAGS} ${UNIT_BIN} ${GTEST_FLAGS} ${GTEST_EXTRA_FLAGS}
