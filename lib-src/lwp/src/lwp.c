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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>

#include <sys/param.h>
#include <sys/mman.h>
#include <assert.h>

/* for netbsd 1.3 and libc5 */
#ifndef	MAP_FAILED
#define MAP_FAILED      ((void *) -1)
#endif

#include <lwp/lwp.h>
#include "lwp.private.h"
#include "lwp_ucontext.h"
#include "lwp_stacktrace.h"

#define  ON	    1
#define  OFF	    0
#define  READY	    2
#define  WAITING    3
#define  MINSTACK   44
#ifndef MAX
#define  MAX(a,b)   ((a) > (b) ? (a) : (b))
#endif
#define	 LWPANCHOR  (*lwp_init)
#define	 MAX_PRIORITIES	(LWP_MAX_PRIORITY+1)

/* simple test, we normally want to mmap our stacks whenever possible, except
 * when we compile venus for Win95 */
#undef MMAP_LWP_STACKS
#if defined(HAVE_MMAP) && !defined(DJGPP) 
#define MMAP_LWP_STACKS 1

#if defined(__NetBSD__) || defined(__FreeBSD__)
char *lwp_stackbase = (char *)0x45000000;
#else
char *lwp_stackbase = (char *)0x15000000;
#endif
#endif /* MMAP_LWP_STACKS */

struct QUEUE {
    PROCESS	head;
    int		count;
} runnable[MAX_PRIORITIES], blocked;

static ucontext_t reaper; /* reaper context, see comments for lwp_Reaper() */
static ucontext_t tracer; /* context for the for stack tracing thread */

/* Invariant for runnable queues: The head of each queue points to the
currently running process if it is in that queue, or it points to the
next process in that queue that should run. */

/* internal procedure declarations */
static void lwpremove(PROCESS p, struct QUEUE *q);
static void lwpinsert(PROCESS p, struct QUEUE *q);
static void lwpmove(PROCESS p, struct QUEUE *from, struct QUEUE *to);
static void Initialize_PCB (PROCESS temp, int priority, char *stack, int stacksize, void (*func)(void *), void *arg, char *name);
static void Abort_LWP(char *msg);
static void Exit_LWP();
static void Free_PCB(PROCESS pid);

static void Overflow_Complain (void);
static void Initialize_Stack (void *stackptr, int stacksize);
static int  Stack_Used (stack_t *stack);

/*----------------------------------------*/
/* Globals identical in  OLD and NEW lwps */
/*----------------------------------------*/

FILE   *lwp_logfile = NULL;
int     lwp_debug = 0;
int 	LWP_TraceProcesses = 0;
PROCESS	lwp_cpptr;
int lwp_nextindex;		    /* Next lwp index to assign */
static struct lwp_ctl *lwp_init = NULL;
int	Cont_Sws;
struct timeval last_context_switch; /* used to find out how long a lwp was running */
struct timeval cont_sw_threshold; /* how long a lwp is allowed to run */
PROCESS cont_sw_id;		  /* id of thread setting the last_context_switch time */

struct timeval run_wait_threshold;

/*-----------------------------------------*/
/* Globals that differ in OLD and NEW lwps */
/*-----------------------------------------*/

int lwp_overflowAction = LWP_SOABORT;	/* Stack checking action */
int lwp_stackUseEnabled = 1;		/* Controls stack size counting */

/* fixing up some missing funtions */
#ifndef timercmp
#define timercmp(tvp, uvp, cmp) \
        ((tvp)->tv_sec cmp (uvp)->tv_sec || \
         (tvp)->tv_sec == (uvp)->tv_sec && (tvp)->tv_usec cmp (uvp)->tv_usec)
#endif
#ifndef timerisset
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
#endif
#ifndef timerclear
#define timerclear(tvp) do {(tvp)->tv_sec=0; (tvp)->tv_usec=0; } while(0);
#endif

/* Iterator macro */
#define for_all_elts(var, q, body)\
	{\
	    PROCESS var, _NEXT_;\
	    int _I_;\
	    for (_I_=q.count, var = q.head; _I_>0; _I_--, var=_NEXT_) {\
		_NEXT_ = var -> next;\
		body\
	    }\
	}

