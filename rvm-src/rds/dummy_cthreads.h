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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rds/Attic/dummy_cthreads.h,v 4.1 1997/01/08 21:54:26 rvb Exp $";
#endif _BLURB_

/* dummy version of
 *
 * Definitions for the C Threads package.
 * (originally contributed by James W. O'Toole Jr., MIT)
 */

#ifndef	_CTHREADS_
#define	_CTHREADS_ 1

/*
 * C Threads package initialization.
 */

#define cthread_init()          0      /* nop */

/*
 * Mutex objects.
 */
typedef struct mutex { int x; } *mutex_t;

#define	MUTEX_INITIALIZER	{0}

#define	mutex_init(m)		((m)->x = 0)
#define	mutex_clear(m)		/* nop */

#define	mutex_lock(m)		((m)->x = 1)
#define mutex_try_lock(m)	((m)->x ? 0 : ((m)->x = 1))
#define mutex_wait_lock(m)	((m)->x = 1)
#define mutex_unlock(m)		((m)->x = 0)

/*
 * Condition variables.
 */
typedef struct condition { int x; } *condition_t;

#define	CONDITION_INITIALIZER		{0}

#define	condition_init(c)		0
#define	condition_clear(c)		0

#define	condition_signal(c) 		0

#define	condition_broadcast(c)		0

#define condition_wait(c,m)		1

/*
 * Threads.
 */
typedef int cthread;
typedef int cthread_t;

#define cthread_fork(func, arg)		(cthread_t)NULL

#define cthread_join(t)			0

#define cthread_yield()			0

#define cthread_exit(result)		exit(result)

#define cthread_self()	    	    	NULL
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


#endif	_CTHREADS_

