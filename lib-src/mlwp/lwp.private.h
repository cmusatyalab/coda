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


#ifndef _LWP_PRIVATE_
#define _LWP_PRIVATE_

/* Merged header for OLD (Andrew-LWP) and NEW (cthread-LWP) versions of LWP.
   Default is NEW (use -llibnewlwp.a)
   Use of -DOLDLWP to turn on OLD (and use -liboldlwp.a)
*/

#include <sys/time.h>
#include <stdio.h>
#ifndef OLDLWP
#include    <cthreads.h>
#endif OLDLWP

/* Initial size of eventlist in a PCB; grows dynamically  */ 
#define EVINITSIZE  5


#ifdef OLDLWP
#if defined(__powerpc__)
struct lwp_context {	/* saved context for dispatcher */
    char *topstack;	/* ptr to top of process stack */
    char *returnadd;	/* return address ? */

    char *ccr;		/* Condition code register */
};
#define STACK_PAD 64	/* make stacks 16 byte aligned and leave space for
			   silly LinuxPPC linkage, or we segfault entering
			   functions --troy */
#else
struct lwp_context {    /* saved context for dispatcher */
    char *topstack;     /* ptr to top of process stack */
};
#define STACK_PAD 4
#endif defined(__powerpc__)
#endif OLDLWP

struct rock
    {/* to hide things associated with this LWP under */
    int  tag;		/* unique identifier for this rock */
    char *value;	/* pointer to some arbitrary data structure */
    };

#define MAXROCKS	8	/* max no. of rocks per LWP */

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
  int		(*ep)(char *);	/* initial entry point */
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

extern int savecontext (PFV whichroutine, struct lwp_context *context, char *whichstack);
extern int returnto (struct lwp_context *context);
#endif OLDLWP

/* Debugging macro */
#ifdef LWPDEBUG
extern FILE *lwp_logfile;
#define lwpdebug(level, msg...)\
	 if (lwp_debug > level && lwp_logfile) {\
	     fprintf(lwp_logfile, "***LWP (%p): ", lwp_cpptr);\
	     fprintf(lwp_logfile, ## msg);\
	     fprintf(lwp_logfile, "\n");\
	     fflush(lwp_logfile);\
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
static void InitVMInfo();
static void InitMyStackInfo(char *);
 

#endif OLDLWP

#endif _LWP_PRIVATE_