/* removes PROCESS p from a QUEUE pointed at by q */
static void lwpremove(PROCESS p, struct QUEUE *q)
{
    /* Special test for only element on queue */
    if (q->count == 1)
	q -> head = NULL;
    else {
	/* Not only element, do normal remove */
	p -> next -> prev = p -> prev;
	p -> prev -> next = p -> next;
    }
    /* See if head pointing to this element */
    if (q->head == p) q -> head = p -> next;
    q->count--;
    p -> next = p -> prev = NULL;
}

static void lwpinsert(PROCESS p, struct QUEUE *q)
{
    if (q->head == NULL) {	/* Queue is empty */
	q -> head = p;
	p -> next = p -> prev = p;
    } else {			/* Regular insert */
	p -> prev = q -> head -> prev;
	q -> head -> prev -> next = p;
	q -> head -> prev = p;
	p -> next = q -> head;
    }
    q->count++;
}

/* Moves a PROCESS p from QUEUE "from" to QUEUE "to" */
static void lwpmove(PROCESS p, struct QUEUE *from, struct QUEUE *to)
{
    lwpremove(p, from);
    lwpinsert(p, to);
}

int LWP_TerminateProcessSupport(void)       /* terminate all LWP support */
{
    int i;

    lwpdebug(0, "Entered Terminate_Process_Support");
    if (!lwp_init)
	return LWP_EINIT;

    if (lwp_cpptr != LWPANCHOR.outerpid)
	/* terminate support not called from same process as the init process */
        Abort_LWP("Terminate_Process_Support invoked from wrong process!");
    /* free all space allocated */
    for (i=0; i<MAX_PRIORITIES; i++)
        for_all_elts(cur, runnable[i], { Free_PCB(cur);})
    for_all_elts(cur, blocked, { Free_PCB(cur);})
    free((char *)lwp_init);
    lwp_init = NULL;
    return LWP_SUCCESS;
}



int LWP_GetRock(int Tag, char **Value)
{
    /* Obtains the pointer Value associated with the rock Tag of this LWP.
       Returns:
            LWP_SUCCESS    if specified rock exists and Value has been filled
            LWP_EBADROCK   rock specified does not exist
    */
    int i;
    struct rock *ra;

    ra = lwp_cpptr->rlist;

    for (i = 0; i < lwp_cpptr->rused; i++)
        if (ra[i].tag == Tag) {
            *Value =  ra[i].value;
            return(LWP_SUCCESS);
	}
    return(LWP_EBADROCK);
}


int LWP_NewRock(int Tag, char *Value)
{
    /* Finds a free rock and sets its value to Value.
        Return codes:
                LWP_SUCCESS     Rock did not exist and a new one was used
                LWP_EBADROCK    Rock already exists.
                LWP_ENOROCKS    All rocks are in use.

        From the above semantics, you can only set a rock value once.
        This is specifically to prevent multiple users of the LWP
        package from accidentally using the same Tag value and
        clobbering others.  You can always use one level of
        indirection to obtain a rock whose contents can change.  */
    
    int i;
    struct rock *ra;   /* rock array */

    ra = lwp_cpptr->rlist;

    /* check if rock has been used before */
    for (i = 0; i < lwp_cpptr->rused; i++)
        if (ra[i].tag == Tag) return(LWP_EBADROCK);

    /* insert new rock in rock list and increment count of rocks */
    if (lwp_cpptr->rused < MAXROCKS) {
        ra[lwp_cpptr->rused].tag = Tag;
        ra[lwp_cpptr->rused].value = Value;
        lwp_cpptr->rused++;
        return(LWP_SUCCESS);
    }
    else return(LWP_ENOROCKS);
}

int LWP_CurrentProcess(PROCESS *pid)
{
    lwpdebug(0, "Entered LWP_CurrentProcess");
    *pid = lwp_cpptr;
    return lwp_init ? LWP_SUCCESS : LWP_EINIT;
}

