#ifndef _BLURB_
#define _BLURB_
#/*
#
#            Coda: an Experimental Distributed File System
#                             Release 4.0
#
#          Copyright (c) 1987-1996 Carnegie Mellon University
#                         All Rights Reserved
#
#Permission  to  use, copy, modify and distribute this software and its
#documentation is hereby granted,  provided  that  both  the  copyright
#notice  and  this  permission  notice  appear  in  all  copies  of the
#software, derivative works or  modified  versions,  and  any  portions
#thereof, and that both notices appear in supporting documentation, and
#that credit is given to Carnegie Mellon University  in  all  documents
#and publicity pertaining to direct or indirect use of this code or its
#derivatives.
#
#CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
#SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
#FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
#DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
#RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
#ANY DERIVATIVE WORK.
#
#Carnegie  Mellon  encourages  users  of  this  software  to return any
#improvements or extensions that  they  make,  and  to  grant  Carnegie
#Mellon the rights to redistribute these changes without encumbrance.
#*/
#
#static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/rp2gen/Makefile.612.cs,v 1.1 1996/11/22 19:08:40 braam Exp $";
#endif /*_BLURB_*/










# Machine type (since @cputype has vanished)
SYSTYPE = $$SYSTYPE

# Coda installation
DESTDIR = /afs/andrew.cmu.edu/scs/cs/612/$(SYSTYPE)

CC = cc
MAKE = make
BUILD = build
INSTALL = /usr/cs/etc/install -m 0755 -o cmu -g cmu -c

CFLAGS = -g -I.. -I$(DESTDIR)/include
VERSION=NDEBUG

all:
	build -f Makefile.real VERSION=$(VERSION) CFLAGS="${CFLAGS}" CC="$(CC)" CPU="$(SYSTYPE)" all

install:
	$(INSTALL) @$(SYSTYPE).$(VERSION)/rp2gen ${DESTDIR}/bin/rp2gen
