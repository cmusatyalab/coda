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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/lib-src/mlwp/Attic/lock.c,v 4.2 1997/02/26 16:04:57 rvb Exp $";
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


#include <stdio.h>
#ifdef	__BSD44__
#include <sys/time.h>
#endif
#include "lwp.h"
#include "lwp.private.h"
#include "lock.h"


void Lock_Init(lock)
    register struct Lock *lock;
{
    lock -> readers_reading = 0;
    lock -> excl_locked = 0;
    lock -> wait_states = 0;
    lock -> num_waiting = 0;
}

void Lock_Obtain(lock, how)
    register struct Lock *lock;
    int how;
{

    switch (how) {

	case READ_LOCK:		lock->num_waiting++;
				do {
				    lock -> wait_states |= READ_LOCK;
				    LWP_WaitProcess(&lock->readers_reading);
				} while (lock->excl_locked & WRITE_LOCK);
				lock->num_waiting--;
				lock->readers_reading++;
				break;

	case WRITE_LOCK:	lock->num_waiting++;
				do {
				    lock -> wait_states |= WRITE_LOCK;
				    LWP_WaitProcess(&lock->excl_locked);
				} while (lock->excl_locked || lock->readers_reading);
				lock->num_waiting--;
				lock->excl_locked = WRITE_LOCK;
				break;

	case SHARED_LOCK:	lock->num_waiting++;
				do {
				    lock->wait_states |= SHARED_LOCK;
				    LWP_WaitProcess(&lock->excl_locked);
				} while (lock->excl_locked);
				lock->num_waiting--;
				lock->excl_locked = SHARED_LOCK;
				break;

	case BOOSTED_LOCK:	lock->num_waiting++;
				do {
				    lock->wait_states |= WRITE_LOCK;
				    LWP_WaitProcess(&lock->excl_locked);
				} while (lock->readers_reading);
				lock->num_waiting--;
				lock->excl_locked = WRITE_LOCK;
				break;

	default:		printf("Can't happen, bad LOCK type: %d\n", how);
				abort();
    }
}

/* release a lock, giving preference to new readers */
void Lock_ReleaseR(lock)
    register struct Lock *lock;
{

    if (lock->wait_states & READ_LOCK) {
	lock->wait_states &= ~READ_LOCK;
	LWP_NoYieldSignal(&lock->readers_reading);
    }
    else {
	lock->wait_states &= ~EXCL_LOCKS;
	LWP_NoYieldSignal(&lock->excl_locked);
    }
}

/* release a lock, giving preference to new writers */
void Lock_ReleaseW(lock)
    register struct Lock *lock;
{

    if (lock->wait_states & EXCL_LOCKS) {
	lock->wait_states &= ~EXCL_LOCKS;
	LWP_NoYieldSignal(&lock->excl_locked);
    }
    else {
	lock->wait_states &= ~READ_LOCK;
	LWP_NoYieldSignal(&lock->readers_reading);
    }
}

