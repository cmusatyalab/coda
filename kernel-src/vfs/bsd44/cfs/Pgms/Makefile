#/* 
# * Mach Operating System
# * Copyright (c) 1990 Carnegie-Mellon University
# * Copyright (c) 1989 Carnegie-Mellon University
# * All rights reserved.  The CMU software License Agreement specifies
# * the terms and conditions for use and redistribution.
# */
#
#/*
# * This code was written for the Coda file system at Carnegie Mellon University.
# * Contributers include David Steere, James Kistler, and M. Satyanarayanan.
# */

#
# TODO: Update header dependencies
#

# Which C compiler to use
CC = $(CC?$(CC):CC)
PLAINC = $(PLAINC?$(PLAINC):cc)
RP2GEN= ${BINDIR}/rp2gen

CFLAGS= $(CFLAGS) -I$(KERNELDIR)

# Identify files of interest

HFILES = cfs.h cnode.h cfs_opstats.h cfsio.h
CFILES = cfsmount.c cfsunmount.c pioctl.c cfsncstat.c

all: pioctl.o cfsmount cfsunmount cfsncstat

cfsmount: cfsmount.c
	$(PLAINC) $(CFLAGS) -o cfsmount cfsmount.c

cfsunmount: cfsunmount.c
	$(PLAINC) $(CFLAGS) -o cfsunmount cfsunmount.c

pioctl.o: pioctl.c
	$(CC) $(CFLAGS) -c pioctl.c

cfsncstat: cfsncstat.c
	$(PLAINC) $(CFLAGS) -o cfsncstat cfsncstat.c

clean:
	-rm -rf *.o cfsmount cfsunmount cfsncstat

#------------- Installation----------------
libinstall: all
	-cd $(LIBDIR); chmod 0644 pioctl.o 
	${INSTALL} pioctl.o $(PIOCTL_O)
	-cd $(BINDIR); chmod 0755 cfsmount cfsunmount cfsncstat
	${INSTALL} cfsmount cfsunmount cfsncstat ${BINDIR}


hdrinstall: ${HFILES}
	-mkdir ${INCLDIR}/cfs
	-chmod 0644 ${INCLDIR}/cfs/*
	${INSTALL} $> ${INCLDIR}/cfs

#dtcheaders: dtcreg.h dtc.h
#	-chmod 0644 ${INCLDIR}/cfs/*
#	${INSTALL} $> ${INCLDIR}

srcinstall:
# Manually setenv SUPPRESS_ALPHACI if you want to suppress the RCS checkin by alphaci
	SRCDIR=$(SRCDIR); export SRCDIR; $(ALPHACI)

install: libinstall hdrinstall srcinstall # dtcheaders /* skip dtcheaders for now */
