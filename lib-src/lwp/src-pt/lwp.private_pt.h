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

#ifndef _LWP_PRIVATE_H_
#define _LWP_PRIVATE_H_

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

#include "dllist.h"

/* rocks are useful. you can, for instance, hide neat things under them */
struct rock {
    int tag;
    char *value;
};

struct lwp_pcb {
    pthread_t thread; /* thread id */
    struct list_head list; /* list of all threads */

    int concurrent;
    int waiting;
    int priority;

    char *name; /* ASCII name */

    void (*func)(void *); /* entry point */
    void *parm; /* parameters */

#define MAXROCKS 8 /* max. # of rocks per LWP */
    int nrocks; /* rocks in use */
    struct rock rock[MAXROCKS]; /* and the rocks themselves */

    sem_t waitq; /* used by QWait/QSignal */

    pthread_cond_t event; /* used by INTERNALSIGNAL/MWait */
    int eventcnt; /* # of events in the evlist */
    int waitcnt; /* # of events we wait for */
    int evsize; /* size of the evlist */
    char **evlist; /* list of events we wait for */
};

void lwp_JOIN(PROCESS pid);
void lwp_LEAVE(PROCESS pid);
int lwp_threads_waiting(void);

/* logging classes */
#define LWP_DBG_LOCKS 1

extern int lwp_loglevel;
extern FILE *lwp_logfile;

#define lwp_dbg(class, msg...)           \
    do {                                 \
        if (lwp_loglevel & class) {      \
            fprintf(lwp_logfile, ##msg); \
            fflush(lwp_logfile);         \
        }                                \
    } while (0);

#define lwp_mutex_lock(lock)                                     \
    pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, \
                         (void *)(lock));                        \
    pthread_mutex_lock(lock)

#define lwp_mutex_unlock(lock) pthread_cleanup_pop(1)

#endif /* _LWP_PRIVATE_H_ */
