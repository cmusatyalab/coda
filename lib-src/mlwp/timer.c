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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/lib-src/mlwp/timer.c,v 4.3 1998/04/14 20:42:23 braam Exp $";
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

#include <sys/time.h>
#include <stdlib.h>
#define _TIMER_IMPL_
#include "lwp.h"
#include "lwp.private.h"
#include "timer.h"
#include <assert.h>
typedef unsigned char bool;
#define NIL	0

#define expiration TotalTime

#define new_elem()	((struct TM_Elem *) malloc(sizeof(struct TM_Elem)))

#define MILLION	1000000

static int globalInitDone = 0;

/* declaration of private routines */
static void subtract C_ARGS((register struct timeval *t1, register struct timeval *t2, register struct timeval *t3));
static void add C_ARGS((register struct timeval *t1, register struct timeval *t2));
static bool blocking C_ARGS((register struct TM_Elem *t));



/* t1 = t2 - t3 */
static void subtract(t1, t2, t3)
    register struct timeval *t1;
    register struct timeval *t2;
    register struct timeval *t3;
{
    register int sec, usec;

    sec = t2 -> tv_sec;
    usec = t2 -> tv_usec;
    if (t3->tv_usec > usec) {
	usec += MILLION;
	sec--;
    }
    t1 -> tv_usec = usec - t3 -> tv_usec;
    t1 -> tv_sec = sec - t3 -> tv_sec;
}

/* t1 += t2; */
static void add(t1, t2)
    register struct timeval *t1;
    register struct timeval *t2;
{
    t1 -> tv_usec += t2 -> tv_usec;
    t1 -> tv_sec += t2 -> tv_sec;
    if (t1->tv_usec >= MILLION) {
	t1 -> tv_sec ++;
	t1 -> tv_usec -= MILLION;
    }
}

/* t1 == t2 */
int TM_eql(t1, t2)
    register struct timeval *t1;
    register struct timeval *t2;
{
    return (t1->tv_usec == t2->tv_usec) && (t1->tv_sec == t2->tv_sec);
}


static bool blocking(t)
    register struct TM_Elem *t;
{
    return (t->TotalTime.tv_sec < 0 || t->TotalTime.tv_usec < 0);
}



/*
    Initializes a list -- returns -1 if failure, else 0.
*/

int TM_Init(list)
    register struct TM_Elem **list;
{
    if (!globalInitDone) {
	FT_Init (0, 0);
	globalInitDone = 1;
    }
    *list = new_elem();
    if (*list == NIL)
	return -1;
    else {
	(*list) -> Next = *list;
	(*list) -> Prev = *list;
	(*list) -> TotalTime.tv_sec = 0;
	(*list) -> TotalTime.tv_usec = 0;
	(*list) -> TimeLeft.tv_sec = 0;
	(*list) -> TimeLeft.tv_usec = 0;
	(*list) -> BackPointer = NIL;

	return 0;
    }
}

int TM_Final(list)
    register struct TM_Elem **list;
{
    if (list == NIL || *list == NIL)
	return -1;
    else {
	free((char *)*list);
	*list = NIL;
	return 0;
    }
}

/*
    Inserts elem into the timer list pointed to by *tlistPtr.
*/

void TM_Insert(tlistPtr, elem)
    struct TM_Elem *tlistPtr;
    struct TM_Elem *elem;
{
    register struct TM_Elem *next;
    assert(tlistPtr);
    /* TimeLeft must be set for function IOMGR with infinite timeouts */
    elem -> TimeLeft = elem -> TotalTime;

    /* Special case -- infinite timeout */
    if (blocking(elem)) {
	insque((struct qelem *)elem, (struct qelem *)(tlistPtr->Prev));
	return;
    }

    /* Finite timeout, set expiration time */
    FT_AGetTimeOfDay(&elem->expiration, 0);
    add(&elem->expiration, &elem->TimeLeft);
    next = NIL;
    FOR_ALL_ELTS(p, tlistPtr, {
	if (blocking(p) || !(elem->TimeLeft.tv_sec > p->TimeLeft.tv_sec ||
	    (elem->TimeLeft.tv_sec == p->TimeLeft.tv_sec && elem->TimeLeft.tv_usec >= p->TimeLeft.tv_usec))
	    ) {
		next = p;	/* Save ptr to element that will be after this one */
		break;
	}
     })

    if (next == NIL) next = tlistPtr;
    insque((struct qelem *)elem, (struct qelem *)(next->Prev));
}

/*
    Removes elem from the timer list in which it is located (presumably the one pointed
    to by *tlistPtr).
*/
void TM_Remove(tlistPtr, elem)
     struct TM_Elem *tlistPtr;
     struct TM_Elem *elem;
{
  remque((struct qelem *)elem);
}

/*
    Walks through the specified list and updates the TimeLeft fields in it.
    Returns number of expired elements in the list.
*/

int TM_Rescan(tlist)
    struct TM_Elem *tlist;
{
    struct timeval time;
    register int expired;

    FT_AGetTimeOfDay(&time, 0);
    expired = 0;
    FOR_ALL_ELTS(e, tlist, {
	if (!blocking(e)) {
	    subtract(&e->TimeLeft, &e->expiration, &time);
	    if (0 > e->TimeLeft.tv_sec || (0 == e->TimeLeft.tv_sec && 0 >= e->TimeLeft.tv_usec))
		expired++;
	}
    })
    return expired;
}
    
/*
    RETURNS POINTER TO earliest expired entry from tlist.
    Returns 0 if no expired entries are present.
*/

struct TM_Elem *TM_GetExpired(tlist)
    struct TM_Elem *tlist;
{
    FOR_ALL_ELTS(e, tlist, {
	if (!blocking(e) &&
	    (0 > e->TimeLeft.tv_sec || (0 == e->TimeLeft.tv_sec && 0 >= e->TimeLeft.tv_usec)))
		return e;
    })
    return NIL;
}
    
/*
    Returns a pointer to the earliest unexpired element in tlist.
    Its TimeLeft field will specify how much time is left.
    Returns 0 if tlist is empty or if there are no unexpired elements.
*/

struct TM_Elem *TM_GetEarliest(tlist)
    struct TM_Elem *tlist;
{
    register struct TM_Elem *e;

    e = tlist -> Next;
    return (e == tlist ? NIL : e);
}
