## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir) \
    -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
LDADD = libsecure.la

noinst_LTLIBRARIES = libsecure.la
noinst_PROGRAMS = secure_test

secure_test_SOURCES = secure_test.c
noinst_HEADERS = aes.h grunt.h rijndael-alg-fst.h testvectors.h
libsecure_la_SOURCES = secure_init.c secure_setup.c secure_pbkdf.c \
    secure_aes.c secure_random.c secure_recvfrom.c secure_sendto.c \
    auth_none.c auth_aes_xcbc.c encr_null.c encr_aes_cbc.c encr_aes_ccm.c \
    rijndael-alg-fst.c
#libsecure_la_LDFLAGS = $(LIBTOOL_LDFLAGS)

EXTRA_DIST = README.secure gen_testvectors.sh \
    rijndael-mikescott.h rijndael-mikescott.c
MAINTAINERCLEANFILES = Makefile.in

