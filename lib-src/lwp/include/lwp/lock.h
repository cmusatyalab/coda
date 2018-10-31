/* BLURB lgpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2018 Carnegie Mellon University
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

#ifndef _LWP_LOCK_H_
#define _LWP_LOCK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <lwp/lwp.h>

enum lock_how {
    READ_LOCK = 1,
    WRITE_LOCK = 2,
    SHARED_LOCK = 4
};

/* When compiling for pthreads, always compile with -D_REENTRANT (on glibc
 * systems) or -D_THREAD_SAFE and -pthread/-kthread (on FreeBSD) */
#if !defined(_REENTRANT) && !defined(_THREAD_SAFE)

/* all locks wait on excl_locked except for READ_LOCK, which waits on
 * readers_reading */
struct Lock {
	unsigned char   wait_states;	/* type of lockers waiting */
	unsigned char   excl_locked;	/* anyone have boosted, shared or write lock? */
	unsigned char   readers_reading;/* # readers actually with read locks */
	unsigned char   num_waiting;	/* probably need this soon */
	PROCESS         excl_locker;
};

/* next defines wait_states for which we wait on excl_locked */
#define EXCL_LOCKS (WRITE_LOCK|SHARED_LOCK)

void Lock_Obtain (struct Lock*, int);
void Lock_ReleaseR (struct Lock *);
void Lock_ReleaseW (struct Lock *);

#else /* _REENTRANT || _THREAD_SAFE */
#include <pthread.h>

struct Lock {
    char             initialized;
    char             readers;
    PROCESS          excl;
    pthread_mutex_t  _access;
    pthread_cond_t   wakeup;
};
#endif /* _REENTRANT || _THREAD_SAFE */

typedef struct Lock Lock;

/* extern definitions for lock manager routines */
void ObtainReadLock(struct Lock *lock);
void ObtainWriteLock(struct Lock *lock);
void ObtainSharedLock(struct Lock *lock);
void ReleaseReadLock(struct Lock *lock);
void ReleaseWriteLock(struct Lock *lock);
void ReleaseSharedLock(struct Lock *lock);
int CheckLock(struct Lock *lock);
int WriteLocked(struct Lock *lock);
void Lock_Init (struct Lock *lock);

/**
 * Safely obtain two simultaneous locks
 *
 * @param lock_1 pointer to the first lock
 * @param how_1  first lock's type
 * @param lock_2 pointer to the second lock
 * @param how_2  second lock's type
 */
void ObtainDualLock(register struct Lock *lock_1, enum lock_how how_1, register struct Lock *lock_2, enum lock_how how_2);

/**
 * Safely release two simultaneous locks
 *
 * @param lock_1 pointer to the first lock
 * @param how_1  first lock's type
 * @param lock_2 pointer to the second lock
 * @param how_2  second lock's type
 */
void ReleaseDualLock(register struct Lock *lock_1, enum lock_how how_1, register struct Lock *lock_2, enum lock_how how_2);

#ifdef __cplusplus
}
#endif

#endif /* _LWP_LOCK_H_ */