PROCESS LWP_ThisProcess()
{
    lwpdebug(0, "Entered LWP_ThisProcess");
    return lwp_init ? lwp_cpptr : NULL;
}


void LWP_SetLog(FILE *file, int level)
{
    lwp_logfile = file;
    lwp_debug = level;
}
	

int LWP_GetProcessPriority(PROCESS pid, int *priority)
{
    lwpdebug(0, "Entered Get_Process_Priority");
    if (!lwp_init)
	return LWP_EINIT;

    *priority = pid -> priority;
    return 0;
}

int LWP_WaitProcess(void *event)
{
    void *tempev[2];

    lwpdebug(0, "Entered Wait_Process");
    if (event == NULL) return LWP_EBADEVENT;
    tempev[0] = event;
    tempev[1] = NULL;
    return LWP_MwaitProcess(1, (char **)tempev);
}

static void Exit_LWP()
{
    exit (-1);
}

char *LWP_Name()
{
    return(lwp_cpptr->name);    
}

int LWP_Index()
{
    return(lwp_cpptr->index);
}

int LWP_HighestIndex()
{
    return(lwp_nextindex-1);
}

static void CheckWorkTime(PROCESS currentThread, PROCESS nextThread) 
{
    struct timeval current;
    struct timeval worktime;

    if (!cont_sw_threshold.tv_sec && !cont_sw_threshold.tv_usec) return;

    if (last_context_switch.tv_sec && cont_sw_id == currentThread)
    {
	gettimeofday(&current, NULL);
	worktime.tv_sec = current.tv_sec;
	worktime.tv_usec = current.tv_usec;
	if (worktime.tv_usec < last_context_switch.tv_usec) {
	    worktime.tv_usec += 1000000;
	    worktime.tv_sec -= 1;
	}
	worktime.tv_sec -= last_context_switch.tv_sec;
	worktime.tv_usec -= last_context_switch.tv_usec;

	if (timercmp(&worktime, &cont_sw_threshold, >)) {
	    struct tm *lt = localtime((const time_t *)&current.tv_sec);
	    fprintf(stderr, "[ %02d:%02d:%02d ] ***LWP %s(%p) took too much cpu %d secs %6d usecs\n", 
		    lt->tm_hour, lt->tm_min, lt->tm_sec, 
		    currentThread->name, currentThread, (int)worktime.tv_sec, (int)worktime.tv_usec);
	    fflush(stderr);	    
	}
	last_context_switch.tv_sec = current.tv_sec;
	last_context_switch.tv_usec = current.tv_usec;
    }
    else gettimeofday(&last_context_switch, NULL);
    cont_sw_id = nextThread;
}


static void CheckRunWaitTime(PROCESS thread) 
{
    struct timeval current;
    struct timeval waittime;

    if (!timerisset(&run_wait_threshold)) return;
    /* timer can be null during process creation. */
    if (!timerisset(&thread->lastReady)) return;

    gettimeofday(&current, NULL);
    waittime.tv_sec = current.tv_sec;
    waittime.tv_usec = current.tv_usec;
    if (waittime.tv_usec < thread->lastReady.tv_usec) {
	waittime.tv_usec += 1000000;
	waittime.tv_sec -= 1;
    }
    waittime.tv_sec -= thread->lastReady.tv_sec;
    waittime.tv_usec -= thread->lastReady.tv_usec;

    if (timercmp(&waittime, &run_wait_threshold, >)) {
	struct tm *lt = localtime((const time_t *)&current.tv_sec);
	fprintf(stderr, "[ %02d:%02d:%02d ] ***LWP %s(%p) run-wait too long %d secs %6d usecs\n", 
		lt->tm_hour, lt->tm_min, lt->tm_sec, 
		thread->name, thread, (int)waittime.tv_sec, (int)waittime.tv_usec);
	fflush(stderr);	    
    }
    timerclear(&thread->lastReady);
}

/*------------------------------------------*/
/* Routines that differ in OLD and NEW lwps */
/*------------------------------------------*/

