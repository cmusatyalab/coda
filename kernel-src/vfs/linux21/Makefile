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
OBJS        =  coda_linux.o psdev.o cache.o cnode.o inode.o dir.o file.o upcall.o symlink.o pioctl.o sysctl.o stats.o

DEFINES =  -DMODULE  -DMODVERSIONS -Wstrict-prototypes -Wall -O2
INCL    =  -I../common  -I/usr/src/linux/include  -I. -include /usr/src/linux/include/linux/modversions.h  
# CFLAGS  := -D__KERNEL__ $(INCL)   $(DEFINES)  $(CFLAGS)

CFLAGS =  -D__KERNEL__ -I../common -I. -I/usr/src/linux/include -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -D__SMP__ -pipe -fno-strength-reduce -m486 -malign-loops=2 -malign-jumps=2 -malign-functions=2 -DCPU=586 -D__SMP__ -DMODULE -DMODVERSIONS -include /usr/src/linux/include/linux/modversions.h 
coda:	coda.o
	echo  ""

Coda: coda.o
	echo ""

coda.o: $(OBJS)
	$(LD) -m "`ld --help | awk '/supported emulations/ {print $$4}'`" -r -o $@ $(OBJS)

install: coda.o
	cp -p coda.o ${BINDIR}
	depmod -a

clean:
	rm -f ${OBJS} coda.o

