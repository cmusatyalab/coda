# Generated automatically from Makefile.in by configure.
#
# Makefile for the Linux userfs-filesystem routines.
#
# Edit DEFINES below depending on your kernel version
#

TOPDIR = ../../..
ifndef MAKDIR
MAKDIR = ../../..
endif
srcdir = .
TOPOBJ = /home/braam/coda-4.6.0

include $(MAKDIR)/Makeconf

INCLDIR     := $(INCLDIR)/cfs
BINDIR      =  $(prefix)/../lib/modules/`uname -r`/fs
CBINS       =  coda.o
CSBINS 	    =  ../linux/coda-debug ../linux/coda-trace
EXECUTABLES =  coda.o
OBJS        =  psdev.o cnode.o inode.o dir.o file.o upcall.o coda_linux.o symlink.o pioctl.o cache.o sysctl.o stats.o

DEFINES =   -O2 -D__SMP__ -Wall -Wstrict-prototypes  -fomit-frame-pointer  -pipe -fno-strength-reduce -m486 -malign-loops=2 -malign-jumps=2 -malign-functions=2 -DCPU=586
INCL    =  -I. -DMODULE  -DMODVERSIONS --include /usr/src/linux/include/linux/modversions.h
CFLAGS  := -D__KERNEL__   $(DEFINES)  $(INCL) 

linincl:
	-mkdir linux
	-ln -s $(INCLDIR)/coda.h linux/coda.h
	-ln -s $(INCLDIR)/coda_opstats.h linux/coda_opstats.h

coda: coda.o
	touch coda.o

Coda: coda.o
	touch coda.o

coda.o: linincl $(OBJS)
	$(LD) -m "`ld --help | awk '/supported emulations/ {print $$4}'`" -r -o $@ $(OBJS)

include $(MAKDIR)/configs/Makerules

