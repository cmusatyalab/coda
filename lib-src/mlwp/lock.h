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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./lib-src/mlwp/lock.h,v 1.1 1996/11/22 19:18:54 braam Exp $";
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

#ifndef _LWPLOCK_
#define _LWPLOCK_

#include "cargs.h"

/* The following macros allow multi statement macros to be defined safely, i.e.
   - the multi statement macro can be the object of an if statement;
   - the call to the multi statement macro may be legally followed by a semi-colon.
   BEGINMAC and ENDMAC have been tested with both the portable C compiler and
   Hi-C.  Both compilers were from the Palo Alto 4.2BSD software releases, and
   both optimized out the constant loop code.  For an example of the use
   of BEGINMAC and ENDMAC, see the definition for ReleaseWriteLock, below.
   An alternative to this, using "if(1)" for BEGINMAC is not used because it
   may generate worse code with pcc, and may generate warning messages with hi-C.
*/
#define BEGINMAC do {
#define ENDMAC   } while (0)

/* all locks wait on excl_locked except for READ_LOCK, which waits on readers_reading */
struct Lock {
    unsigned char	wait_states;	/* type of lockers waiting */
    unsigned char	excl_locked;	/* anyone have boosted, shared or write lock? */
    unsigned char	readers_reading;	/* # readers actually with read locks */
    unsigned char	num_waiting;	/* probably need this soon */
};

#define READ_LOCK	1
#define WRITE_LOCK	2
#define SHARED_LOCK	4
/* this next is not a flag, but rather a parameter to Lock_Obtain */
#define BOOSTED_LOCK 6

/* next defines wait_states for which we wait on excl_locked */
#define EXCL_LOCKS (WRITE_LOCK|SHARED_LOCK)

#define ObtainReadLock(lock)\
	if (!((lock)->excl_locked & WRITE_LOCK) && !(lock)->wait_states)\
	    (lock) -> readers_reading++;\
	else\
	    Lock_Obtain(lock, READ_LOCK)

#define ObtainWriteLock(lock)\
	if (!(lock)->excl_locked && !(lock)->readers_reading)\
	    (lock) -> excl_locked = WRITE_LOCK;\
	else\
	    Lock_Obtain(lock, WRITE_LOCK)

#define ObtainSharedLock(lock)\
	if (!(lock)->excl_locked && !(lock)->wait_states)\
	    (lock) -> excl_locked = SHARED_LOCK;\
	else\
	    Lock_Obtain(lock, SHARED_LOCK)

#define BoostSharedLock(lock)\
	if (!(lock)->readers_reading)\
	    (lock)->excl_locked = WRITE_LOCK;\
	else\
	    Lock_Obtain(lock, BOOSTED_LOCK)

/* this must only be called with a WRITE or boosted SHARED lock! */
#define UnboostSharedLock(lock)\
	BEGINMAC\
	    (lock)->excl_locked = SHARED_LOCK; \
	    if((lock)->wait_states) \
		Lock_ReleaseR(lock); \
	ENDMAC

#ifdef notdef
/* this is what UnboostSharedLock looked like before the hi-C compiler */
/* this must only be called with a WRITE or boosted SHARED lock! */
#define UnboostSharedLock(lock)\
	((lock)->excl_locked = SHARED_LOCK,\
	((lock)->wait_states ?\
		Lock_ReleaseR(lock) : 0))
#endif notdef

#define ReleaseReadLock(lock)\
	BEGINMAC\
	    if (!--(lock)->readers_reading && (lock)->wait_states)\
		Lock_ReleaseW(lock) ; \
	ENDMAC


#ifdef notdef
/* This is what the previous definition should be, but the hi-C compiler generates
  a warning for each invocation */
#define ReleaseReadLock(lock)\
	(!--(lock)->readers_reading && (lock)->wait_states ?\
		Lock_ReleaseW(lock)    :\
		0)
#endif notdef

#define ReleaseWriteLock(lock)\
        BEGINMAC\
	    (lock)->excl_locked &= ~WRITE_LOCK;\
	    if ((lock)->wait_states) Lock_ReleaseR(lock);\
        ENDMAC

#ifdef notdef
/* This is what the previous definition should be, but the hi-C compiler generates
   a warning for each invocation */
#define ReleaseWriteLock(lock)\
	((lock)->excl_locked &= ~WRITE_LOCK,\
	((lock)->wait_states ?\
		Lock_ReleaseR(lock) : 0))
#endif notdef	

/* can be used on shared or boosted (write) locks */
#define ReleaseSharedLock(lock)\
        BEGINMAC\
	    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);\
	    if ((lock)->wait_states) Lock_ReleaseR(lock);\
        ENDMAC

#ifdef notdef
/* This is what the previous definition should be, but the hi-C compiler generates
   a warning for each invocation */
/* can be used on shared or boosted (write) locks */
#define ReleaseSharedLock(lock)\
	((lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK),\
	((lock)->wait_states ?\
		Lock_ReleaseR(lock) : 0))
#endif notdef
	

/* I added this next macro to make sure it is safe to nuke a lock -- Mike K. */
#define LockWaiters(lock)\
	((int) ((lock)->num_waiting))

#define CheckLock(lock)\
	((lock)->excl_locked? (int) -1 : (int) (lock)->readers_reading)

#define WriteLocked(lock)\
	((lock)->excl_locked != 0)

/* extern definitions for lock manager routines */
extern void Lock_Init C_ARGS((struct Lock*));
extern void Lock_Obtain C_ARGS((struct Lock*, int));
extern void Lock_ReleaseR C_ARGS((struct Lock *));
extern void Lock_ReleaseW C_ARGS((struct Lock *));

#endif _LWPLOCK_
