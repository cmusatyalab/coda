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

#include <sys/time.h>
#include <stdlib.h>
#ifdef HAVE_SEARCH_H
#include <search.h>
#endif

#include <lwp/lwp.h>
#include <lwp/timer.h>
#include "lwp.private.h"

typedef unsigned char bool;

#define expiration TotalTime

#define new_elem()	((struct TM_Elem *) malloc(sizeof(struct TM_Elem)))

#define MILLION	1000000

static int globalInitDone = 0;

/* declaration of private routines */
static void subtract (struct timeval *t1, struct timeval *t2, struct timeval *t3);
static void add (struct timeval *t1, struct timeval *t2);
static bool blocking (struct TM_Elem *t);



/* t1 = t2 - t3 */
static void subtract(struct timeval *t1, struct timeval *t2, struct timeval *t3)
{
    int sec, usec;

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
static void add(struct timeval *t1, struct timeval *t2)
{
    t1 -> tv_usec += t2 -> tv_usec;
    t1 -> tv_sec += t2 -> tv_sec;
    if (t1->tv_usec >= MILLION) {
	t1 -> tv_sec ++;
	t1 -> tv_usec -= MILLION;
    }
}

/* t1 == t2 */
int TM_eql(struct timeval *t1, struct timeval *t2)
{
    return (t1->tv_usec == t2->tv_usec) && (t1->tv_sec == t2->tv_sec);
}


static bool blocking(struct TM_Elem *t)
{
    return (t->TotalTime.tv_sec < 0 || t->TotalTime.tv_usec < 0);
}



/*
    Initializes a list -- returns -1 if failure, else 0.
*/

int TM_Init(struct TM_Elem **list)
{
    if (!globalInitDone) {
	FT_Init (0, 0);
	globalInitDone = 1;
    }
    *list = new_elem();
    if (*list == NULL)
	return -1;
    else {
	(*list) -> Next = *list;
	(*list) -> Prev = *list;
	(*list) -> TotalTime.tv_sec = 0;
	(*list) -> TotalTime.tv_usec = 0;
	(*list) -> TimeLeft.tv_sec = 0;
	(*list) -> TimeLeft.tv_usec = 0;
	(*list) -> BackPointer = NULL;

	return 0;
    }
}

int TM_Final(struct TM_Elem **list)
{
    if (list == NULL || *list == NULL)
	return -1;
    else {
	free((char *)*list);
	*list = NULL;
	return 0;
    }
}

/*
    Inserts elem into the timer list pointed to by *tlistPtr.
*/

void TM_Insert(struct TM_Elem *tlistPtr, struct TM_Elem *elem)
{
    struct TM_Elem *next;

    /* TimeLeft must be set for function IOMGR with infinite timeouts */
    elem -> TimeLeft = elem -> TotalTime;

    /* Special case -- infinite timeout */
    if (blocking(elem)) {
	insque((struct qelem *)elem, (struct qelem *)(tlistPtr->Prev));
	return;
    }

    /* Finite timeout, set expiration time */
    FT_GetTimeOfDay(&elem->expiration, 0);
    add(&elem->expiration, &elem->TimeLeft);
    next = NULL;
    FOR_ALL_ELTS(p, tlistPtr, {
	if (blocking(p) || !(elem->TimeLeft.tv_sec > p->TimeLeft.tv_sec ||
	    (elem->TimeLeft.tv_sec == p->TimeLeft.tv_sec && elem->TimeLeft.tv_usec >= p->TimeLeft.tv_usec))
	    ) {
		next = p;	/* Save ptr to element that will be after this one */
		break;
	}
     })

    if (next == NULL) next = tlistPtr;
    insque((struct qelem *)elem, (struct qelem *)(next->Prev));
}

/*
    Removes elem from the timer list in which it is located (presumably the one pointed
    to by *tlistPtr).
*/
void TM_Remove(struct TM_Elem *tlistPtr, struct TM_Elem *elem)
{
  remque((struct qelem *)elem);
}

/*
    Walks through the specified list and updates the TimeLeft fields in it.
    Returns number of expired elements in the list.
*/

int TM_Rescan(struct TM_Elem *tlist)
{
    struct timeval time;
    int expired;

    FT_GetTimeOfDay(&time, 0);
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

struct TM_Elem *TM_GetExpired(struct TM_Elem *tlist)
{
    FOR_ALL_ELTS(e, tlist, {
	if (!blocking(e) &&
	    (0 > e->TimeLeft.tv_sec || (0 == e->TimeLeft.tv_sec && 0 >= e->TimeLeft.tv_usec)))
		return e;
    })
    return NULL;
}
    
/*
    Returns a pointer to the earliest unexpired element in tlist.
    Its TimeLeft field will specify how much time is left.
    Returns 0 if tlist is empty or if there are no unexpired elements.
*/

struct TM_Elem *TM_GetEarliest(struct TM_Elem *tlist)
{
    struct TM_Elem *e;

    e = tlist -> Next;
    return (e == tlist ? NULL : e);
}
