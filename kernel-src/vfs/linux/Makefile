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
CSBINS 	    =  coda-cache coda-debug coda-flush coda-resize coda-trace
EXECUTABLES =  coda.o
HEADERS     =  
OBJS        =  psdev.o cnode.o coda_linux.o upcall.o comm.o super.o inode.o file.o namecache.o sysctl.o 

DEFINES =  -DMODULE  -DL20 -DMODVERSIONS -Wstrict-prototypes -Wall -O2
INCL    =  -I/usr/src/linux/include  -include /usr/src/linux/include/linux/modversions.h
CFLAGS  := -D__KERNEL__ $(INCL)   $(DEFINES)  $(CFLAGS)

linux:
	ln -s $(INCLDIR) linux

coda: coda.o
	-touch coda.o

Coda: coda.o
	-touch coda.o

coda.o: linux $(OBJS)
	$(LD) -m "`ld --help | awk '/supported emulations/ {print $$4}'`" -r -o $@ $(OBJS)

include $(MAKDIR)/configs/Makerules

