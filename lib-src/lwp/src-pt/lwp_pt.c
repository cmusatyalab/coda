/* BLURB lgpl

                           Coda File System
                              Release 5

            Copyright (c) 1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
#*/

#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include "lwp.private_pt.h"

/* BEGIN - NOT USED exported variables */
int          lwp_debug;	          /* ON = show LWP debugging trace */
int          lwp_overflowAction;  /* Action to take on stack overflow. */
int          lwp_stackUseEnabled; /* Tells if stack size counting is enabled. */
/* variables used for checking work time of an lwp */
struct timeval last_context_switch; /* how long a lwp was running */
struct timeval cont_sw_threshold;  /* how long a lwp is allowed to run */
struct timeval run_wait_threshold;
/* END - NOT USED exported variables */

FILE *lwp_logfile = NULL; /* where to log debug messages to */
int   lwp_loglevel = 0;   /* which messages to log */

static pthread_key_t    lwp_private; /* thread specific data */
static struct list_head lwp_list;    /* list of all threads */

/* information passed to a child process */
struct lwp_forkinfo {
    void  (*func)(void *);
    char   *parm; 
    char   *name;
    int     prio;
    PROCESS pid;
};

/* mutexes to block concurrent threads & various run queues */
static pthread_mutex_t run_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t run_cond = PTHREAD_COND_INITIALIZER;
PROCESS lwp_cpptr = NULL; /* the currently running LWP thread */

/* Short explanation of the scheduling
 * 
 * All non-active non-concurrent threads are waiting:
 *  - in SCHEDULE on the pid->run_cond condition variable (runnable threads)
 *  - in LWP_MwaitProcess on the pid->event condition variable, or
 *  - in ObtainLock on the lock->wakeup condition variable
 *  
 * All these condition variables have run_mutex as their protecting mutex.
 * Whenever a non-concurrent thread is about to block in cond_wait it has
 * to call SIGNAL to unblock the next runnable thread.
 * 
 * IOMGR_Select and LWP_QWait make a non-concurrent thread temporarily
 * concurrent, using lwp_LEAVE and lwp_YIELD. lwp_LEAVE unblocks a runnable
 * thread before releasing the run_mutex. lwp_LEAVE and lwp_YIELD are the
 * _only_ functions that obtain and release the run_mutex, for the rest it
 * is only implicitly released while waiting on condition variables.
 * 
 * SCHEDULE links a thread on the tail of it's run-queue, and attempts to
 * unblock a runnable thread. It then starts waiting to get signalled
 * itself. This is a strict priority based roundrobin scheduler, as long as
 * there are runnable higher priority threads, lower queues will not be run
 * at all. All threads on the same queue are scheduled in a roundrobin order.
 * 
 * Non-concurrent threads have to be very careful not to get cancelled while
 * waiting on condition variables because the cleanup handler needs to get
 * access to the shared list of processes, and therefore needs to lock the
 * run_mutex.
 */

/*-------------BEGIN SCHEDULER CODE------------*/

static int lwp_waiting;

int lwp_threads_waiting(void)
{
    int ret;

    lwp_mutex_lock(&run_mutex);
    ret = lwp_waiting;
    lwp_mutex_unlock(&run_mutex);

    return ret;
}

static void _SCHEDULE(PROCESS pid, int leave)
{
}

void lwp_LEAVE(PROCESS pid)
{
    if (pid == lwp_cpptr) {
	lwp_mutex_lock(&run_mutex);
	lwp_cpptr = NULL;
	pthread_cond_signal(&run_cond);
	lwp_mutex_unlock(&run_mutex);
    }
    pthread_testcancel();
}

void lwp_YIELD(PROCESS pid)
{
    lwp_LEAVE(pid);

    if (pid->concurrent)
	return;

    lwp_mutex_lock(&run_mutex);
    if (lwp_cpptr || lwp_waiting) {
	lwp_waiting++;
	pid->waiting = 1;

	/* block at least once to give others a chance to run */
	do {
	    pthread_cond_wait(&run_cond, &run_mutex);
	} while(lwp_cpptr);

	lwp_waiting--;
	pid->waiting = 0;
    }
    lwp_cpptr = pid;
    lwp_mutex_unlock(&run_mutex);
}

/*-------------END SCHEDULER CODE------------*/


