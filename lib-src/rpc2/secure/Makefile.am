## Process this file with automake to produce Makefile.in
# BLURB lgpl
#
#			    Coda File System
#				Release 5
#
#		Copyright (c) 1987-1999 Carnegie Mellon University
#			Additional copyrights listed below
#
# This  code  is  distributed "AS IS" without warranty of any kind under
# the  terms of the  GNU  Library General Public Licence  Version 2,  as
# shown in the file LICENSE. The technical and financial contributors to
# Coda are listed in the file CREDITS.
#
#			Additional copyrights
#			    none currently
#
#*/

INCLUDES = -I$(top_srcdir)/include -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
LDADD = libsecure.la

noinst_LTLIBRARIES = libsecure.la
noinst_PROGRAMS = secure_test

secure_test_SOURCES = secure_test.c
noinst_HEADERS = aes.h grunt.h rijndael-alg-fst.h testvectors.h
libsecure_la_SOURCES = secure_init.c secure_setup.c \
    secure_aes.c secure_random.c secure_recvfrom.c secure_sendto.c \
    auth_none.c auth_aes_xcbc.c encr_null.c encr_aes_cbc.c encr_aes_ccm.c \
    rijndael-alg-fst.c
libsecure_la_LDFLAGS = -version-info $(LIBTOOL_VERSION)

EXTRA_DIST = README.secure gen_testvectors.sh \
    rijndael-mikescott.h rijndael-mikescott.c
MAINTAINERCLEANFILES = Makefile.in

