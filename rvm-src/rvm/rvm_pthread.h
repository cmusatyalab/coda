#ifndef _BLURB_
#define _BLURB_
/*

     RVM: an Experimental Recoverable Virtual Memory Package
			Release 1.3

       Copyright (c) 1990-1994 Carnegie Mellon University
                      All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

RVM  IS  AN  EXPERIMENTAL  SOFTWARE  PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   Such improvements and extensions
should be returned to Software.Distribution@cs.cmu.edu.

*/

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./rvm-src/rvm/rvm_pthread.h,v 1.1 1996/11/22 19:16:55 braam Exp $";
#endif _BLURB_

#ifndef _RVM_PTHREAD_H_
#define _RVM_PTHREAD_H_

/* pthread compatibility for RVM */

#include <pthread.h>

/* used in pthread_create */
extern pthread_t rvm_pthreadid;

/* used in pthread_join */
extern pthread_addr_t *rvm_ptstat;
extern int       rvm_join_res;

#ifndef MACRO_BEGIN
#define MACRO_BEGIN                 do {
#define MACRO_END                   } while (0)
#endif  /* MACRO_BEGIN */

#define BOGUSCODE     (BOGUS_USE_OF_PTHREADS)   /* force compilation error */

#define RVM_MUTEX          pthread_mutex_t
#define RVM_MUTEX_T        pthread_mutex_t *

#define RVM_CONDITION      pthread_cond_t 
#define RVM_CONDIDITON_T   pthread_cond_t *

/* 
 * Unfortunately, pthread mutexes cannot be initialized statically: they
 * must be initialized by a call to pthread_mutex_init.  Oh well.
 * This means that some locking situations won't work properly.
 * I'll define MUTEX_INITIALIZER to be BOGUSCODE to make this more
 * explicit to pthreads clients.
 */

#define MUTEX_INITIALIZER  BOGUSCODE

/* Supported cthread definitions: */

#define cthread_t                    pthread_t *

#define cthread_fork(fname, arg)     (pthread_create(&rvm_pthreadid,       \
					 	     pthread_attr_default, \
						     (fname),              \
						     (arg)),               \
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

#define cthread_init()               (0)

#define cthread_exit(retval)         (pthread_exit((pthread_addr_t)(retval)))

#define cthread_yield()              (pthread_yield())

#define condition_wait(c,m)          (pthread_cond_wait((c),(m)))

#define condition_signal(c)          (pthread_cond_signal((c)))

#define condition_broadcast(c)       (pthread_cond_broadcast((c)))

/* This is defined just as in rvm_lwp.h, but is almost surely a bug */
#define condition_clear(c)           /* nop */

#define condition_init(c)            (pthread_cond_init((c),                  \
							pthread_attr_default))

#define mutex_init(m)                (pthread_mutex_init((m),                 \
							 pthread_attr_default))

/* 
 * cthreads.h on mach machines defines this exactly this way.  I have
 * no idea why
 */
#define mutex_clear(m)                /* nop */

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

#endif _RVM_PTHREAD_H_
