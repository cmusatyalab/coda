#
# Makefile for the Linux userfs-filesystem routines.
#
# Edit DEFINES below depending on your kernel version
#


BINDIR      =  /lib/modules/`uname -r`/fs
CBINS       =  coda.o
# CSBINS 	    =  coda-cache coda-debug coda-flush coda-resize coda-trace
EXECUTABLES =  coda.o
HEADERS     =  
OBJS        =  psdev.o cnode.o super.o inode.o file.o comm.o namecache.o sysctl.o 

DEFINES =  -DMODULE  -DMODVERSIONS -Wstrict-prototypes -Wall -O2
INCL    =  -I../common -I. -I/usr/src/linux/include  -include /usr/src/linux/include/linux/modversions.h
CFLAGS  := -D__KERNEL__ $(INCL)   $(DEFINES)  $(CFLAGS)


coda.o coda Coda : $(OBJS)
	$(LD) -m "`ld --help | awk '/supported emulations/ {print $$4}'`" -r -o $@ $(OBJS)

install: coda.o
	cp -p coda.o ${BINDIR}

clean:
	rm -f $(OBJS) coda.o
# include $(MAKDIR)/Makerules