/* this function is called when a thread is cancelled and the thread specific
 * data is going to be destroyed */
static void lwp_cleanup_process(void *data)
{
    PROCESS pid = (PROCESS)data;

    /* now we need the run_mutex to fiddle around with the process list */
    lwp_mutex_lock(&run_mutex);
    {
	if (pid == lwp_cpptr) {
	    lwp_cpptr = NULL;
	    pthread_cond_signal(&run_cond);
	}
	if (pid->waiting)
	    lwp_waiting--;

	list_del(&pid->list);
    }
    lwp_mutex_unlock(&run_mutex);

    /* ok, we're safe, start cleaning up */
    sem_destroy(&pid->waitq);
    pthread_cond_destroy(&pid->event);

    if (pid->name)   free(pid->name);
    if (pid->evlist) free(pid->evlist);
    free(data);
}

static int lwp_inited = 0;
int LWP_Init (int version, int priority, PROCESS *ret)
{
    PROCESS pid;

    if (version != LWP_VERSION) {
	fprintf(stderr, "**** FATAL ERROR: LWP VERSION MISMATCH ****\n");
	exit(-1);
    }

    if (lwp_inited)
	return LWP_SUCCESS;

    lwp_logfile = stderr;

    if (priority < 0 || priority > LWP_MAX_PRIORITY)
	return LWP_EBADPRI;

    assert(pthread_key_create(&lwp_private, lwp_cleanup_process) == 0);

    list_init(&lwp_list);

    lwp_inited = 1;

    /* now set up our private process structure */
    assert(LWP_CurrentProcess(&pid) == 0);

    pid->name = strdup("Main Process");
    pid->priority = priority;

    /* As we're still initializing (and therefore the first LWP thread) we
     * don't need to fiddle with the run_mutex */
    list_add(&pid->list, &lwp_list);
    lwp_cpptr = pid;

    if (ret) *ret = pid;

    return LWP_SUCCESS;
}

int LWP_CurrentProcess(PROCESS *pid)
{
    /* normally this is a short function */
    if (!pid) return LWP_EBADPID;
    *pid = (PROCESS)pthread_getspecific(lwp_private);
    if (*pid) return LWP_SUCCESS;

    /* but if there wasn't any thread specific data yet, we need to
     * initialize it now */
    *pid = (PROCESS)malloc(sizeof(struct lwp_pcb));

    if (!*pid) {
	fprintf(lwp_logfile, "Couldn't allocate thread specific data\n");
	return LWP_ENOMEM;
    }
    memset(*pid, 0, sizeof(struct lwp_pcb));

    (*pid)->thread   = pthread_self();
    (*pid)->evsize   = 5;
    (*pid)->evlist   = (char **)malloc((*pid)->evsize * sizeof(char*));

    list_init(&(*pid)->list);
    assert(sem_init(&(*pid)->waitq, 0, 0) == 0);
    assert(pthread_cond_init(&(*pid)->event, NULL) == 0);

    pthread_setspecific(lwp_private, *pid);

    return LWP_SUCCESS;
}

/* The entry point for new threads, this sets up the thread specific data
 * and locks */
static void *lwp_newprocess(void *arg)
{
    struct lwp_forkinfo *newproc = (struct lwp_forkinfo *)arg;
    PROCESS              pid, parent = newproc->pid;

    /* block incoming signals to this thread */
    sigset_t mask;
    sigemptyset(&mask);
    /* just adding the ones that venus tends to use */
    sigaddset(&mask, SIGHUP);
#ifdef SIGIOT
    sigaddset(&mask, SIGIOT);
#endif
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGXCPU);
    sigaddset(&mask, SIGXFSZ);
    sigaddset(&mask, SIGVTALRM);
    sigaddset(&mask, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);

    /* Initialize the thread specific data */
    LWP_CurrentProcess(&pid);

    pid->func = newproc->func;
    pid->parm = newproc->parm;
    pid->priority = newproc->prio;
    pid->name = strdup(newproc->name);

    /* Tell the parent thread that he's off the hook (although the caller
     * of LWP_CreateProcess isn't if any volatile parameters were passed,
     * but that was already the case). */
    newproc->pid = pid;
    LWP_QSignal(parent);

    lwp_mutex_lock(&run_mutex);
    list_add(&pid->list, &lwp_list);
    lwp_mutex_unlock(&run_mutex);

    lwp_YIELD(pid);

    /* Fire off the newborn */
    pid->func(pid->parm);

    lwp_LEAVE(pid);
    pthread_exit(NULL);
    /* Not reached */
}

