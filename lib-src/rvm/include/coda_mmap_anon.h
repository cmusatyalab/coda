/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#ifndef CODA_MMAP_ANON_H
#define CODA_MMAP_ANON_H

#include <unistd.h>
#include <sys/mman.h>

#ifndef MAP_ANON
#define MAP_ANON 0
#endif

#define mmap_anon(raddrptr, addrptr, len, prot)                    \
    do {                                                           \
        int fd = -1, flags = MAP_ANON | MAP_PRIVATE;               \
        if (addrptr)                                               \
            flags |= MAP_FIXED;                                    \
        if (!MAP_ANON)                                             \
            fd = open("/dev/zero", O_RDWR);                        \
        raddrptr = mmap((char *)addrptr, len, prot, flags, fd, 0); \
        if (fd != -1)                                              \
            close(fd);                                             \
    } while (0);

#endif /* CODA_MMAP_ANON_H */
