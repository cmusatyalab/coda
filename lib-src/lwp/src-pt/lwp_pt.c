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
#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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
    PFIC    func;
    char   *parm; 
    char   *name;
    int     priority;
    sem_t   inited;
    PROCESS pid;
};

/* mutexes to block concurrent threads & various run queues */
static pthread_mutex_t run_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t join_mutex = PTHREAD_MUTEX_INITIALIZER;
struct list_head lwp_runq[LWP_MAX_PRIORITY + 1];
struct list_head lwp_join_queue;
PROCESS lwp_cpptr = NULL; /* the current non-concurrent process */

#ifdef BUGGY_MUTEX_LOCK
/* hmm, RH5.2 seems to suffer from some serious implementation problems */
#define PTHREAD_MUTEX_LOCK(mtx) do { \
    while((mtx)->m_count) sched_yield(); \
    pthread_mutex_lock(mtx); \
} while(0);
#else
#define PTHREAD_MUTEX_LOCK(mtx) pthread_mutex_lock(mtx);
#endif

/* Short explanation of the scheduling
 * 
 * The currently active non-concurrent thread always has the run_mutex, this
 * is the only invariant. All non-active non-concurrent threads are waiting:
 *  - in SCHEDULE on the pid->run_cond condition variable (runnable threads)
 *  - in LWP_MwaitProcess on the pid->event condition variable, or
 *  - in ObtainWrite/SharedLock on the pid->lock_cond condition variable, or
 *  - in ObtainReadLock on the lock->poke_readers condition variable
 *  
 * All these condition variables have run_mutex as their protecting mutex.
 * Whenever a non-concurrent thread is about to block in cond_wait it has
 * to call SIGNAL to unblock the next runnable thread. When it returns from
 * the cond_wait, it has to update the pid->havelock and lwp_cpptr variable,
 * as they have been lost (other threads were running).
 * 
 * IOMGR_Select and LWP_QWait make the non-concurrent thread temporarily
 * concurrent, using lwp_LEAVE and lwp_JOIN. lwp_LEAVE unblocks a runnable
 * thread before releasing the run_mutex. lwp_LEAVE and lwp_JOIN are the
 * _only_ functions that obtain and release the run_mutex, for the rest it
 * is only implicitly released while waiting on condition variables.
 * 
 * A thread trying to lwp_JOIN registers itself on the lwp_join_queue which
 * is protected by the join_mutex. This queue is handled at a higher
 * priority and not covered by the run_mutex to avoid starvation for
 * concurrent processes that try to get back in. (The run_mutex is quite
 * popular).
 *
 * SIGNAL unblocks the thread with the highest priority. All threads that
 * attempt to get on the runqueue's after roaming concurrently, are given
 * a little priority boost. They have a hard enough time trying to get the
 * run_mutex. An unblocked thread doesn't just go off and start running,
 * it first has to obtain the run_mutex (which we still hold on to). SIGNAL
 * returns a non zero number when the `signalled process' was ourselves.
 * This is a special case, because the scheduler will not temporarily
 * release the run_mutex while waiting on a condition variable. This would
 * lock out any threads that have been signalled in lwp_JOIN, and are
 * attempting to gain the run_mutex. The scheduler does an extra yield to
 * allow them in.
 * 
 * SCHEDULE links a thread on the tail of it's run-queue, and attempts to
 * unblock a runnable thread. It then starts waiting to get signalled
 * itself. This is a strict priority based roundrobin scheduler, as long as
 * there are runnable higher priority threads, lower queues will not be run
 * at all. All threads on the same queue are scheduled in a roundrobin order.
 * 
 * Non-concurrent thread have to be very careful not to get cancelled while
 * waiting on condition variables. Because the cleanup handler needs to get
 * access to the shared list of processes, and therefore needs to lock the
 * run_mutex. However, we cannot (easily) reaquire it without rejoining, but
 * we are already on the runqueues. So either the lwp_JOIN code becomes a
 * lot more complex, or we simply avoid cancellations while we are in
 * `scheduling limbo'.
 */

/*-------------BEGIN SCHEDULER CODE------------*/

