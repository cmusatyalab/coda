/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

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


/*
	(Multiple) readers & writers test of LWP stuff.

*/

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>

#define DEFAULT_READERS	5

#define STACK_SIZE	65536

/* The shared queue */
typedef struct QUEUE {
    struct QUEUE	*prev, *next;
    char		*data;
    struct Lock		lock;
} queue;

/* declaration of internal routines */
static queue *init();
static char empty (queue *q);
static void insert (queue *q, char *s);
static char *myremove (queue *q);
static void read_process (int id);
static void write_process();

static char *messages[] =
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

static queue *init()
{
    queue *q;

    q = (queue *) malloc(sizeof(queue));
    q -> prev = q -> next = q;
    q -> data = NULL;
    return(q);
}

static char empty(queue *q)
{
    return (q->prev == q && q->next == q);
}

static void cleanup(queue *q)
{
    assert(empty(q));
    free(q);
}

static void insert(queue *q, char *s)
{
    queue *n;

    n = (queue *) malloc(sizeof(queue));
    n -> data = s;
    n -> prev = q -> prev;
    q -> prev -> next = n;
    q -> prev = n;
    n -> next = q;
}

static char *myremove(queue *q)
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
    free(old);
    return(s);
}

queue *q;

int asleep;	/* Number of processes sleeping -- used for
		   clean termination */

static void read_process(id)
    int id;
{

    printf("\t[Reader %d]\n", id);
    LWP_DispatchProcess();		/* Just relinquish control for now */

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

	printf("[%d: %s]\n", id, myremove(q));

	ReleaseReadLock(&q->lock);
	LWP_DispatchProcess();
    }
}

static void write_process(){

    char **mesg;

    printf("\t[Writer]\n");

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

int main(int argc, char **argv)
{

    int nreaders, i;
    long interval;	/* To satisfy Brad */
    PROCESS *readers;
    PROCESS writer, mainthread;
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
    LWP_Init(LWP_VERSION, 0, &mainthread);
    printf("[Support initialized]\n");
    tv.tv_sec = 0;
    tv.tv_usec = interval;

    /* Initialize queue */
    q = init();

    /* Initialize lock */
    Lock_Init(&q->lock);

    asleep = 0;
    /* Now create readers */
    printf("[Creating Readers...\n");
    readers = (PROCESS *) calloc((unsigned)nreaders, (unsigned)(sizeof(PROCESS)));
    for (i=0; i<nreaders; i++)
	LWP_CreateProcess((PFI)read_process, STACK_SIZE, 0, (char *)(long)i,
			  "Reader", &readers[i]);
    printf("done]\n");

    printf("\t[Creating Writer...\n");
    LWP_CreateProcess((PFI)write_process, STACK_SIZE, 1, 0, "Writer", &writer);
    printf("done]\n");

    /* Now loop until everyone's done */
    while (asleep != nreaders+1) LWP_DispatchProcess();
    /* Destroy the readers */
    for (i=nreaders-1; i>=0; i--) LWP_DestroyProcess(readers[i]);
    printf("\n*Exiting*\n");
    LWP_TerminateProcessSupport();
    free(readers);
    cleanup(q);
    return 0;
}
