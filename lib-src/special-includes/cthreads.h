/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log: cthreads.h,v $
 * Revision 1.1  1996/11/22 19:19:06  braam
 * First Checkin (pre-release)
 *
 * Revision 1.8  90/11/02  16:46:37  mrt
 * 	Changed declaration of chtread_init from void to extern to agree
 * 	with the Mach 3.0 cthreads package.
 * 	[90/11/02            mrt]
 * 
 * Revision 1.7  90/08/17  21:39:09  mrt
 * 	Removed the debugging macros from here as they were conflicting
 * 	with things that other programs defined. Made definition
 * 	of extern int cthread_debug unconditional
 * 	[90/08/17            mrt]
 * 
 * Revision 1.6  89/08/22  11:24:26  mrt
 * 	Changed _STDC_ to __STDC__
 * 	[89/08/22            mrt]
 * 
 * Revision 1.5  89/05/05  18:48:41  mrt
 * 	Cleanup for Mach 2.5
 * 
 * 24-Mar-89  Michael Jones (mbj) at Carnegie-Mellon University
 *	Implement fork() for multi-threaded programs.  Added MACRO_BEGIN
 *	and MACRO_END for defining statement macros.  Modified all
 *	statement macros to use them.  The "if (1) { stmts } else" trick
 *	was too prone to eating the next statement when the terminating
 *	semicolon was omitted from the macro call.
 *
 * 28-Oct-88  Eric Cooper (ecc) at Carnegie Mellon University
 *	Implemented spin_lock() as test and test-and-set logic
 *	(using mutex_try_lock()) in sync.c.  Changed ((char *) 0)
 *	to 0, at Mike Jones's suggestion, and turned on ANSI-style
 *	declarations in either C++ or _STDC_.
 *
 * 29-Sep-88  Eric Cooper (ecc) at Carnegie Mellon University
 *	Changed NULL to ((char *) 0) to avoid dependency on <stdio.h>,
 *	at Alessandro Forin's suggestion.
 *
 * 08-Sep-88  Alessandro Forin (af) at Carnegie Mellon University
 *	Changed queue_t to cthread_queue_t and string_t to char *
 *	to avoid conflicts.
 *
 * 01-Apr-88  Eric Cooper (ecc) at Carnegie Mellon University
 *	Changed compound statement macros to use the
 *	do { ... } while (0) trick, so that they work
 *	in all statement contexts.
 *
 * 19-Feb-88  Eric Cooper (ecc) at Carnegie Mellon University
 *	Made spin_unlock() and mutex_unlock() into procedure calls
 *	rather than macros, so that even smart compilers can't reorder
 *	the clearing of the lock.  Suggested by Jeff Eppinger.
 *	Removed the now empty <machine>/cthreads.h.
 * 
 * 01-Dec-87  Eric Cooper (ecc) at Carnegie Mellon University
 *	Changed cthread_self() to mask the current SP to find
 *	the self pointer stored at the base of the stack.
 *
 * 22-Jul-87  Eric Cooper (ecc) at Carnegie Mellon University
 *	Fixed bugs in mutex_set_name and condition_set_name
 *	due to bad choice of macro formal parameter name.
 *
 * 21-Jul-87  Eric Cooper (ecc) at Carnegie Mellon University
 *	Moved #include <machine/cthreads.h> to avoid referring
 *	to types before they are declared (required by C++).
 *
 *  9-Jul-87  Michael Jones (mbj) at Carnegie Mellon University
 *	Added conditional type declarations for C++.
 *	Added _cthread_init_routine and _cthread_exit_routine variables
 *	for automatic initialization and finalization by crt0.
 */
/*
 * cthreads.h  by Eric Cooper
 *
 * Definitions for the C Threads package.
 */

#ifndef	_CTHREADS_
#define	_CTHREADS_ 1

#if	c_plusplus || __STDC__

#ifndef	C_ARG_DECLS
#define	C_ARG_DECLS(arglist)	arglist
#endif	not C_ARG_DECLS

typedef void *any_t;

#else	not (c_plusplus || __STDC__)

#ifndef	C_ARG_DECLS
#define	C_ARG_DECLS(arglist)	()
#endif	not C_ARG_DECLS

typedef char *any_t;

#endif	not (c_plusplus || __STDC__)

#include <mach.h>

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif	TRUE

#ifndef MACRO_BEGIN

#ifdef	lint
int	NEVER;
#else	lint
#define	NEVER 0
#endif	lint

#define	MACRO_BEGIN	do {
#define	MACRO_END	} while (NEVER)

#endif	MACRO_BEGIN

/*
 * C Threads package initialization.
 */
extern int cthread_init();

/*
 * Queues.
 */
typedef struct cthread_queue {
	struct cthread_queue_item *head;
	struct cthread_queue_item *tail;
} *cthread_queue_t;

typedef struct cthread_queue_item {
	struct cthread_queue_item *next;
} *cthread_queue_item_t;

#define	NO_QUEUE_ITEM	((cthread_queue_item_t) 0)

#define	QUEUE_INITIALIZER	{ NO_QUEUE_ITEM, NO_QUEUE_ITEM }

#define	cthread_queue_alloc()	((cthread_queue_t) calloc(1, sizeof(struct cthread_queue)))
#define	cthread_queue_init(q)	((q)->head = (q)->tail = 0)
#define	cthread_queue_free(q)	free((any_t) (q))

#define	cthread_queue_enq(q, x) \
	MACRO_BEGIN \
		(x)->next = 0; \
		if ((q)->tail == 0) \
			(q)->head = (cthread_queue_item_t) (x); \
		else \
			(q)->tail->next = (cthread_queue_item_t) (x); \
		(q)->tail = (cthread_queue_item_t) (x); \
	MACRO_END

