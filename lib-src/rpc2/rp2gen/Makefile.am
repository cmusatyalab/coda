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

CC=${NATIVECC}
EXEEXT=
YFLAGS=-d
INCLUDES = -I$(top_builddir)/include -I$(top_srcdir)/include

bin_PROGRAMS = rp2gen

rp2gen_SOURCES = rp2main.c rp2gen_parser.y rp2gen_lexer.l symtab.c crout.c \
    util.c rp2.h rp2gen_parser.h

CLEANFILES = rp2gen_parser.c rp2gen_parser.h rp2gen_lexer.c
MAINTAINERCLEANFILES = Makefile.in

rp2gen_parser.h: rp2gen_parser.c