/* A process calls this routine to wait until somebody signals it.
 * LWP_QWait removes the calling process from the runnable queue
 * and makes the process sleep until some other process signals via LWP_QSignal.
 */
int LWP_QWait()
{
    if (--lwp_cpptr->qpending >= 0)
	return LWP_SUCCESS;

    lwp_cpptr->status = WAITING;
    lwpmove(lwp_cpptr, &runnable[lwp_cpptr->priority], &blocked);
    timerclear(&lwp_cpptr->lastReady);
    LWP_DispatchProcess();

    return LWP_SUCCESS;
}

/* signal the PROCESS pid - by adding it to the runnable queue */
int LWP_QSignal(PROCESS pid)
{
    if (++pid->qpending != 0)
	return LWP_ENOWAIT;

    lwpdebug(0, "LWP_Qsignal: %s is going to QSignal %s\n", 
	     lwp_cpptr->name, pid->name);

    pid->status = READY;
    lwpmove(pid, &blocked, &runnable[pid->priority]);
    lwpdebug(0, "LWP_QSignal: Just inserted %s into runnable queue\n", pid->name);
    gettimeofday(&pid->lastReady, NULL);
    return LWP_SUCCESS;	
}

int LWP_CreateProcess(void (*ep)(void *), int stacksize, int priority,
		      void *parm, char *name, PROCESS *pid)
{
    PROCESS temp;
    char *stackptr;
#ifdef MMAP_LWP_STACKS
    int pagesize;
#endif

    lwpdebug(0, "Entered LWP_CreateProcess");

    if (!lwp_init)
	return LWP_EINIT;

    temp = (PROCESS) malloc (sizeof (struct lwp_pcb));
    if (!temp)
	return LWP_ENOMEM;

    if (stacksize < MINSTACK)
	stacksize = 1024;
    else
	stacksize = 4 * ((stacksize+3) / 4);

#ifndef MMAP_LWP_STACKS
    stackptr = (char *) malloc(stacksize);
#else
#ifdef MAP_ANON
    stackptr = mmap(lwp_stackbase, stacksize, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANON, -1, 0);
#else
    {
	int fd;
	stackptr = MAP_FAILED;
	if ((fd = open("/dev/zero", O_RDWR)) != -1) {
	    stackptr = mmap(lwp_stackbase, stacksize,
			    PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | (lwp_stackbase ? MAP_FIXED : 0),
			    fd, 0);
	    (void)close(fd);
	}
    }
#endif
    if ( stackptr == MAP_FAILED ) {
	perror("stack: ");
	assert(0);
    }
    pagesize = getpagesize();
    lwp_stackbase += ((stacksize/pagesize) + 2) * pagesize;
#endif
    if (!stackptr)
	return LWP_ENOMEM;

    if (priority < 0 || priority >= MAX_PRIORITIES)
	return LWP_EBADPRI;

    Initialize_Stack(stackptr, stacksize);
    Initialize_PCB(temp, priority, stackptr, stacksize, ep, parm, name);
    lwpinsert(temp, &runnable[priority]);
    gettimeofday(&temp->lastReady, NULL);

    LWP_DispatchProcess();
    *pid = temp;
    return 0;
}

/* The reaper is used to kill the current thread and schedule another
 * runnable thread. This context is called whenever a thread exits and by
 * LWP_DestroyProcess if the current process is destroyed. */
static void lwp_Reaper(void)
{
    Free_PCB(lwp_cpptr);
    lwp_cpptr = NULL;
    LWP_DispatchProcess();
    /* we never get here */
}

