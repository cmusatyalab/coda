## Process this file with automake to produce Makefile.in

# BLURB lgpl
# 
#                            Coda File System
#                               Release 5
# 
#           Copyright (c) 1987-1999 Carnegie Mellon University
#                   Additional copyrights listed below
# 
# This  code  is  distributed "AS IS" without warranty of any kind under
# the  terms of the  GNU  Library General Public Licence  Version 2,  as
# shown in the file LICENSE. The technical and financial contributors to
# Coda are listed in the file CREDITS.
# 
#                         Additional copyrights
#                            none currently
# 
#*/

noinst_PROGRAMS = testrvm rvm_basher lwp_basher
INCLUDES = -I$(top_srcdir)/include @LWPINCLUDES@

testrvm_SOURCES = testrvm.c testrvm.h
testrvm_LDADD = ../rvm/librvm.la

rvm_basher_LDADD = ../rds/librds.la ../seg/libseg.la ../rvm/librvm.la

lwp_basher_SOURCES =
lwp_basher_LDADD = lwp_basher.o ../rds/librdslwp.la ../seg/libseg.la \
    ../rvm/librvmlwp.la @LIBLWP@

EXTRA_DIST = README basher_parms log_file map_chk_file map_data_file \
    t1_chk_file t3_chk_file t5_chk_file test_data_file
MAINTAINERCLEANFILES = Makefile.in

rvm_basher.o: rvm_basher.c lwp_basher.o
lwp_basher.o: rvm_basher.c 
	$(COMPILE) -DRVM_USELWP -c rvm_basher.c && mv rvm_basher.o lwp_basher.o