int LWP_CreateProcess(void (*ep)(void *), int stacksize, int priority,
		      void *parm, char *name, PROCESS *ret)
{
    PROCESS             pid;
    struct lwp_forkinfo newproc;
    pthread_attr_t      attr;
    pthread_t           threadid;
    int                 err;

    if (priority < 0 || priority > LWP_MAX_PRIORITY)
	return LWP_EBADPRI;

    assert(LWP_CurrentProcess(&pid) == 0);

    newproc.func = ep;
    newproc.parm = parm;
    newproc.name = name;
    newproc.prio = priority;
    newproc.pid  = pid;

    /* For some reason cygwin likes to return EBUSY here */
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    err = pthread_create(&threadid, &attr, lwp_newprocess, &newproc);
    if (err) {
	fprintf(lwp_logfile, "Thread %s creation failed, error %s",
		name, strerror(errno));
	return LWP_EMAXPROC;
    }

    /* Wait until the new thread has finished initialization. */
    LWP_QWait();
    if (ret) *ret = newproc.pid;

    return LWP_SUCCESS;
}

static void _LWP_DestroyProcess (PROCESS pid)
{
    pthread_cancel(pid->thread);
    if (pid->waitcnt) {
	pid->waitcnt = 0;
	pthread_cond_signal(&pid->event);
    }
}

int LWP_DestroyProcess (PROCESS pid)
{
    lwp_mutex_lock(&run_mutex);
    {
	_LWP_DestroyProcess(pid);
    }
    lwp_mutex_unlock(&run_mutex);
    return LWP_SUCCESS;
}

int LWP_TerminateProcessSupport()
{
    struct list_head *ptr;
    PROCESS           this, pid;

    assert(LWP_CurrentProcess(&this) == 0);

    lwp_mutex_lock(&run_mutex);
    {
	/* I should not kill myself. */
	list_del(&this->list);

	for (ptr = lwp_list.next; ptr != &lwp_list; ptr = ptr->next) {
	    pid = list_entry(ptr, struct lwp_pcb, list);
	    _LWP_DestroyProcess(pid);
	}
    }
    lwp_mutex_unlock(&run_mutex);

    /* Threads should be cancelled by now, we just have to wait for them to
     * terminate. */
    while(!list_empty(&lwp_list))
	lwp_YIELD(this);

    /* We can start cleaning. */
    lwp_cleanup_process(this);
    pthread_mutex_destroy(&run_mutex);
    pthread_key_delete(lwp_private);

    return LWP_SUCCESS;
}

int LWP_DispatchProcess(void)
{
    PROCESS pid;

    if (LWP_CurrentProcess(&pid))
	return LWP_EBADPID;

    lwp_YIELD(pid);

    return LWP_SUCCESS;
}

/* QSignal/QWait give _at least once_ semantics, and does almost no locking
 * while LWP_INTERNALSIGNAL/LWP_MWaitEvent give _at most once_ semantics
 * and require more elaborate locking */
/* As QSignals don't get lost, it would have solved the RVM thread deadlock
 * too. My guess is that this is the preferred behaviour. */
int LWP_QSignal(PROCESS pid)
{
    sem_post(&pid->waitq);
    return LWP_SUCCESS;
}

int LWP_QWait()
{
    PROCESS pid;

    if (LWP_CurrentProcess(&pid))
	return LWP_EBADPID;

    lwp_LEAVE(pid);
    sem_wait(&pid->waitq); /* wait until we get signalled */
    lwp_YIELD(pid);

    return LWP_SUCCESS;
}