static void Dump_One_Process(PROCESS pid, FILE *fp)
{
    stack_t *stack = &pid->ctx.uc_stack;
    int i;

    fprintf(fp,"***LWP: process %s (%p)\n", pid->name, pid);
    if (pid->ep)
	fprintf(fp,"\tentry point: %p, parameter %p\n", pid->ep, pid->parm);
    fprintf(fp,"\tpriority: %d, status: ", pid->priority);
    switch (pid->status) {
	case READY:	fprintf(fp, "READY");     break;
	case WAITING:	fprintf(fp, "WAITING");   break;
	default:	fprintf(fp, "unknown");
	}
    fprintf(fp, "\n");

    if (pid->eventcnt) {
	fprintf(fp, "\tnumber of events outstanding: %d\n", pid->waitcnt);
	fprintf(fp, "\tevent list:");
	for (i = 0; i < pid->eventcnt; i++)
	    fprintf(fp, " %p", pid->eventlist[i]);
	fprintf(fp, "\n");
    }
    if (pid->wakevent)
	fprintf(fp, "\tlast wakeup event: %d\n", pid->wakevent);

    if (stack->ss_sp) {
	fprintf(fp, "\tstack base: %p, size: %d\n",
		stack->ss_sp, stack->ss_size);
	fprintf(fp, "\tstack usage high water mark: %d\n", Stack_Used(stack));
	fprintf(fp, "\tstack pointer at last yield: %p\n", pid->topstack);
    }

    lwp_stacktrace(fp, pid->topstack, stack);

    fprintf(fp,"==========================================\n");
}

static void lwp_Tracer(void)
{
    int i;

    for (i=0; i<MAX_PRIORITIES; i++)
	for_all_elts(x, runnable[i], {
		     fprintf(lwp_logfile, "[Priority %d]\n", i);
		     Dump_One_Process(x, lwp_logfile);
		     fflush(lwp_logfile);
		     })
    for_all_elts(x, blocked, {
		 fprintf(lwp_logfile, "[Blocked]\n");
		 Dump_One_Process(x, lwp_logfile);
		 fflush(lwp_logfile);
		 })
    fprintf(lwp_logfile, "Trace done\n");

    /* jump back to the thread that called us */
    setcontext(&lwp_cpptr->ctx);
}

static void init_contexts(void)
{
#define REAPER_STACKSIZE 8192
    getcontext(&reaper);
    reaper.uc_stack.ss_sp = malloc(REAPER_STACKSIZE);
    reaper.uc_stack.ss_size = REAPER_STACKSIZE;
    makecontext(&reaper, lwp_Reaper, 0);

#define TRACER_STACKSIZE 16384
    getcontext(&tracer);
    tracer.uc_stack.ss_sp = malloc(REAPER_STACKSIZE);
    tracer.uc_stack.ss_size = REAPER_STACKSIZE;
    makecontext(&tracer, lwp_Tracer, 0);
}

int LWP_DestroyProcess(PROCESS pid)
{
	lwpdebug(0, "Entered Destroy_Process");
	if (!lwp_init)
	    return LWP_EINIT;

	if (lwp_cpptr == pid)
	    swapcontext(&lwp_cpptr->ctx, &reaper);

	Free_PCB(pid);
	return LWP_SUCCESS;
}

static void lwp_Print_Processes(int dummy)
{
    if (!lwp_init) {
	fprintf(lwp_logfile, "***LWP: LWP support not initialized\n");
	return;
    }

    lwp_cpptr->topstack = &dummy;

    /* switch to the context that dumps the process stacks */
    swapcontext(&lwp_cpptr->ctx, &tracer);
}

void LWP_Print_Processes(void)
{
    lwp_Print_Processes(0);
}

