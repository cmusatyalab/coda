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

/* dummy version of
 *
 * Definitions for the C Threads package.
 * (originally contributed by James W. O'Toole Jr., MIT)
 */

#ifndef _CTHREADS_
#define _CTHREADS_ 1

/*
 * C Threads package initialization.
 */

#define cthread_init() \
    {                  \
    } /* nop */

/*
 * Mutex objects.
 */
#ifdef sun
#define mutex ct_mutex
#endif
typedef struct mutex {
    int x;
} * mutex_t;

#define MUTEX_INITIALIZER \
    {                     \
        0                 \
    }

#define mutex_init(m) ((m)->x = 0)
#define mutex_clear(m) /* nop */

#define mutex_lock(m) ((m)->x = 1)
#define mutex_try_lock(m) ((m)->x ? 0 : ((m)->x = 1))
#define mutex_wait_lock(m) ((m)->x = 1)
#define mutex_unlock(m) ((m)->x = 0)

/*
 * Condition variables.
 */
typedef struct condition {
    int x;
} * condition_t;

#define CONDITION_INITIALIZER \
    {                         \
        0                     \
    }

#define condition_init(c) \
    {                     \
    }
#define condition_clear(c) \
    {                      \
    }

#define condition_signal(c) \
    {                       \
    }

#define condition_broadcast(c) \
    {                          \
    }

#define condition_wait(c, m) \
    {                        \
    }

/*
 * Threads.
 */
typedef int cthread;

/* What should be type of cthread_t ?
 * In rvm_lwp.h, it is type (PROCESS)   (eq. to (struct lwp_pcb *)),  
 * In rvm_pthread.h, it is type (pthread_t *),
 * Here, I leave it untouch as type (int) but we may need to modify this
 * in future.  -- 3/18/97 Clement
 */
typedef long cthread_t;

#define cthread_fork(func, arg) (cthread_t) NULL

#define cthread_join(t) 0

#define cthread_yield() \
    {                   \
    }

#define cthread_exit(result) exit(result)

#define cthread_self() (cthread_t) NULL
/* Unsupported cthread calls */

#define mutex_alloc() BOGUSCODE
#define mutex_set_name(m, x) BOGUSCODE
#define mutex_name(m) BOGUSCODE
#define mutex_free(m) BOGUSCODE

#define condition_alloc() BOGUSCODE
#define condition_set_name(c, x) BOGUSCODE
#define condition_name(c) BOGUSCODE
#define condition_free(c) BOGUSCODE

#define cthread_detach() BOGUSCODE
#define cthread_sp() BOGUSCODE
#define cthread_assoc(id, t) BOGUSCODE
#define cthread_set_name BOGUSCODE
#define cthread_name BOGUSCODE
#define cthread_count() BOGUSCODE
#define cthread_set_limit BOGUSCODE
#define cthread_limit() BOGUSCODE
#define cthread_set_data(t, x) BOGUSCODE
#define cthread_data(t) BOGUSCODE

#endif /* _CTHREADS_ */