/* PRE:  holding the run_mutex */
/* POST: holding the run_mutex, the next runnable thread has been signalled */
static int SIGNAL(PROCESS pid)
{
    PROCESS next = NULL;
    int i, done = 0;

    pid->havelock = 0;
    lwp_cpptr = NULL;

    /* signal a thread waiting to join */
    PTHREAD_MUTEX_LOCK(&join_mutex);
    {
        if (!list_empty(&lwp_join_queue)) {
            next = list_entry(lwp_join_queue.next, struct lwp_pcb, runq);
            pthread_cond_signal(&next->join_cond);
            done = 1;
        }
    }
    pthread_mutex_unlock(&join_mutex);
    if (done) return (pid == next);
    
    /* otherwise signal the highest priority thread */
    for (i = LWP_MAX_PRIORITY; i >= 0; i--) {
        if (!list_empty(&lwp_runq[i])) {
            next = list_entry(lwp_runq[i].next, struct lwp_pcb, runq);
            pthread_cond_signal(&next->run_cond);
            break;
        }
    }
    return (pid == next);
}

/* PRE:  holding the run_mutex */
/* POST: holding the run_mutex, other threads might have executed */
static void SCHEDULE(PROCESS pid)
{
    int i, done, oldstate;

    /* We need to avoid cancellations because we get stuck in the the
     * cleanup handler if we get cancelled while waiting on the condition
     * variable. (cleanup needs to call lwp_JOIN before removing us from the
     * list of threads, but we are already joined) */
    pthread_testcancel();
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
    list_add(&pid->runq, lwp_runq[pid->priority].prev);

    /* If we are the highest priority thread on the runqueue,
     * `higher' priority threads might be in limbo between being signalled
     * and grabbing the run_mutex, we have to yield and give them a chance
     * to enter. */
    if (SIGNAL(pid)) {
        pthread_mutex_unlock(&run_mutex);
#ifdef _POSIX_PRIORITY_SCHEDULING
        sched_yield(); /* really try very hard to give them a chance by
                          forcing a context switch */
#endif
        PTHREAD_MUTEX_LOCK(&run_mutex);
    }

    do {
        PTHREAD_MUTEX_LOCK(&join_mutex);
        {
            done = list_empty(&lwp_join_queue);
        }
        pthread_mutex_unlock(&join_mutex);

        for (i = LWP_MAX_PRIORITY; done && i > pid->priority; i--)
            if (!list_empty(&lwp_runq[i])) done = 0;

        if (done && lwp_runq[pid->priority].next == &pid->runq)
            break;

        pthread_cond_wait(&pid->run_cond, &run_mutex);
    } while(1);

    list_del(&pid->runq);

    pid->havelock = 1;
    lwp_cpptr = pid;

    /* if we get cancelled here, our cleanup function will correctly release
     * the run_mutex */
    pthread_setcancelstate(oldstate, NULL);
    pthread_testcancel();
}

/* PRE:  not holding the run_mutex */
/* POST: holding the run_mutex */
void lwp_JOIN(PROCESS pid)
{
    assert(pid->havelock == 0);

    PTHREAD_MUTEX_LOCK(&join_mutex);
    {
        list_add(&pid->runq, lwp_join_queue.prev);
        while (lwp_join_queue.next != &pid->runq)
            /* this is a safe cancellation point because we (should) only hold
             * the join_mutex */
            pthread_cond_wait(&pid->join_cond, &join_mutex);
    }
    pthread_mutex_unlock(&join_mutex);

    PTHREAD_MUTEX_LOCK(&run_mutex);
  /*{*/
        pid->havelock = 1;
        lwp_cpptr = pid;

        PTHREAD_MUTEX_LOCK(&join_mutex);
        {
            list_del(&pid->runq);
        }
        pthread_mutex_unlock(&join_mutex);
}

/* PRE:  holding the run_mutex */
/* POST: released the run_mutex, and the next non-concurrent thread is
 *       unblocked */
void lwp_LEAVE(PROCESS pid)
{
    assert(pid->havelock == 1);

        SIGNAL(pid);
  /*}*/
    pthread_mutex_unlock(&run_mutex);
}

/*-------------END SCHEDULER CODE------------*/


/* this function is called when a thread is cancelled and the thread specific
 * data is going to be destroyed */
static void lwp_cleanup_process(void *data)
{
    PROCESS pid = (PROCESS)data;

    /* we need the run_mutex to fiddle around with the process list */
    if (!pid->havelock) lwp_JOIN(pid);

    list_del(&pid->list);

    /* make sure we don't block other processes, and we are not on
     * the runqueue anymore */
    lwp_LEAVE(pid);

    /* ok, we're safe, now start cleaning up */
    list_del(&pid->lockq);
    sem_destroy(&pid->waitq);
    pthread_cond_destroy(&pid->run_cond);
    pthread_cond_destroy(&pid->lock_cond);
    pthread_cond_destroy(&pid->join_cond);
    pthread_cond_destroy(&pid->event);

    free(pid->evlist);
    free(data);
}

