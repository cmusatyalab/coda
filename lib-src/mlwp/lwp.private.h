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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/lib-src/mlwp/lwp.private.h,v 1.1.1.1 1996/11/22 19:19:01 rvb Exp";
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


#ifndef _LWP_PRIVATE_
#define _LWP_PRIVATE_

/* Merged header for OLD (Andrew-LWP) and NEW (cthread-LWP) versions of LWP.
   Default is NEW (use -llibnewlwp.a)
   Use of -DOLDLWP to turn on OLD (and use -liboldlwp.a)
*/

#include <sys/time.h>
#ifndef OLDLWP
#include    <cthreads.h>
#endif OLDLWP

/* Initial size of eventlist in a PCB; grows dynamically  */ 
#define EVINITSIZE  5


#ifdef OLDLWP
struct lwp_context {	/* saved context for dispatcher */
    char *topstack;	/* ptr to top of process stack */
};
#endif OLDLWP

struct rock
    {/* to hide things associated with this LWP under */
    int  tag;		/* unique identifier for this rock */
    char *value;	/* pointer to some arbitrary data structure */
    };

#define MAXROCKS	4	/* max no. of rocks per LWP */

struct lwp_pcb {			/* process control block */
  char		name[32];		/* ASCII name */
  int		rc;			/* most recent return code */
  char		status;			/* status flags */
  char		**eventlist;		/* ptr to array of eventids */
  char		eventlistsize;		/* size of eventlist array */
  int		eventcnt;		/* no. of events currently in eventlist array*/
  int		wakevent;		/* index of eventid causing wakeup */
  int		waitcnt;		/* min number of events awaited */
  char		blockflag;		/* if (blockflag), process blocked */
  int		priority;		/* dispatching priority */
  PROCESS	misc;			/* for LWP internal use only */
  char		*stack;			/* ptr to process stack */
  int		stacksize;		/* size of stack */
  long		stackcheck;		/* first word of stack for overflow checking */
  int		(*ep)C_ARGS((char *));	/* initial entry point */
  char		*parm;			/* initial parm for process */
  int		rused;			/* no of rocks presently in use */
  struct rock	rlist[MAXROCKS];	/* set of rocks to hide things under */
  PROCESS	next, prev;		/* ptrs to next and previous pcb */
  int		level;			/* nesting level of critical sections */
  struct IoRequest	*iomgrRequest;	/* request we're waiting for */
  int           index;                  /* LWP index: should be small index; actually is
                                           incremented on each lwp_create_process */ 
  struct timeval lastReady;		/* if ready, time placed in the run queue */

#ifdef OLDLWP
  struct lwp_context context;		/* saved context for next dispatch */
#else OLDLWP
  struct mutex  m;			/* mutex for the condition that process waits on */
  struct condition c;			/* condition that process waits on */
#endif OLDLWP
  };

extern int lwp_nextindex;                      /* Next lwp index to assign */


extern PROCESS	lwp_cpptr;		/* pointer to current process pcb */
struct	 lwp_ctl {			/* LWP control structure */
    int		processcnt;		/* number of lightweight processes */
    char	*outersp;		/* outermost stack pointer */
    PROCESS	outerpid;		/* process carved by Initialize */
    PROCESS	first, last;		/* ptrs to first and last pcbs */
    char	dsptchstack[800];	/* stack for dispatcher use only */
};

extern char PRE_Block;			/* used in preemption control (in preempt.c) */

#ifdef OLDLWP
/* Routines in process.s */
extern savecontext C_ARGS((PFV whichroutine, struct lwp_context *context, char *whichstack));
extern returnto C_ARGS((struct lwp_context *context));
#endif OLDLWP

/* Debugging macro */
#ifdef LWPDEBUG
#define lwpdebug(level, msg)\
	 if (lwp_debug > level) {\
	     printf("***LWP (0x%x): ", lwp_cpptr);\
	     printf msg;\
	     putchar('\n');\
	     fflush(stdout);\
	 }
#else LWPDEBUG
#define lwpdebug(level, msg)
#endif LWPDEBUG

#ifndef OLDLWP
typedef struct si {
    cthread_t	id;		/* this thread's id */
    vm_address_t address;	/* this thread's sp */
    vm_size_t	size;		/* size of the stack region */
    vm_prot_t	protection;	/* protection of the stack */
    vm_prot_t	maxprot;	/* max protection  on the stack */
} stackinfo;

#define MAXTHREADS	100
PRIVATE void InitVMInfo();
PRIVATE void InitMyStackInfo(char *);
 

#endif OLDLWP

#endif _LWP_PRIVATE_
