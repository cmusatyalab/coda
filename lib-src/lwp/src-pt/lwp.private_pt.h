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
    int   tag;
    char *value;
};

struct lwp_pcb {
    pthread_t        thread;            /* thread id */
    struct list_head list;              /* list of all threads */

    struct list_head runq;
    pthread_cond_t   run_cond;
    pthread_cond_t   join_cond;
    struct list_head lockq;
    pthread_cond_t   lock_cond;
    int              concurrent;
    int              havelock;
    int              priority;

    char	     name[32];          /* ASCII name */

    int       (*func)(void *);          /* entry point */
    void       *parm;                   /* parameters */

#define MAXROCKS 8                      /* max. # of rocks per LWP */
    int         nrocks;                 /* rocks in use */
    struct rock rock[MAXROCKS];         /* and the rocks themselves */
    
    sem_t          waitq;               /* used by QWait/QSignal */
    pthread_cond_t event;               /* used by INTERNALSIGNAL/MWait */
    int            eventcnt;            /* # of events in the evlist */
    int            waitcnt;             /* # of events we wait for */
    int            evsize;              /* size of the evlist */
    char         **evlist;              /* list of event we wait for */
};

extern struct list_head lwp_runq[LWP_MAX_PRIORITY + 1];
extern struct list_head lwp_join_queue;

void lwp_JOIN(PROCESS pid);
void lwp_LEAVE(PROCESS pid);

/* logging classes */
#define LWP_DBG_LOCKS 1

extern int   lwp_loglevel;
extern FILE *lwp_logfile;

#define lwp_debug(class, msg...) \
    do { if (lwp_loglevel & class) \
        { fprintf(lwp_logfile, ## msg); \
         fflush(lwp_logfile); } } while(0);

#endif /* _LWP_PRIVATE_H_ */