static int lwp_inited = 0;
int LWP_Init (int version, int priority, PROCESS *pid)
{
    PROCESS me;
    int i;

    if (version != LWP_VERSION) {
        fprintf(stderr, "**** FATAL ERROR: LWP VERSION MISMATCH ****\n");
        exit(-1);
    }

    if (lwp_inited) return LWP_SUCCESS;
    
    lwp_logfile = stderr;

    if (priority < 0 || priority > LWP_MAX_PRIORITY)
        return LWP_EBADPRI;
    
    assert(pthread_key_create(&lwp_private, lwp_cleanup_process) == 0);

    for (i = 0; i <= LWP_MAX_PRIORITY; i++)
        list_init(&lwp_runq[i]);
    list_init(&lwp_join_queue);
    list_init(&lwp_list);

    lwp_inited = 1;

    /* now set up our private process structure */
    assert(LWP_CurrentProcess(&me) == 0);

    strncpy(me->name, "Main Process", 31);
    me->priority = priority;

    lwp_JOIN(me);
    list_add(&me->list, &lwp_list);

    if (pid) *pid = me;

    return LWP_SUCCESS;
}

int LWP_CurrentProcess(PROCESS *pid)
{
    /* normally this is a short function */
    if (!pid) return LWP_EBADPID;
    *pid = (PROCESS)pthread_getspecific(lwp_private);
    if (*pid) return LWP_SUCCESS;

    /* but if there wasn't any thread specific data yet, we need to
     * initialize new data */
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

    list_init(&(*pid)->runq);
    assert(pthread_cond_init(&(*pid)->run_cond, NULL) == 0);
    assert(pthread_cond_init(&(*pid)->join_cond, NULL) == 0);

    list_init(&(*pid)->lockq);
    assert(pthread_cond_init(&(*pid)->lock_cond, NULL) == 0);

    pthread_setspecific(lwp_private, *pid);

    return LWP_SUCCESS;
}

/* The entry point for new threads, this sets up the thread specific data
 * and locks */
static void *lwp_newprocess(void *arg)
{
    struct lwp_forkinfo *newproc = (struct lwp_forkinfo *)arg;
    PROCESS              pid, parent;
    int                  retval;

    /* block incoming signals to this thread */
    sigset_t mask;
    sigemptyset(&mask);
    /* just adding the ones that venus tends to use */
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGIOT);
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
    pid->priority = newproc->priority;
    strncpy(pid->name, newproc->name, 31);
    
    /* Tell the parent thread that he's off the hook (although the caller
     * of LWP_CreateProcess isn't if any volatile parameters were passed,
     * but that was already the case). */
    parent = newproc->pid;
    newproc->pid = pid;
    LWP_QSignal(parent);

    /* Grab the concurrency semaphore, to comply with existing LWP policy
     * of not having concurrent threads. */
    LWP_QWait(); /* as a side effect this does an implicit lwp_JOIN */
    list_add(&pid->list, &lwp_list);

    /* Fire off the newborn */
    retval = pid->func(pid->parm);

    pthread_exit(&retval);
    /* Not reached */
}

int LWP_CreateProcess (PFIC ep, int stacksize, int priority, char *parm, 
                       char *name, PROCESS *pid)
{
    PROCESS             me;
    struct lwp_forkinfo newproc;
    pthread_attr_t      attr;
    pthread_t           threadid;
    int                 err;

    if (priority < 0 || priority > LWP_MAX_PRIORITY)
        return LWP_EBADPRI;
    
    assert(LWP_CurrentProcess(&me) == 0);

    newproc.func  = ep;
    newproc.parm  = parm;
    newproc.name  = name;
    newproc.priority = priority;
    newproc.pid   = me;
    
    assert(pthread_attr_init(&attr) == 0);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    err = pthread_create(&threadid, &attr, lwp_newprocess, &newproc);
    if (err) {
        fprintf(lwp_logfile, "Thread %s creation failed, error %s",
                  name, strerror(errno));
        return LWP_EMAXPROC;
    }

    /* Wait until the new thread has finished initialization. */
    LWP_QWait();
    if (pid) *pid = newproc.pid;
    LWP_QSignal(newproc.pid);
    LWP_DispatchProcess();

    return LWP_SUCCESS;
}

int LWP_DestroyProcess (PROCESS pid)
{
    pthread_cancel(pid->thread);
    if (pid->waitcnt) {
	pid->waitcnt = 0;
	pthread_cond_signal(&pid->event);
    }
    return LWP_SUCCESS;
}

