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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/lib-src/mlwp/Attic/lock.h,v 4.3 1998/04/14 20:42:21 braam Exp $";
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

#ifndef LWPLOCK_INCLUDED
#define LWPLOCK_INCLUDED

#include <lwp.h>
/* all locks wait on excl_locked except for READ_LOCK, which waits on readers_reading */
struct Lock {
    unsigned char   wait_states;	/* type of lockers waiting */
    unsigned char   excl_locked;	/* anyone have boosted, shared or write lock? */
    unsigned char   readers_reading;	/* # readers actually with read locks */
    unsigned char   num_waiting;	/* probably need this soon */
    PROCESS         excl_locker;
};

typedef struct Lock Lock;


#define READ_LOCK	1
#define WRITE_LOCK	2
#define SHARED_LOCK	4
/* this next is not a flag, but rather a parameter to Lock_Obtain */
#define BOOSTED_LOCK 6

/* next defines wait_states for which we wait on excl_locked */
#define EXCL_LOCKS (WRITE_LOCK|SHARED_LOCK)

/* extern definitions for lock manager routines */
void ObtainReadLock(struct Lock *lock);
void ObtainWriteLock(struct Lock *lock);
void ObtainSharedLock(struct Lock *lock);
void BoostSharedLock(struct Lock *lock);
void UnboostSharedLock(struct Lock *lock);
void ReleaseReadLock(struct Lock *lock);
void ReleaseWriteLock(struct Lock *lock);
void ReleaseSharedLock(struct Lock *lock);
int LockWaiters(struct Lock *lock);
int CheckLock(struct Lock *lock);
int WriteLocked(struct Lock *lock);
extern void Lock_Init (struct Lock*);
extern void Lock_Obtain (struct Lock*, int);
extern void Lock_ReleaseR (struct Lock *);
extern void Lock_ReleaseW (struct Lock *);

#endif /* LWPLOCK_INCLUDED */