/* Used to be externally visible as  LWP_InitializeProcessSupport() */
static int InitializeProcessSupport(int priority, PROCESS *pid)
{
	PROCESS temp;
	int i;
	
	lwpdebug(0, "Entered InitializeProcessSupport");
	if (lwp_init) 
	    return LWP_SUCCESS;

	init_contexts();

	timerclear(&last_context_switch);
	cont_sw_id = NULL;

	if (priority >= MAX_PRIORITIES) 
		return LWP_EBADPRI;
	for (i=0; i<MAX_PRIORITIES; i++) {
		runnable[i].head = NULL;
		runnable[i].count = 0;
	}
	blocked.head = NULL;
	blocked.count = 0;
	lwp_init = (struct lwp_ctl *) malloc(sizeof(struct lwp_ctl));
	temp = (PROCESS) malloc(sizeof(struct lwp_pcb));
	if (lwp_init == NULL || temp == NULL)
		Abort_LWP("Insufficient Storage to Initialize LWP Support");

	LWPANCHOR.processcnt = 1;
	LWPANCHOR.outerpid = temp;
	LWPANCHOR.outersp = NULL;
	Initialize_PCB(temp, priority, NULL, 0, NULL, NULL, "Main Process");
	lwpinsert(temp, &runnable[priority]);
	lwp_cpptr = temp;
	gettimeofday(&temp->lastReady, NULL);
	LWP_DispatchProcess();
	LWPANCHOR.outersp = temp->topstack;
	*pid = temp;
	return LWP_SUCCESS;
}

/* New initialization procedure; checks header and library versions
   Use this instead of LWP_InitializeProcessSupport() First argument
   should always be LWP_VERSION.  */

int LWP_Init(int version, int priority, PROCESS *pid)
{

    lwp_logfile = stderr;
    if (version != LWP_VERSION) {
	    fprintf(stderr, "**** FATAL ERROR: LWP VERSION MISMATCH ****\n");
	    exit(-1);
    }
    return(InitializeProcessSupport(priority, pid));    
}


/* wait on m of n events */
int LWP_MwaitProcess(int wcount, char **evlist)
{
	int ecount, i;

	lwpdebug(0, "Entered Mwait_Process [waitcnt = %d]", wcount);
	if (evlist == NULL) {
		return LWP_EBADCOUNT;
	}
	for (ecount = 0; evlist[ecount] != NULL; ecount++) ;
	if (ecount == 0) {
		return LWP_EBADCOUNT;
	}
	if (!lwp_init)
	    return LWP_EINIT;

	if (wcount>ecount || wcount<0) {
	    return LWP_EBADCOUNT;
	}
	if (ecount > lwp_cpptr->eventlistsize) {
	    lwp_cpptr->eventlist = 
		(char **)realloc((char *)lwp_cpptr->eventlist, 
				 ecount*sizeof(char *));
	    lwp_cpptr->eventlistsize = ecount;
	}
	for (i=0; i<ecount; i++) 
	    lwp_cpptr -> eventlist[i] = evlist[i];
	if (wcount > 0) {
	    lwp_cpptr -> status = WAITING;
	    lwpmove(lwp_cpptr, &runnable[lwp_cpptr->priority], &blocked);
	    timerclear(&lwp_cpptr->lastReady);
	}
	lwp_cpptr -> wakevent = 0;
	lwp_cpptr -> waitcnt = wcount;
	lwp_cpptr -> eventcnt = ecount;
	LWP_DispatchProcess();
	lwp_cpptr -> eventcnt = 0;
	return LWP_SUCCESS;
}


static void Abort_LWP(char *msg)
{
    lwpdebug(0, "Entered Abort_LWP");
    printf("***LWP Abort: %s\n", msg);
    Exit_LWP();
}

static void Free_PCB(PROCESS pid)
{
    stack_t *stack = &pid->ctx.uc_stack;

    lwpdebug(0, "Entered Free_PCB");

    lwpremove(pid, (pid->status==WAITING ?
		    &blocked :
		    &runnable[pid->priority]));
    LWPANCHOR.processcnt--;

    if (pid->name)
	free(pid->name);

    if (stack->ss_sp) {
	lwpdebug(0, "HWM stack usage: %d, [PCB at %p]", Stack_Used(stack), pid);
#ifdef MMAP_LWP_STACKS
	munmap(stack->ss_sp, stack->ss_size);
#else
	free(stack->ss_sp);
#endif
    }

    if (pid->eventlist != NULL)  free((char *)pid->eventlist);
    free((char *)pid);
}	

