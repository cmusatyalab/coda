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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./lib-src/mlwp/rw.c,v 1.1 1996/11/22 19:18:59 braam Exp $";
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


/*
	(Multiple) readers & writers test of LWP stuff.

*/

#include <sys/time.h>
#include <stdio.h>

#include "lwp.h"
#include "lock.h"
#ifdef PREEMPT
#include "preempt.h"
#endif PREEMPT

#define DEFAULT_READERS	5

#define STACK_SIZE	65536

/* The shared queue */
typedef struct QUEUE {
    struct QUEUE	*prev, *next;
    char		*data;
    struct Lock		lock;
} queue;

/* declaration of internal routines */
PRIVATE queue *init();
PRIVATE char empty C_ARGS((queue *q));
PRIVATE void insert C_ARGS((queue *q, char *s));
PRIVATE char *myremove C_ARGS((queue *q));
PRIVATE void read_process C_ARGS((int id));
PRIVATE void write_process();

PRIVATE char *messages[] =
    {
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	0
    };

PRIVATE queue *init(){

    queue *q;

    q = (queue *) malloc(sizeof(queue));
    q -> prev = q -> next = q;
    return(q);
}

PRIVATE char empty(q)
    queue *q;
{

    return (q->prev == q && q->next == q);
}

PRIVATE void insert(q, s)
    queue *q;
    char *s;
{
    queue *n;

    n = (queue *) malloc(sizeof(queue));
    n -> data = s;
    n -> prev = q -> prev;
    q -> prev -> next = n;
    q -> prev = n;
    n -> next = q;
}

PRIVATE char *myremove(q)
    queue *q;
{

    queue *old;
    char *s;

    if (empty(q)) {
	printf("Remove from empty queue");
	abort();
    }

    old = q -> next;
    q -> next = old -> next;
    q -> next -> prev = q;
    s = old -> data;
    free((char *)old);
    return(s);
}

queue *q;

int asleep;	/* Number of processes sleeping -- used for
		   clean termination */

PRIVATE void read_process(id)
    int id;
{

    printf("\t[Reader %d]\n", id);
    LWP_DispatchProcess();		/* Just relinquish control for now */

#ifdef PREEMPT
    PRE_PreemptMe(); 
#endif PREEMPT

    for (;;) {
        register int i;

	/* Wait until there is something in the queue */
	asleep++;
	ObtainReadLock(&q->lock);
	while (empty(q)) {
	    ReleaseReadLock(&q->lock);
	    LWP_WaitProcess((char *)q);
	    ObtainReadLock(&q->lock);
	}
	asleep--;
	for (i=0; i<10000; i++) ;
#ifdef PREEMPT
	PRE_BeginCritical();
#endif PREEMPT

	printf("[%d: %s]\n", id, myremove(q));

#ifdef PREEMPT
	PRE_EndCritical();
#endif PREEMPT
	ReleaseReadLock(&q->lock);
	LWP_DispatchProcess();
    }
}

PRIVATE void write_process(){

    char **mesg;

    printf("\t[Writer]\n");
#ifdef PREEMPT
    PRE_PreemptMe();
#endif PREEMPT

    /* Now loop & write data */
    for (mesg=messages; *mesg!=0; mesg++) {
	ObtainWriteLock(&q->lock);
	insert(q, *mesg);
	ReleaseWriteLock(&q->lock);
	LWP_SignalProcess((char *)q);
    }

    asleep++;
}
/*
	Arguments:
		0:	Unix junk, ignore
		1:	Number of readers to create (default is DEFAULT_READERS)
		2:	# msecs for interrupt (to satisfy Larry)
		3:	Present if lwp_debug to be set
*/

main(argc, argv)
    int argc;
    char **argv;
{

    int nreaders, i;
    long interval;	/* To satisfy Brad */
    PROCESS *readers;
    PROCESS writer;
    struct timeval tv;

    printf("\n*Readers & Writers*\n\n");
    setbuf(stdout, 0);

    /* Determine # readers */
    if (argc == 1)
	nreaders = DEFAULT_READERS;
    else
	sscanf(*++argv, "%d", &nreaders);
    printf("[There will be %d readers]\n", nreaders);

    interval = (argc >= 3 ? atoi(*++argv)*1000 : 50000);

    if (argc == 4) lwp_debug = 1;
    LWP_Init(LWP_VERSION, 0, (PROCESS *)&i);
    printf("[Support initialized]\n");
    tv.tv_sec = 0;
    tv.tv_usec = interval;
#ifdef PREEMPT
    PRE_InitPreempt(&tv);
#endif PREEMPT

    /* Initialize queue */
    q = init();

    /* Initialize lock */
    Lock_Init(&q->lock);

    asleep = 0;
    /* Now create readers */
    printf("[Creating Readers...\n");
    readers = (PROCESS *) calloc((unsigned)nreaders, (unsigned)(sizeof(PROCESS)));
    for (i=0; i<nreaders; i++)
	LWP_CreateProcess((PFI)read_process, STACK_SIZE, 0, (char *)i, "Reader", &readers[i]);
    printf("done]\n");

    printf("\t[Creating Writer...\n");
    LWP_CreateProcess((PFI)write_process, STACK_SIZE, 1, 0, "Writer", &writer);
    printf("done]\n");

    /* Now loop until everyone's done */
    while (asleep != nreaders+1) LWP_DispatchProcess();
    /* Destroy the readers */
    for (i=nreaders-1; i>=0; i--) LWP_DestroyProcess(readers[i]);
    printf("\n*Exiting*\n");
}
