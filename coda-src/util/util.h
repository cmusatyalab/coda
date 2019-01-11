/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#include "coda_assert.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>

#ifndef IN /* rpc2.private.h also defines these */
/* Parameter usage */
#define IN /* Input parameter */
#define OUT /* Output parameter */
#define INOUT /* Obvious */
#endif /* !IN */

#define TRUE 1
#define FALSE 0

/* Useful functions in libutil.a */
int HashString(char *s, unsigned int size);
void eprint(const char *, ...);
void fdprint(long afd, const char *fmt, ...);

/* Routine for conditionally printing timestamped log messages */
extern void LogMsg(int msglevel, int debuglevel, FILE *fout, const char *fmt,
                   ...);
#define VLog(level, fmt, a...) LogMsg(level, VolDebugLevel, stdout, fmt, ##a)
#define SLog(level, fmt, a...) LogMsg(level, SrvDebugLevel, stdout, fmt, ##a)
#define DLog(level, fmt, a...) LogMsg(level, DirDebugLevel, stdout, fmt, ##a)
#define ALog(level, fmt, a...) LogMsg(level, VolDebugLevel, stdout, fmt, ##a)
#define CLog(level, fmt, a...) LogMsg(level, VolDebugLevel, stdout, fmt, ##a)

/* The routine that prints the timestamp */
extern void PrintTimeStamp(FILE *fout);

/* Hostname related utilities */
int UtilHostEq(const char *name1, const char *name2);
char *hostname(char *name);

/* Process releted utilities */
void UtilDetach();

/* Useful locking macros */
#define U_wlock(b) ObtainWriteLock(&((b)->lock))
#define U_rlock(b) ObtainReadLock(&((b)->lock))
#define U_wunlock(b) ReleaseWriteLock(&((b)->lock))
#define U_runlock(b) ReleaseReadLock(&((b)->lock))

/* Extern decls for variables used in Coda to control verbosity of
   messages from LogMsg(). Should these be here?
*/

extern int SrvDebugLevel; /* Server */
extern int VolDebugLevel; /* Vol package */
extern int DirDebugLevel; /* Dir package */
extern int AL_DebugLevel; /* ACL package */
extern int AuthDebugLevel; /* Auth package */

#ifdef __CYGWIN32__
#include <stdarg.h>
/* int vsnprintf(char *buf, size_t len, char *fmt, va_list ap); */
int snprintf(char *buf, size_t len, const char *fmt, ...);
long int gethostid(void);
#endif

#ifdef __cplusplus
}
#endif
