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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/lib-src/mlwp/lwp.h,v 4.2 1997/10/23 18:53:34 braam Exp $";
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

#ifndef LWP_INCLUDED
#define LWP_INCLUDED

#include <sys/time.h>


/*
    LWP_VERSION is first argument to LWP_Init().
    Ensures header matches library.
*/
#define LWP_VERSION  210888002  /* Change this if you make an incompatible change of some kind */

#define LWP_SUCCESS	0
#define LWP_EBADPID	-1
#define LWP_EBLOCKED	-2
#define LWP_EINIT	-3
#define LWP_EMAXPROC	-4
#define LWP_ENOBLOCK	-5
#define LWP_ENOMEM	-6
#define LWP_ENOPROCESS	-7
#define LWP_ENOWAIT	-8
#define LWP_EBADCOUNT	-9
#define LWP_EBADEVENT	-10
#define LWP_EBADPRI	-11
#define LWP_NO_STACK	-12
/* These two are for the signal mechanism. */
#define LWP_EBADSIG	-13		/* bad signal number */
#define LWP_ESYSTEM	-14		/* system call failed */
/* These are for the rock mechanism */
#define LWP_ENOROCKS	-15	/* all rocks are in use */
#define LWP_EBADROCK	-16	/* the specified rock does not exist */


/* Maximum priority permissible (minimum is always 0) */
#define LWP_MAX_PRIORITY 4

/* Usual priority used by user LWPs */
#define LWP_NORMAL_PRIORITY (LWP_MAX_PRIORITY-1)



/* Users aren't really supposed to know what a pcb is, but .....*/
typedef struct lwp_pcb *PROCESS;


extern char lwp_debug;			/* ON = show LWP debugging trace */

/* Action to take on stack overflow. */
#define LWP_SOQUIET	1		/* do nothing */
#define LWP_SOABORT	2		/* abort the program */
#define LWP_SOMESSAGE	3		/* print a message and be quiet */
extern int lwp_overflowAction;

/* Tells if stack size counting is enabled. */
extern int lwp_stackUseEnabled;

typedef int (*PFIC)(char *);
typedef void (*PFV)();


/* variables used for checking work time of an lwp */
extern struct timeval last_context_switch; /* how long a lwp was running */
extern struct timeval cont_sw_threshold;  /* how long a lwp is allowed to run */

extern struct timeval run_wait_threshold;

#ifndef __cplusplus
typedef int (*PFI) (char *);
#endif  __cplusplus


extern int LWP_QWait();
extern int LWP_QSignal (register PROCESS pid);
extern int LWP_Init (int version, int priority, PROCESS *pid);
extern int LWP_TerminateProcessSupport();
extern int LWP_CreateProcess (PFIC ep, int stacksize, int priority, char *parm, 
			     char *name, PROCESS *pid);
extern int LWP_CurrentProcess (PROCESS *pid);
PROCESS LWP_ThisProcess();
extern int LWP_DestroyProcess (PROCESS pid);
extern int LWP_DispatchProcess();
extern int LWP_GetProcessPriority (PROCESS pid, int *priority);
extern int LWP_INTERNALSIGNAL (char *event, int yield);
extern int LWP_WaitProcess (char *event);
extern int LWP_MwaitProcess (int wcount, char *evlist[]);
extern int LWP_StackUsed (PROCESS pid, int *max, int *used); 
extern int LWP_NewRock (int Tag, char *Value);
extern int LWP_GetRock (int Tag,  char **Value);
extern char *LWP_Name ();  /* NOTE: returs a char * !! */
extern int LWP_Index();
extern int LWP_HighestIndex();
extern void LWP_UnProtectStacks();	/* only available for newlwp */
extern void LWP_ProtectStacks();	

#define LWP_SignalProcess(event)	LWP_INTERNALSIGNAL(event, 1)
#define LWP_NoYieldSignal(event)	LWP_INTERNALSIGNAL(event, 0)


/* extern definitions for the io manager routines */
extern int IOMGR_SoftSig (PFIC aproc, char *arock);
extern int IOMGR_Initialize();
extern int IOMGR_Finalize();
extern int IOMGR_Poll();
extern int IOMGR_Select (register int fds, register int *readfds, register int *writefds, 
			register int *exceptfds, struct timeval *timeout);
extern int IOMGR_Cancel (register PROCESS pid);
extern int IOMGR_Signal (int signo, char *event);
extern int IOMGR_CancelSignal (int signo);

/* declarations for fasttime.c routines */
extern int FT_Init (int printErrors, int notReally);
#if	__GNUC__ >= 2
struct timezone;
#endif
extern int FT_GetTimeOfDay (struct timeval *tv, struct timezone *tz);
extern int TM_GetTimeOfDay (struct timeval *tv, struct timezone *tz);
extern int FT_AGetTimeOfDay (struct timeval *tv, struct timezone *tz);
extern unsigned int FT_ApproxTime() ;

#endif /* LWP_INCLUDED */