/* explicit voluntary preemption */
static int lwp_DispatchProcess(int dummy)
{
    static int dispatch_count = 0;
    PROCESS old_cpptr = lwp_cpptr;
    int i;

    if (!lwp_init)
	return LWP_EINIT;

    /* keep track of where the stack is */
    if (lwp_cpptr)
	lwp_cpptr->topstack = &dummy;

    lwpdebug(0, "Entered LWP_DispatchProcess");

    if (LWP_TraceProcesses > 0) {
	for (i=0; i<MAX_PRIORITIES; i++) {
	    printf("[Priority %d, runnable (%d):", i, runnable[i].count);
	    for_all_elts(p, runnable[i], { printf(" \"%s\"", p->name); })
	    puts("]");
    	}
	printf("[Blocked (%d):", blocked.count);
	for_all_elts(p, blocked, { printf(" \"%s\"", p->name); })
	puts("]");
    }

    /* Check for stack overflowif this lwp has a stack.  Check for
       the guard word at the front of the stack being damaged.
       WARNING!  This code assumes that stacks grow downward. */
    if (lwp_cpptr && lwp_cpptr->stackcheck &&
        (lwp_cpptr->stackcheck != *(int *)lwp_cpptr->ctx.uc_stack.ss_sp ||
	 (char *)&dummy < (char *)lwp_cpptr->ctx.uc_stack.ss_sp))
    {
	switch (lwp_overflowAction) {
	    case LWP_SOABORT:
		Overflow_Complain();
		abort ();
	    case LWP_SOMESSAGE:
	    default:
		Overflow_Complain();
		lwp_overflowAction = LWP_SOQUIET;
	    case LWP_SOQUIET:
		break;
	}
    }
    /* Move head of current runnable queue forward if current LWP is still in it. */
    if (lwp_cpptr && lwp_cpptr == runnable[lwp_cpptr->priority].head) 
	runnable[lwp_cpptr->priority].head = runnable[lwp_cpptr->priority].head -> next;

    /* Find highest priority with runnable processes. */
    for (i=MAX_PRIORITIES-1; i>=0; i--)
	if (runnable[i].head)
	    break;

    if (i < 0)
	Abort_LWP("LWP_DispatchProcess: Possible deadlock, "
		  "no runnable threads found\n");

    if (LWP_TraceProcesses > 0)
	printf("Dispatch %d [PCB at %p] \"%s\"\n", 
         ++dispatch_count, runnable[i].head, runnable[i].head->name);

    if (old_cpptr)
        gettimeofday(&old_cpptr->lastReady, NULL);	/* back in queue */

    lwp_cpptr = runnable[i].head;
    Cont_Sws++; /* number of context switches, for statistics */

    /* check time to context switch */
    if (timerisset(&cont_sw_threshold))
	CheckWorkTime(old_cpptr, lwp_cpptr);

    /* check time waiting to run */
    if (timerisset(&run_wait_threshold))
	CheckRunWaitTime(lwp_cpptr);

    if (!old_cpptr) {
	setcontext(&lwp_cpptr->ctx);
	assert(0); /* we should never get here */
    }

    /* If another thread is now runnable, swap context */
    if (lwp_cpptr != old_cpptr)
	swapcontext(&old_cpptr->ctx, &lwp_cpptr->ctx);

    return LWP_SUCCESS;
}

int LWP_DispatchProcess(void)
{
    return lwp_DispatchProcess(0);
}

static void Initialize_PCB(PROCESS temp, int priority, char *stack, 
			   int stacksize, void (*func)(void *), void *arg,
			   char *name)
{
    lwpdebug(0, "Entered Initialize_PCB");
    memset(temp, 0, sizeof(struct lwp_pcb));

    if (name)
	temp->name = strdup(name);

    temp->status = READY;
    temp->eventlist = (char **)malloc(EVINITSIZE*sizeof(char *));
    temp->eventlistsize = EVINITSIZE;
    temp->priority = priority;
    temp->index = lwp_nextindex++;
    temp->level = 1;		/* non-preemptable */
    timerclear(&temp->lastReady);

    temp->ep = func;
    temp->parm = arg;

    if (stack) {
	temp->stackcheck = *(int *)stack;

	getcontext(&temp->ctx);
	temp->ctx.uc_stack.ss_sp = stack;
	temp->ctx.uc_stack.ss_size = stacksize;
	temp->ctx.uc_link = &reaper; /* whenever a thread exits, we
					automatically reap it and schedule
					another runnable thread */
	makecontext(&temp->ctx, (void (*)(void))func, 1, arg);
    }

    lwpdebug(0, "Leaving Initialize_PCB\n");
}

