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


#include <stdio.h>
#include <assert.h>
#include "lwp.h"
#include "lwp.private.h"
#include "lock.h"

/* null out a lock */
void Lock_Init(register struct Lock *lock)
{
	lock -> readers_reading = 0;
	lock -> excl_locked = 0;
	lock -> wait_states = 0;
	lock -> num_waiting = 0;
	lock -> excl_locker = NULL;
}

void Lock_Obtain(register struct Lock *lock, int how)
{
	PROCESS me = LWP_ThisProcess();

	switch (how) {
	case READ_LOCK:
		lock->num_waiting++;
		do {
			lock -> wait_states |= READ_LOCK;
			LWP_WaitProcess(&lock->readers_reading);
		} while ((lock->excl_locker != me) && 
			 (lock->excl_locked & WRITE_LOCK));
		lock->num_waiting--;
		lock->readers_reading++;
		break;

	case WRITE_LOCK:
		lock->num_waiting++;
		do {
			lock -> wait_states |= WRITE_LOCK;
			LWP_WaitProcess(&lock->excl_locked);
		} while ( (lock->excl_locked  && lock->excl_locker != me)
			  || lock->readers_reading);
		lock->num_waiting--;
		lock->excl_locked = WRITE_LOCK;
		lock->excl_locker = me;
		break;

	case SHARED_LOCK:	
		lock->num_waiting++;
		do {
			lock->wait_states |= SHARED_LOCK;
			LWP_WaitProcess(&lock->excl_locked);
		} while (lock->excl_locked);
		lock->num_waiting--;
		lock->excl_locked = SHARED_LOCK;
		break;

#if 0
	case BOOSTED_LOCK:	
		lock->num_waiting++;
		do {
			lock->wait_states |= WRITE_LOCK;
			LWP_WaitProcess(&lock->excl_locked);
		} while (lock->readers_reading);
		lock->num_waiting--;
		lock->excl_locked = WRITE_LOCK;
		lock->excl_locker = me;
		break;
#endif

	default:
		fprintf(stderr, "Can't happen, bad LOCK type: %d\n", how);
		abort();
	}
}

/* release a lock, giving preference to new readers */
void Lock_ReleaseR(register struct Lock *lock)
{
	PROCESS me = LWP_ThisProcess();

	if (lock->excl_locked & WRITE_LOCK) {
		assert(lock->excl_locker == me);
		lock->excl_locker = NULL;
	}

	if (lock->wait_states & READ_LOCK) {
		lock->wait_states &= ~READ_LOCK;
		LWP_NoYieldSignal(&lock->readers_reading);
	} else {
		lock->wait_states &= ~EXCL_LOCKS;
		LWP_NoYieldSignal(&lock->excl_locked);
	}
}

/* release a lock, giving preference to new writers */
void Lock_ReleaseW(register struct Lock *lock)
{
	PROCESS me = LWP_ThisProcess();

	if (lock->excl_locked & WRITE_LOCK) {
		assert(lock->excl_locker == me);
		lock->excl_locker = NULL;
	}

	if (lock->wait_states & EXCL_LOCKS) {
		lock->wait_states &= ~EXCL_LOCKS;
		LWP_NoYieldSignal(&lock->excl_locked);
	} else {
		lock->wait_states &= ~READ_LOCK;
		LWP_NoYieldSignal(&lock->readers_reading);
	}
}


/*  Previously these were macros. You can't stop a debugger on a macro, 
    so I changed them to inline functions.
*/

void  ObtainReadLock(struct Lock *lock)
{
	PROCESS me = LWP_ThisProcess();

	if (!(lock->excl_locked & WRITE_LOCK) && !(lock)->wait_states) {
		lock->readers_reading++;
		return;
	}
	if ( (lock->excl_locked & WRITE_LOCK) && lock->excl_locker == me) {
		lock->readers_reading++;
		return;
	} else
		Lock_Obtain(lock, READ_LOCK);
}

void  ObtainWriteLock(struct Lock *lock)
{
	PROCESS me = LWP_ThisProcess();

	if (!(lock)->excl_locked && !(lock)->readers_reading) {
		lock->excl_locked = WRITE_LOCK;
		lock->excl_locker = me;
		return;
	} 
	if ( (lock->excl_locked & WRITE_LOCK)  && lock->excl_locker == me)
		return; 
	Lock_Obtain(lock, WRITE_LOCK);
	return;
}

void  ObtainSharedLock(struct Lock *lock)
{
    if (!(lock)->excl_locked && !(lock)->wait_states)
	    (lock) -> excl_locked = SHARED_LOCK;
    else
	    Lock_Obtain(lock, SHARED_LOCK);
}

void  ReleaseReadLock(struct Lock *lock)
{
    if (!--(lock)->readers_reading && (lock)->wait_states)
	Lock_ReleaseW(lock) ; 
}

void  ReleaseWriteLock(struct Lock *lock)
{
	if ((lock)->wait_states) 
		Lock_ReleaseR(lock);
	(lock)->excl_locked &= ~WRITE_LOCK;
}

/* can be used on shared or boosted (write) locks */
void  ReleaseSharedLock(struct Lock *lock)
{
	if ((lock)->wait_states) 
		Lock_ReleaseR(lock);
	(lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);
}

int  CheckLock(struct Lock *lock)
{
	if ((lock)->excl_locked)
		return -1;
	else
		return (lock)->readers_reading;
}

int  WriteLocked(struct Lock *lock)
{
	return ((lock)->excl_locked != 0);
}

#if 0
/* We don't use this stuff pjb */

int LockWaiters(struct Lock *lock);
void BoostSharedLock(struct Lock *lock);
void UnboostSharedLock(struct Lock *lock);

/* I added this next macro to make sure it is safe to nuke a lock -- Mike K. */
int  LockWaiters(struct Lock *lock)
{
	return ((int) ((lock)->num_waiting));
}

/* this next is not a flag, but rather a parameter to Lock_Obtain */
#define BOOSTED_LOCK 6


void  BoostSharedLock(struct Lock *lock)
{
    if (!(lock)->readers_reading)
	    (lock)->excl_locked = WRITE_LOCK;
    else
	    Lock_Obtain(lock, BOOSTED_LOCK);
}

/* this must only be called with a WRITE or boosted SHARED lock! */
void  UnboostSharedLock(struct Lock *lock)
{
	(lock)->excl_locked = SHARED_LOCK;
	if((lock)->wait_states)
		Lock_ReleaseR(lock);
}
#endif
