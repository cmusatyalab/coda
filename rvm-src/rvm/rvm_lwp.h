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
                           none currently

#*/

/* LWP compatability for RVM */

#include <lwp.h>
#include <lock.h>

#ifdef RVM_LWPPID
extern PROCESS rvm_lwppid;
#else  RVM_LWPPID
#define RVM_LWPPID
PROCESS                     rvm_lwppid;     /* LWP process id */
#endif RVM_LWPPID

#ifndef MACRO_BEGIN
#define MACRO_BEGIN			do {
#define MACRO_END			} while(0)
#endif MACRO_BEGIN

#define STACKSIZE	1024 * 16
#define BOGUSCODE 	(BOGUS_USE_OF_CTHREADS)  /* force compilation error */

#define RVM_MUTEX			struct Lock
#define RVM_MUTEX_T			struct Lock *
#define RVM_CONDITION			char 
#define RVM_CONDITION_T			char *
#define	MUTEX_INITIALIZER		{0, 0, 0, 0}
/* Supported cthread definitions */

#define cthread_t			PROCESS
#define cthread_fork(fname, arg)	(LWP_CreateProcess((fname), STACKSIZE, \
					  LWP_NORMAL_PRIORITY,	\
					  (char *)arg, 		\
					  (char *)"rvm_thread",	\
					  &rvm_lwppid), \
                                         rvm_lwppid)
#define cthread_join(foo)		(0)
#define cthread_init()			MACRO_BEGIN \
                                          LWP_Init(LWP_VERSION, \
                                                   LWP_NORMAL_PRIORITY, \
                                                   &rvm_lwppid); \
                                          IOMGR_Initialize(); \
                                        MACRO_END
#define cthread_exit(retval)		return
#define cthread_yield()			MACRO_BEGIN \
                                          IOMGR_Poll(); \
                                          LWP_DispatchProcess(); \
                                        MACRO_END
#define condition_wait(c, m)		MACRO_BEGIN \
                                          ReleaseWriteLock((m)); \
					  LWP_WaitProcess((c)); \
					  ObtainWriteLock((m)); \
                                        MACRO_END
#define condition_signal(c)		(LWP_SignalProcess((c)))
#define condition_broadcast(c)		(LWP_SignalProcess((c)))
#define condition_clear(c)		/* nop  */
#define condition_init(c)		/* nop */
#define mutex_init(m)			(Lock_Init(m))
#define mutex_clear(m)			/* nop */
#define LOCK_FREE(m)			(!WriteLocked(&(m)))
#define cthread_self() \
    (LWP_CurrentProcess(&rvm_lwppid), rvm_lwppid)
/* synchronization tracing definitions of lock/unlock */

#ifdef DEBUGRVM
#define mutex_lock(m)			MACRO_BEGIN \
                                         printf("mutex_lock OL(0x%x)%s:%d...", \
                                          (m), __FILE__, __LINE__); \
                                         ObtainWriteLock((m)); \
                                         printf("done\n"); \
                                        MACRO_END
#define mutex_unlock(m)			MACRO_BEGIN \
                                         printf("mutex_unlock RL(0x%x)%s:%d...",\
                                          (m), __FILE__, __LINE__); \
                                         ReleaseWriteLock((m)); \
                                         printf("done\n"); \
                                        MACRO_END
#else DEBUGRVM
#define mutex_lock(m)			ObtainWriteLock((m))
#define mutex_unlock(m)			ReleaseWriteLock((m))
#endif
/* Unsupported cthread calls */

#define	mutex_alloc()			BOGUSCODE
#define	mutex_set_name(m, x)		BOGUSCODE
#define	mutex_name(m)			BOGUSCODE
#define	mutex_free(m)			BOGUSCODE

#define	condition_alloc()		BOGUSCODE
#define	condition_set_name(c, x)	BOGUSCODE
#define	condition_name(c)		BOGUSCODE
#define	condition_free(c)		BOGUSCODE

#define cthread_detach()		BOGUSCODE
#define cthread_sp()			BOGUSCODE
#define	cthread_assoc(id, t)		BOGUSCODE
#define cthread_set_name		BOGUSCODE
#define cthread_name			BOGUSCODE
#define cthread_count()			BOGUSCODE
#define cthread_set_limit		BOGUSCODE
#define cthread_limit()			BOGUSCODE
#define	cthread_set_data(t, x)		BOGUSCODE
#define	cthread_data(t)			BOGUSCODE
