/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2010 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _RVM_PTHREAD_H_
#define _RVM_PTHREAD_H_

/* pthread compatibility for RVM */

#include <pthread.h>

/* used in pthread_create */
extern pthread_t rvm_pthreadid;

/* used in pthread_join */
extern void *rvm_ptstat;
extern int       rvm_join_res;

#ifndef MACRO_BEGIN
#define MACRO_BEGIN                 do {
#define MACRO_END                   } while (0)
#endif  /* MACRO_BEGIN */

#define BOGUSCODE     (BOGUS_USE_OF_PTHREADS)   /* force compilation error */

#define RVM_MUTEX          pthread_mutex_t
#define RVM_CONDITION      pthread_cond_t 

/* 
 * Unfortunately, pthread mutexes cannot be initialized statically: they
 * must be initialized by a call to pthread_mutex_init.  Oh well.
 * This means that some locking situations won't work properly.
 * I'll define MUTEX_INITIALIZER to be BOGUSCODE to make this more
 * explicit to pthreads clients.
 */

/* That's nonsense, the following is from pthread_mutex(3):
 * 
 *    Variables of type pthread_mutex_t can also be initialized statically
 *    using the constants PTHREAD_MUTEX_INITIALIZER (for fast mutexes),
 *    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP (for recursive mutexes), and
 *    PTHREADS_ERRORCHECK_MUTEX_INITIALIZER_MP (for error checking mutexes).
 *    
 * --JH
 */

#define MUTEX_INITIALIZER  PTHREAD_MUTEX_INITIALIZER

/* Supported cthread definitions: */

#define cthread_t                    pthread_t *

#define cthread_fork(fname, arg)     (pthread_create(&rvm_pthreadid, NULL, \
						     (void *(*)(void*))(fname),\
						     (arg)), \
				      &rvm_pthreadid)

/* 
 * Returns either NULL or the address of the pthread_status block.
 * Unfortunately, it appears that chtread_join didn't have a way of
 * saying "badthread, can't do it," so I'm not sure of the best way to
 * do this.
 */
#define cthread_join(t)              (rvm_join_res =                      \
                                         pthread_join(*(t),&rvm_ptstat),  \
                                      (rvm_join_res) ? NULL : rvm_ptstat)

#define cthread_init()               do {} while(0)

#define cthread_exit(retval)         (pthread_exit((void *)(retval)))

#define cthread_yield()              do {} while(0)

#define condition_wait(c,m)          (pthread_cond_wait((c),(m)))

#define condition_signal(c)          (pthread_cond_signal((c)))

#define condition_broadcast(c)       (pthread_cond_broadcast((c)))

/* This is defined just as in rvm_lwp.h, but is almost surely a bug */
#define condition_clear(c)           /* nop */

#define condition_init(c)            (pthread_cond_init((c), NULL))

#define mutex_init(m)                (pthread_mutex_init((m), NULL))
#define mutex_clear(m)               (pthread_mutex_destroy(m), NULL)

/* This doesn't work for some reason... */
/*
#define LOCK_FREE(m)         (rvm_ptlocked = pthread_mutex_trylock(&(m)),     \
                              if (rvm_ptlocked) {pthread_mutex_unlock(&(m))}, \
			      rvm_ptlocked)
*/
/* defined in rvm_pthread.c */
extern int rvm_lock_free(pthread_mutex_t *m);
#define LOCK_FREE(m)         (rvm_lock_free(&(m)))

#define cthread_self()       (rvm_pthreadid = pthread_self(), &rvm_pthreadid)

#ifdef  DEBUGRVM
#define mutex_lock(m)        MACRO_BEGIN                             \
                               printf("mutex_lock OL(0x%x)%s:%d...", \
                                      (m), __FILE__, __LINE__);      \
                               pthread_mutex_lock((m));              \
                               printf("done\n");                     \
                             MACRO_END
#define mutex_unlock(m)      MACRO_BEGIN                               \
                               printf("mutex_unlock RL(0x%x)%s:%d...", \
                                       (m), __FILE__, __LINE__);       \
                               pthread_mutex_unlock((m));              \
                               printf("done\n");                       \
                             MACRO_END
#else   /* DEBUGRVM */
#define mutex_lock(m)        (pthread_mutex_lock((m)))
#define mutex_unlock(m)      (pthread_mutex_unlock((m)))
#endif  /* DEBUGRVM */


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

#endif /* _RVM_PTHREAD_H_ */
