/* BLURB lgpl

                           Coda File System
                              Release 5

            Copyright (c) 1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
#*/

#include <pthread.h>
#include <assert.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include "lwp.private_pt.h"

/**** RW locks *****/
/* Well, actually Read/Shared/Write locks
 * Read lock   -- obtained when there are no (queued) write lockers.
 * 
 *                blocks all writers.
 *                 (shares an `exclusive lock' with other readers and at
 *                  most one shared locker)
 *                      
 * Shared lock -- obtained when there are no (queued) write lockers.
 * 
 *                blocks all others.
 *                 (shares the lock with readers present when it entered, but
 *                  sets excl to block access for new lockers)
 *                      
 * Write lock  -- obtained when there are no others holding the lock.
 *                      
 *                blocks all others.
 *                 (sets the excl flag to block access)
 */

void Lock_Init(struct Lock *lock)
{
    lock->initialized = 1;
    lock->readers     = 0;
    lock->excl        = NULL;
    pthread_mutex_init(&lock->_access, NULL);
    pthread_cond_init(&lock->wakeup, NULL);

    lwp_dbg(LWP_DBG_LOCKS, "I lock %p\n", lock)
}

static void ObtainLock(struct Lock *lock, char type)
{
    PROCESS pid;
    assert(LWP_CurrentProcess(&pid) == 0);

    if (!lock->initialized)
	Lock_Init(lock);

    lwp_LEAVE(pid);
    lwp_mutex_lock(&lock->_access);
    {
	/* now start waiting, writers wait until all readers have left, all
	 * lockers wait for the excl flag to be cleared */
	/* this is a safe cancellation point because we (should) only hold
	 * the access mutex, and we take ourselves off the pending list in the
	 * cleanup handler */
	while (lock->excl || (type == 'W' && lock->readers))
	    pthread_cond_wait(&lock->wakeup, &lock->_access);

	/* Obtain the correct lock flags, read locks increment readers, write
	 * locks set the excl flag and shared locks do both */
	if (type != 'R') lock->excl = pid;
	if (type != 'W') lock->readers++;

	    /* signal other threads, there might be more readers */
	if (type == 'R')
	    pthread_cond_broadcast(&lock->wakeup);

	lwp_dbg(LWP_DBG_LOCKS, "%c+ pid %p lock %p\n", type, pid, lock);
    }
    lwp_mutex_unlock(&lock->_access);
    lwp_YIELD(pid);
}

static void ReleaseLock(struct Lock *lock, char type)
{
    PROCESS pid;
    assert(LWP_CurrentProcess(&pid) == 0);

    /* acquire the lock-access mutex */
    lwp_mutex_lock(&lock->_access);

    if (type != 'R') {
        assert(lock->excl == pid);
        lock->excl = NULL;
    }
    if (type != 'W')
        lock->readers--;

    lwp_dbg(LWP_DBG_LOCKS, "%c- pid %p lock %p\n", type, pid, lock)

    /* if we cleared the lock, signal the next pending locker */
    if (!lock->excl && !lock->readers)
        pthread_cond_signal(&lock->wakeup);

    /* and release the lock-access mutex */
    lwp_mutex_unlock(&lock->_access);
}

void ObtainReadLock(struct Lock *lock)    { ObtainLock(lock, 'R'); }
void ObtainWriteLock(struct Lock *lock)   { ObtainLock(lock, 'W'); }
void ObtainSharedLock(struct Lock *lock)  { ObtainLock(lock, 'S'); }
void ReleaseReadLock(struct Lock *lock)   { ReleaseLock(lock, 'R'); }
void ReleaseWriteLock(struct Lock *lock)  { ReleaseLock(lock, 'W'); }
void ReleaseSharedLock(struct Lock *lock) { ReleaseLock(lock, 'S'); }

/* This function is silly anyway you look at it.
 * Think 2 concurrent threads trying to get a lock, that would use
 * this function to check if it is available to avoid blocking.
 * Accidents waiting to happen? */
int CheckLock(struct Lock *lock)
{
    if (lock->readers)     return lock->readers;
    if (WriteLocked(lock)) return -1;
    return 0;
}

/* What is the purpose here? to see whether there is any active
 * writer (0 readers, n writers), or any pending writers and or
 * active shared locks (n writers), or whether a shared lock has
 * become a write lock (1 reader, 1 writer)
 * The original code didn't give many clues, maybe the LWP docs do? */
int WriteLocked(struct Lock *lock)
{
    return (lock->excl != NULL);
}