static int Internal_Signal(void *event)
{
    int rc = LWP_ENOWAIT;
    int i;

    lwpdebug(0, "Entered Internal_Signal [event id %p]", event);
    if (!lwp_init) return LWP_EINIT;
    if (event == NULL) return LWP_EBADEVENT;

    for_all_elts(temp, blocked, {     /* for all pcb's on the blocked q */
        if (temp->status == WAITING)
	{
            for (i=0; i < temp->eventcnt; i++)
	    { /* check each event in list */
                if (temp -> eventlist[i] == event)
		{
                    temp -> eventlist[i] = NULL;
                    rc = LWP_SUCCESS;
                    /* reduce waitcnt by 1 for the signal */
                    /* if wcount reaches 0 then make the process runnable */
                    if (--temp->waitcnt == 0)
		    {
                        temp -> status = READY;
                        temp -> wakevent = i+1;
                        lwpmove(temp, &blocked, &runnable[temp->priority]);
			gettimeofday(&temp->lastReady, NULL);
                        break;
		    }
		}
	    }
	}
    })
    return rc;
}    

int LWP_INTERNALSIGNAL(void *event, int yield)
{
    int rc;
    lwpdebug(0, "Entered LWP_SignalProcess");
    if (!lwp_init)
	return LWP_EINIT;

    rc = Internal_Signal(event);
    if (yield) 
	LWP_DispatchProcess();

    return rc;
}


static void Initialize_Stack(void *stackptr, int stacksize)
{
/* This can be any unlikely pattern except 0x00010203 or the reverse. */
#define STACKMAGIC	0xBADBADBA
    int i;

    lwpdebug(0, "Entered Initialize_Stack");
    if (lwp_stackUseEnabled)
	for (i=0; i<stacksize; i++)
	    ((unsigned char *)stackptr)[i] = i &0xff;
    else
	*(int *)stackptr = STACKMAGIC;
}

static int Stack_Used(stack_t *stack)
{
    int i;

    if (*(int *) stack->ss_sp == STACKMAGIC)
	return 0;

    for (i = 0; i < stack->ss_size; i++)
	if (((unsigned char *)stack->ss_sp)[i] != (i & 0xff))
	    return (stack->ss_size - i);
    return 0;
}

int LWP_StackUsed(PROCESS pid, int *max, int *used)
{
    *max = pid->ctx.uc_stack.ss_size;
    *used = Stack_Used(&pid->ctx.uc_stack);
    if (*used == 0)
	return LWP_NO_STACK;
    return LWP_SUCCESS;
}


/* Complain of a stack overflow to stderr without using stdio (and use as
 * little stack as possible). */
static void Overflow_Complain()
{
    static const char *msg1 = "LWP: stack overflow in process ";
    static const char *msg2 = "!\n";
    write (2, msg1, strlen(msg1));
    write (2, lwp_cpptr->name, strlen(lwp_cpptr->name));
    write (2, msg2, strlen(msg2));
}

void PRE_Concurrent(int on) { }
void PRE_BeginCritical(void) { }
void PRE_EndCritical(void) { }

/*  The following documents the Assembler interfaces used by old LWP: 

savecontext(void (*ep)(), struct lwp_context *savearea, char *sp)


    Stub for Assembler routine that will
    save the current SP value in the passed
    context savearea and call the function
    whose entry point is in ep.  If the sp
    parameter is NULL, the current stack is
    used, otherwise sp becomes the new stack
    pointer.

returnto(struct lwp_context *savearea);

    Stub for Assembler routine that will
    restore context from a passed savearea
    and return to the restored C frame.
*/