int LWP_INTERNALSIGNAL(void *event, int yield)
{
    struct list_head *ptr;
    PROCESS           this, pid;
    int               i;

    assert(LWP_CurrentProcess(&this) == 0);

    lwp_mutex_lock(&run_mutex);
    {
	list_for_each(ptr, &lwp_list)
	{
	    pid = list_entry(ptr, struct lwp_pcb, list);
	    if (pid == this) continue;
	    if (!pid->waitcnt) continue;

	    for (i = 0; i < pid->eventcnt; i++) {
		if (pid->evlist[i] == event) {
		    pid->evlist[i] = NULL;
		    pid->waitcnt--;
		    break;
		}
	    }
	    if (pid->eventcnt && !pid->waitcnt)
		pthread_cond_signal(&pid->event);
	}
    }
    lwp_mutex_unlock(&run_mutex);

    if (yield)
	lwp_YIELD(this);

    return LWP_SUCCESS;
}

/* MWaitProcess actually knows a lot about how the scheduling works.
 * We need to avoid cancellations because we get stuck in the the
 * cleanup handler if we get cancelled while waiting on the condition
 * variable. (cleanup needs to lock the run_mutex to removing us from the
 * list of threads, but we're already sort of `joined') */
int LWP_MwaitProcess (int wcount, char *evlist[])
{
    PROCESS pid;
    int     entries, i;

    if (!evlist) return LWP_EBADCOUNT;

    /* count number of entries in the eventlist */
    for (entries = 0; evlist[entries] != NULL; entries++) /* loop */;
    if (wcount <= 0 || wcount > entries) return LWP_EBADCOUNT;

    if (LWP_CurrentProcess(&pid)) return LWP_EBADPID;

    /* copy the events */
    if (entries > pid->evsize) {
        pid->evlist = (char **)realloc(pid->evlist, entries * sizeof(*evlist));
	assert(pid->evlist != NULL);
        pid->evsize = entries;
    }
    memcpy(pid->evlist, evlist, entries * sizeof(*evlist));
    pid->waitcnt = wcount;
    pid->eventcnt = entries;

    lwp_LEAVE(pid);

    /* wait until we received enough events */
    lwp_mutex_lock(&run_mutex);
    while (pid->waitcnt)
	pthread_cond_wait(&pid->event, &run_mutex);
    lwp_mutex_unlock(&run_mutex);

    lwp_YIELD(pid);

    return LWP_SUCCESS;
}

int LWP_WaitProcess (void *event)
{
    void *evlist[2];

    evlist[0] = event; evlist[1] = NULL;
    return LWP_MwaitProcess(1, (char**)evlist);
}

int LWP_NewRock (int Tag, char *Value)
{
    PROCESS pid;
    int     i;
    
    if (LWP_CurrentProcess(&pid))
        return LWP_EBADPID;
    
    for (i = 0; i < pid->nrocks; i++)
        if (Tag == pid->rock[i].tag)
            return LWP_EBADROCK;

    if (pid->nrocks == MAXROCKS - 1)
        return LWP_ENOROCKS;

    pid->rock[pid->nrocks].tag   = Tag;
    pid->rock[pid->nrocks].value = Value;
    pid->nrocks++;
    
    return LWP_SUCCESS;
}

int LWP_GetRock (int Tag,  char **Value)
{
    PROCESS pid;
    int     i;
    
    if (LWP_CurrentProcess(&pid))
        return LWP_EBADPID;
    
    for (i = 0; i < pid->nrocks; i++) {
        if (Tag == pid->rock[i].tag) {
            *Value = pid->rock[i].value;
            return LWP_SUCCESS;
        }
    }

    return LWP_EBADROCK;
}

char *LWP_Name(void)
{
    PROCESS pid;
    if (LWP_CurrentProcess(&pid)) return NULL;
    return pid->name;
}

int LWP_GetProcessPriority (PROCESS pid, int *priority)
{
    if (priority) *priority = pid->priority;
    return LWP_SUCCESS;
}

void LWP_SetLog(FILE *file, int level)
{
    lwp_logfile  = file;
    lwp_loglevel = level;
}

/* silly function, is already covered by LWP_CurrentProcess */
PROCESS LWP_ThisProcess(void)
{
    PROCESS pid;
    int     err;
    err = LWP_CurrentProcess(&pid);
    return (err ? NULL : pid);
}

int LWP_StackUsed (PROCESS pid, int *max, int *used)
{
    if (max)  max  = 0;
    if (used) used = 0;
    return LWP_SUCCESS;
}

int LWP_Index()            { return 0; }
int LWP_HighestIndex()     { return 0; }
void LWP_UnProtectStacks() { return; } /* only available for newlwp */
void LWP_ProtectStacks()   { return; }