int LWP_TerminateProcessSupport()
{
    struct list_head *ptr;
    PROCESS           me, pid;
    
    assert(LWP_CurrentProcess(&me) == 0);

    /* make us a scheduled (non-concurrent) thread */
    if (me->concurrent) lwp_JOIN(me);

    for (ptr = lwp_list.next; ptr != &lwp_list; ptr = ptr->next) {
        pid = list_entry(ptr, struct lwp_pcb, list);

        /* venus flytrap? */
        //pid->concurrent = 0;

        /* I should not kill myself. */
        if (!pthread_equal(me->thread, pid->thread))
            LWP_DestroyProcess(pid);
    }

    /* At this point we are at least sure that all non-concurrent threads
     * are blocking in the scheduler. And as the scheduler is a cancellation
     * point, all blocked threads should be cancelled by now. */
    list_del(&me->list);
    while(!list_empty(&lwp_list)) {
	SCHEDULE(me);
    }
    lwp_LEAVE(me);

    /* We can start cleaning. */
    lwp_cleanup_process(me);
    pthread_mutex_destroy(&run_mutex);
    pthread_key_delete(lwp_private);

    return LWP_SUCCESS;
}

int LWP_DispatchProcess()
{
    PROCESS pid;

    if (LWP_CurrentProcess(&pid))
        return LWP_EBADPID;

    if (!pid->concurrent) SCHEDULE(pid);
    else                  pthread_testcancel();

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

    if (pid->havelock) lwp_LEAVE(pid);
    sem_wait(&pid->waitq); /* wait until we get signalled */
    if (!pid->concurrent) lwp_JOIN(pid);

    return LWP_SUCCESS;
}

int LWP_INTERNALSIGNAL(void *event, int yield)
{
    struct list_head *ptr;
    PROCESS           me, pid;
    int               i;

    assert(LWP_CurrentProcess(&me) == 0);

    if (!me->havelock) lwp_JOIN(me);

    for (ptr = lwp_list.next; ptr != &lwp_list; ptr = ptr->next) {
        pid = list_entry(ptr, struct lwp_pcb, list);
        if (pid == me) continue;

        for (i = 0; i < pid->eventcnt; i++) {
            if (pid->evlist[i] == event) {
                pid->evlist[i] = NULL;
                pid->waitcnt--;
            }
        }
        if (pid->eventcnt && pid->waitcnt <= 0)
            pthread_cond_signal(&pid->event);
    }

    if (me->concurrent) lwp_LEAVE(me);
    else if (yield)     LWP_DispatchProcess();

    return LWP_SUCCESS;
}

/* MWaitProcess actually knows a lot about how the scheduling works.
 * We need to avoid cancellations because we get stuck in the the
 * cleanup handler if we get cancelled while waiting on the condition
 * variable. (cleanup needs to call lwp_JOIN before removing us from the
 * list of threads, but we're already sort of `joined') */
int LWP_MwaitProcess (int wcount, char *evlist[])
{
    PROCESS pid;
    int     entries, i, oldstate;

    if (!evlist) return LWP_EBADCOUNT;

    /* count number of entries in the eventlist */
    for (entries = 0; evlist[entries] != NULL; entries++) /* loop */;
        if (wcount <= 0 || wcount > entries) return LWP_EBADCOUNT;

    if (LWP_CurrentProcess(&pid)) return LWP_EBADPID;

    /* copy the events */
    if (entries > pid->evsize) {
        pid->evlist = (char **)realloc(pid->evlist, entries * sizeof(char*));
        pid->evsize = entries;
    }
    for (i = 0; i < entries; i++) pid->evlist[i] = evlist[i];
    pid->waitcnt = wcount;

    /* from here on everything has to occur non-concurrent, and we want to
     * avoid cancellations */
    if (!pid->havelock) lwp_JOIN(pid);
    else		pthread_testcancel();
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    /* this will enable other threads to start sending us signals */
    pid->eventcnt = entries;
    SIGNAL(pid); /* unblock the next runnable process */

    /* wait until we got enough events */
    while (pid->waitcnt > 0) pthread_cond_wait(&pid->event, &run_mutex);

    /* reset the havelock & lwp_cpptr variables, other's might have stolen
     * them while we were `out' */
    pid->havelock = 1;
    lwp_cpptr = pid;
    
    /* make sure we don't get stray signals anymore */
    pid->eventcnt = 0;

    /* concurrent threads may roam freely again */
    if (pid->concurrent) lwp_LEAVE(pid);

    /* now we can safely get cancelled again, our cleanup function will
     * release the run_mutex if necessary */
    pthread_setcancelstate(oldstate, NULL);
    pthread_testcancel();

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

