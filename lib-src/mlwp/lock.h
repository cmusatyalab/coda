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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/lib-src/mlwp/lock.h,v 4.1 1997/01/08 21:54:12 rvb Exp $";
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

/* all locks wait on excl_locked except for READ_LOCK, which waits on readers_reading */
struct Lock {
    unsigned char	wait_states;	/* type of lockers waiting */
    unsigned char	excl_locked;	/* anyone have boosted, shared or write lock? */
    unsigned char	readers_reading;	/* # readers actually with read locks */
    unsigned char	num_waiting;	/* probably need this soon */
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
static inline void ObtainReadLock(struct Lock *lock);
static inline void ObtainWriteLock(struct Lock *lock);
static inline void ObtainSharedLock(struct Lock *lock);
static inline void BoostSharedLock(struct Lock *lock);
static inline void UnboostSharedLock(struct Lock *lock);
static inline void ReleaseReadLock(struct Lock *lock);
static inline void ReleaseWriteLock(struct Lock *lock);
static inline void ReleaseSharedLock(struct Lock *lock);
static inline int LockWaiters(struct Lock *lock);
static inline int CheckLock(struct Lock *lock);
static inline int WriteLocked(struct Lock *lock);
extern void Lock_Init (struct Lock*);
extern void Lock_Obtain (struct Lock*, int);
extern void Lock_ReleaseR (struct Lock *);
extern void Lock_ReleaseW (struct Lock *);


/*  Previously these were macros. You can't stop a debugger on a macro, 
    so I changed them to inline functions.
*/

static inline void  ObtainReadLock(struct Lock *lock)
{
    if (!((lock)->excl_locked & WRITE_LOCK) && !(lock)->wait_states)
	(lock) -> readers_reading++;
    else
	Lock_Obtain(lock, READ_LOCK);
}

static inline void  ObtainWriteLock(struct Lock *lock)
{
    if (!(lock)->excl_locked && !(lock)->readers_reading)
	(lock) -> excl_locked = WRITE_LOCK;\
    else
	Lock_Obtain(lock, WRITE_LOCK);
}

static inline void  ObtainSharedLock(struct Lock *lock)
{
    if (!(lock)->excl_locked && !(lock)->wait_states)
	(lock) -> excl_locked = SHARED_LOCK;
    else
	Lock_Obtain(lock, SHARED_LOCK);
}

static inline void  BoostSharedLock(struct Lock *lock)
{
    if (!(lock)->readers_reading)
	(lock)->excl_locked = WRITE_LOCK;
    else
	Lock_Obtain(lock, BOOSTED_LOCK);
}

/* this must only be called with a WRITE or boosted SHARED lock! */
static inline void  UnboostSharedLock(struct Lock *lock)
{
    (lock)->excl_locked = SHARED_LOCK;
    if((lock)->wait_states)
	Lock_ReleaseR(lock);
}

static inline void  ReleaseReadLock(struct Lock *lock)
{
    if (!--(lock)->readers_reading && (lock)->wait_states)
	Lock_ReleaseW(lock) ; 
}

static inline void  ReleaseWriteLock(struct Lock *lock)
{
    (lock)->excl_locked &= ~WRITE_LOCK;
    if ((lock)->wait_states) 
	Lock_ReleaseR(lock);
}

/* can be used on shared or boosted (write) locks */
static inline void  ReleaseSharedLock(struct Lock *lock)
{
    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);
    if ((lock)->wait_states) 
	Lock_ReleaseR(lock);
}

/* I added this next macro to make sure it is safe to nuke a lock -- Mike K. */
static inline int  LockWaiters(struct Lock *lock)
{
    return ((int) ((lock)->num_waiting));
}

static inline int  CheckLock(struct Lock *lock)
{
    if ((lock)->excl_locked)
	return -1;
    else
	return (lock)->readers_reading;
}

static inline int  WriteLocked(struct Lock *lock)
{
    return ((lock)->excl_locked != 0);
}


#endif /* LWPLOCK_INCLUDED */
