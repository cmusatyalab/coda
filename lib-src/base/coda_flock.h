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

#ifndef CODA_FLOCK_H
#define CODA_FLOCK_H

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

extern int myflock(int fd, int type, int block);

#ifdef HAVE_FLOCK
#define MYFLOCK_UN	LOCK_UN
#define MYFLOCK_SH	LOCK_SH
#define MYFLOCK_EX	LOCK_EX
#define MYFLOCK_NB	LOCK_NB
#define MYFLOCK_BL	0
#else	/* HAVE_FCNTL */
#define MYFLOCK_UN	F_UNLCK
#define MYFLOCK_SH	F_RDLCK
#define MYFLOCK_EX	F_WRLCK
#define MYFLOCK_NB	F_SETLK
#define MYFLOCK_BL	F_SETLKW
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#endif
