/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

#ifndef CODA_MMAP_ANON_H
#define CODA_MMAP_ANON_H

#ifdef MAP_ANON
#define mmap_anon(raddrptr, addrptr, len, prot)	{ raddrptr = mmap(addrptr, len, prot, (MAP_PRIVATE | MAP_ANON), -1, 0); }
#else
#define mmap_anon(raddrptr, addrptr, len, prot)	{ int fd; \
						  if ((fd = open("/dev/zero", O_RDWR)) == -1) \
						      raddrptr = (char *)-1; \
						  else { \
						      raddrptr = mmap(addrptr, len, prot, (MAP_PRIVATE | (addrptr ? MAP_FIXED : 0)), fd, 0); \
						      (void) close(fd); \
						  } \
						}
#endif
#endif
