#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/util.h,v 4.3 1997/10/23 19:25:02 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


/*
    Miscellany of useful things
    
*/

#ifdef __MACH__
#include <sysent.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif __cplusplus


#include <signal.h>
#include <assert.h> 
#include <stdio.h>

#ifndef IN  /* rpc2.private.h also defines these */
/* Parameter usage */
#define	IN	/* Input parameter */
#define OUT	/* Output parameter */
#define INOUT	/* Obvious */
#endif IN


#define TRUE 1
#define FALSE 0

#ifdef LWP
#define SystemError(y) (fprintf(stderr, "%d(%s): ", getpid(), LWP_ActiveProcess->name), perror(y))
#else
#define SystemError(y) (fprintf(stderr, "%d: ", getpid()), perror(y))
#endif

/* Useful functions in libutil.a */
/*extern int ffs(register int x);*/
extern int CaseFoldedCmp(char *s1, char *s2);

/* length-checked string routines: These used to be macros but C++ generates bogus
	code for comma-expressions in conditionals */
extern int SafeStrCat(char *dest, char *src, int totalspace);
extern int SafeStrCpy(char *dest, char *src, int totalspace);
void eprint(char *, ...);
void fdprint(long afd, char *fmt, ...);


/* Routine for conditionally printing timestamped log messages */
extern void LogMsg(int msglevel, int debuglevel, FILE *fout, char *fmt,  ...);

/* The routine that prints the timestamp */
extern void PrintTimeStamp(FILE *fout);

/* Hostname related utilities */
int UtilHostEq(char *name1, char *name2);
char *hostname(char *);

/* Useful locking macros */
#define U_wlock(b)     ObtainWriteLock(&((b)->lock))
#define U_rlock(b)    ObtainReadLock(&((b)->lock))
#define U_wunlock(b)    ReleaseWriteLock(&((b)->lock))
#define U_runlock(b)    ReleaseReadLock(&((b)->lock))

/* Extern decls for variables used in Coda to control verbosity of messages from LogMsg().
   These should probably be spread out in the individual header files for 
   various packages, like the variables themselves.  But that gets to be a 
   pain, and this is harmless anyway ..... */

extern int SrvDebugLevel;	/* Server */
extern int VolDebugLevel;	/* Vol package */
extern int SalvageDebugLevel;	/* Salvager */
extern int DirDebugLevel;	/* Dir package */
extern int AL_DebugLevel;	/* ACL package */
extern int AuthDebugLevel;	/* Auth package */


#ifdef __cplusplus
}
#endif __cplusplus