#define	cthread_queue_preq(q, x) \
	MACRO_BEGIN \
		if ((q)->tail == 0) \
			(q)->tail = (cthread_queue_item_t) (x); \
		((cthread_queue_item_t) (x))->next = (q)->head; \
		(q)->head = (cthread_queue_item_t) (x); \
	MACRO_END

#define	cthread_queue_head(q, t)	((t) ((q)->head))

#define	cthread_queue_deq(q, t, x) \
	MACRO_BEGIN \
		if (((x) = (t) ((q)->head)) != 0 && \
		    ((q)->head = (cthread_queue_item_t) ((x)->next)) == 0) \
			(q)->tail = 0; \
	MACRO_END

#define	cthread_queue_map(q, t, f) \
	MACRO_BEGIN \
		register cthread_queue_item_t x, next; \
		for (x = (cthread_queue_item_t) ((q)->head); x != 0; x = next) { \
			next = x->next; \
			(*(f))((t) x); \
		} \
	MACRO_END

/*
 * Spin locks.
 */
extern void
spin_lock C_ARG_DECLS((int *p));

extern void
spin_unlock C_ARG_DECLS((int *p));

/*
 * Mutex objects.
 */
typedef struct mutex {
	int lock;
	char *name;
} *mutex_t;

#define	MUTEX_INITIALIZER	{ 0, 0 }

#define	mutex_alloc()		((mutex_t) calloc(1, sizeof(struct mutex)))
#define	mutex_init(m)		((m)->lock = 0)
#define	mutex_set_name(m, x)	((m)->name = (x))
#define	mutex_name(m)		((m)->name != 0 ? (m)->name : "?")
#define	mutex_clear(m)		/* nop */
#define	mutex_free(m)		free((any_t) (m))

#define	mutex_lock(m) \
	MACRO_BEGIN \
		if (! mutex_try_lock(m)) mutex_wait_lock(m); \
	MACRO_END

extern int
mutex_try_lock C_ARG_DECLS((mutex_t m));	/* nonblocking */

extern void
mutex_wait_lock C_ARG_DECLS((mutex_t m));	/* blocking */

extern void
mutex_unlock C_ARG_DECLS((mutex_t m));

/*
 * Condition variables.
 */
typedef struct condition {
	int lock;
	struct cthread_queue queue;
	char *name;
} *condition_t;

#define	CONDITION_INITIALIZER		{ 0, QUEUE_INITIALIZER, 0 }

#define	condition_alloc()		((condition_t) calloc(1, sizeof(struct condition)))
#define	condition_init(c)		MACRO_BEGIN (c)->lock = 0; cthread_queue_init(&(c)->queue); MACRO_END
#define	condition_set_name(c, x)	((c)->name = (x))
#define	condition_name(c)		((c)->name != 0 ? (c)->name : "?")
#define	condition_clear(c)		MACRO_BEGIN condition_broadcast(c); spin_lock(&(c)->lock); MACRO_END
#define	condition_free(c)		MACRO_BEGIN condition_clear(c); free((any_t) (c)); MACRO_END

#define	condition_signal(c) \
	MACRO_BEGIN \
		if ((c)->queue.head) cond_signal(c); \
	MACRO_END

#define	condition_broadcast(c) \
	MACRO_BEGIN \
		if ((c)->queue.head) cond_broadcast(c); \
	MACRO_END

extern void
cond_signal C_ARG_DECLS((condition_t c));

extern void
cond_broadcast C_ARG_DECLS((condition_t c));

extern void
condition_wait C_ARG_DECLS((condition_t c, mutex_t m));

/*
 * Threads.
 */

typedef any_t (*cthread_fn_t) C_ARG_DECLS((any_t arg));

#include <setjmp.h>

typedef struct cthread {
	struct cthread *next;
	struct mutex lock;
	struct condition done;
	int state;
	jmp_buf catchme;
	cthread_fn_t func;
	any_t arg;
	any_t result;
	char *name;
	any_t data;
} *cthread_t;

#define	NO_CTHREAD	((cthread_t) 0)

extern cthread_t
cthread_fork C_ARG_DECLS((cthread_fn_t func, any_t arg));

extern void
cthread_detach C_ARG_DECLS((cthread_t t));

extern any_t
cthread_join C_ARG_DECLS((cthread_t t));

extern void
cthread_yield();

extern void
cthread_exit C_ARG_DECLS((any_t result));

/*
 * This structure must agree with struct cproc in cthread_internals.h
 */
typedef struct ur_cthread {
	struct ur_cthread *next;
	cthread_t incarnation;
} *ur_cthread_t;

extern int
cthread_sp();

extern int cthread_stack_mask;

#define	ur_cthread_self()	(* (ur_cthread_t *) (cthread_sp() & cthread_stack_mask))
#define	cthread_assoc(id, t)	(((ur_cthread_t) (id))->incarnation = (t))
#define	cthread_self()		(ur_cthread_self()->incarnation)

extern void
cthread_set_name C_ARG_DECLS((cthread_t t, char *name));

extern char *
cthread_name C_ARG_DECLS((cthread_t t));

extern int
cthread_count();

extern void
cthread_set_limit C_ARG_DECLS((int n));

extern int
cthread_limit();

#define	cthread_set_data(t, x)	((t)->data = (x))
#define	cthread_data(t)		((t)->data)

extern int
fork();

/*
 * Debugging support.
 */

extern int cthread_debug;

#endif	_CTHREADS_
