#
# Makefile for the linux Coda-filesystem routines.
#

O_TARGET := coda.o
O_OBJS   := psdev.o cache.o cnode.o super.o dir.o file.o upcall.o coda_linux.o\
            symlink.o pioctl.o sysctl.o
M_OBJS   := $(O_TARGET)

# If you want debugging output, please uncomment the following line

# EXTRA_CFLAGS += -DDEBUG -DDEBUG_SMB_MALLOC=1

include $(TOPDIR)/Rules.make

