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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/fail/Attic/delay.c,v 4.4 1998/09/29 16:37:35 braam Exp $";
#endif /*_BLURB_*/





/* 
 * delay.c -- package for delaying packets, to simulate slow networks.
 *           L. Mummert
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <assert.h>
#include <lwp.h>
#include <rpc2.h>
#include <timer.h>
#include "fail.h"
#include "delay.h"

typedef struct packet_info {
    struct packet_info *next;
    int socket;
    struct sockaddr_in *sap;
    RPC2_PacketBuffer  *pb;
    struct timeval timeToWait, debug;
} packetInfo;

typedef struct delay_queue_info {
    unsigned char a,b,c,d;	    	/* IP address for which packets delay */
    int numPackets;             	/* number of packets in the queue */
    struct timeval timer; 	    	/* time next event should happen. */
    packetInfo *delayQueue;		/* queue of events waiting to happen. */
    packetInfo *lastElem;		/* last event in queue */
    int count;			  	/* Number of filters to this server */
} delayQueueInfo;

struct {
    delayQueueInfo *queues;
    int count;
    int size;
} DelayQueues = {0,0,0};

static int Delay_LWP();
static void SubFromTime();
PROCESS DelayLWPPid;

int Delay_Init()
{
    if (DelayQueues.queues)	/* Already initialized */
	return -1;
    
    DelayQueues.queues = (delayQueueInfo *)malloc(4 * sizeof(delayQueueInfo));
    bzero(DelayQueues.queues, 4 * sizeof(delayQueueInfo));
    DelayQueues.count = 0;
    DelayQueues.size = 4;	/* Good number to start with! */
    LWP_CreateProcess((PFIC) Delay_LWP, 4096, LWP_NORMAL_PRIORITY,
		      "Delay_LWP", NULL, &DelayLWPPid);
    return 0;
}

/* 
 * Delay_Packet -- calculates the delay of a packet of length
 * 'length' for a 'speed' bps network.  If the delay is greater
 * than MINDELAY, the packet is copied and queued with the
 * appropriate timeout, and the routine returns 0.  If the
 * delay is too small, the routine does nothing else and 
 * returns 1.
 */
int DelayPacket(speed, socket, sap, pb, queue)
int speed;
long socket;
struct sockaddr_in *sap;
RPC2_PacketBuffer *pb;
int queue;
{
	u_int msec;
	packetInfo *pp;
	struct timeval tmpTime;
	delayQueueInfo *dq;

	assert(speed > 0);
	assert(DelayQueues.queues);	/* Make sure Delay_Init was called */

	msec = pb->Prefix.LengthOfPacket * 1000 * 8 / speed; 
	if (msec < MINDELAY) 
		return(1);

	dq = &DelayQueues.queues[queue];
	dq->numPackets++;

	pp = (packetInfo *)malloc(sizeof(packetInfo));
	pp->timeToWait.tv_sec = msec / 1000;
	pp->timeToWait.tv_usec = (msec % 1000) * 1000;
	pp->socket = socket;
	pp->sap = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	bcopy(sap, pp->sap, sizeof(struct sockaddr_in));
	pp->pb = (RPC2_PacketBuffer *) malloc(pb->Prefix.BufferSize);
	bcopy(pb, pp->pb, pb->Prefix.BufferSize);
	pp->next = NULL;

	/* Insert into queue. */
	if (dq->delayQueue) {		/* If something is already there. */
	    dq->lastElem->next = pp;
	    dq->lastElem = pp;
	} else {
	    /* Start the timer. */
	    assert(!dq->lastElem && !dq->timer.tv_sec && !dq->timer.tv_usec);
	    dq->delayQueue = dq->lastElem = pp;
	    dq->timer = pp->timeToWait;
	}
	
	/* wake delay thread, if nothing waiting, who cares? */
	LWP_NoYieldSignal((char *)Delay_LWP);

	return(0);
}

/* Should queues for IP addresses be removed when no more filters for the address
 * exist? It is likely that the space of IP addresses to which a filter will
 * apply is small, and once a filter is removed it is likely to be replaced
 * or the process is likely to die soon. However I could be wrong. So I've added
 * code for decrementing the count. If you want it to be installed, take out
 * the ifdefs (here and in fail.c) AND DEBUG THE CODE -- DCS
 */
#ifdef NOTDEF
/* Decrement the count for the queue for the server with IP addr a.b.c.d */
int DecQueue(i, a, b, c, d)
     int i;
     unsigned char a, b, c, d;
{
    delayQueueInfo *dq;
    assert(DelayQueues.queues);		/* Make sure Delay_Init was called */

    assert((i >= 0) && (i <= DelayQueues.count));

    dq = &DelayQueues.queues[i];

    /* Maybe a bit strong, but why shouldn't it match? */
    assert((dq->a == a) && (dq->b == b) && (dq->c == c) && (dq->d == d));

    assert(dq->count);
    
    if (--dq->count == 0) {
	free(dq->delayQueue);
	bzero(dq, sizeof(delayQueueInfo));
    }

    return 0;
}
#endif NOTDEF

/* Find a queue for the server with IP address a.b.c.d */
int FindQueue(a, b, c, d)
     unsigned char a, b, c, d;
{
    int i;
    assert(DelayQueues.queues);		/* Make sure Delay_Init was called */

    for (i = 0; i < DelayQueues.count; i++) {
	delayQueueInfo *dq = &DelayQueues.queues[i];
	if ((dq->a == a) && (dq->b == b) && (dq->c == c) && (dq->d == d)) {
#ifdef NOTDEF	    
	    dq->count++;
#endif NOTDEF	    
	    return i;
	}
    }

    return -1;
}

int MakeQueue(a, b, c, d)
     unsigned char a, b, c, d;
{
    delayQueueInfo *dq, *tmp;
    int newsize;
    
    assert(DelayQueues.queues);	/* Make sure Delay_Init was called */

#ifdef NOTDEF
    {
	int count;

	for (count = 0; count < DelayQueues.count; count++) {
	    if (DelayQueues.queues[count].delayQueue == 0) {
		dq = &DelayQueues.queues[count];
		dq->delayQueue = (struct TM_Elem *)malloc(sizeof(struct TM_Elem));
		TM_Init(&dq->delayQueue);
		return count;
	    }
	}
    }
#endif NOTDEF	
    if (DelayQueues.count == DelayQueues.size) {     /* Need to grow the list */
	newsize = DelayQueues.size * 2;
	tmp = (delayQueueInfo *)malloc(newsize * sizeof(delayQueueInfo));
	bcopy(DelayQueues.queues, tmp, DelayQueues.size * sizeof(delayQueueInfo));
	free(DelayQueues.queues);
	DelayQueues.queues = tmp;
	DelayQueues.size = newsize;
    }

    assert(DelayQueues.count < DelayQueues.size);
    dq = &DelayQueues.queues[DelayQueues.count];
    dq->delayQueue = dq->lastElem = NULL;
    dq->a = a; dq->b = b; dq->c = c; dq->d = d;
    dq->timer.tv_sec = dq->timer.tv_usec = 0;
    dq->count = 1;
    dq->numPackets = 0;
    return DelayQueues.count++;
}

/* The following are all private to this module */


/* Decrement time fromp by amtp, return 0 if more time left, 1 otherwise */
static void SubFromTime(fromp, amtp)
struct timeval *fromp, *amtp;
{
	if (amtp->tv_usec > fromp->tv_usec) {
		fromp->tv_sec--;
		fromp->tv_usec += 1000000;
	}
	fromp->tv_sec -= amtp->tv_sec;
	fromp->tv_usec -= amtp->tv_usec;

	if (fromp->tv_sec < 0) 
	    fromp->tv_sec = fromp->tv_usec = 0;
}


static int Delay_LWP()
{
    int i, j, socket;
    struct timeval timeToNext;
    delayQueueInfo *dq, *nextEvent;
    
    assert(DelayQueues.queues);	/* Make sure Delay_Init was called */
    
    while (1) {
	/* find smallest remaining time. */
	nextEvent = NULL;
	for (i = 0; i < DelayQueues.count; i++) {
	    dq = &DelayQueues.queues[i];
	    
	    if (dq->numPackets)		/* Something is waiting. */
		if (!nextEvent || (dq->timer.tv_sec < nextEvent->timer.tv_sec) ||
		    (dq->timer.tv_sec == nextEvent->timer.tv_sec &&
		     dq->timer.tv_usec < nextEvent->timer.tv_usec))
		    nextEvent = dq;
	}

	if (nextEvent == NULL) {
	    LWP_WaitProcess((char *)Delay_LWP);	/* Won't have missed anything */
	    continue;				
	}

	timeToNext = nextEvent->timer;
	
	/* decrement all timers by that time. Assume we only wait timeToNext,
	 * Since finding out how long we really waited would cost a GetTimeOfDay.
	 * We decrement before the select to avoid decrementing events that
	 * occur while we are sleeping.
	 */
	for (i = 0; i < DelayQueues.count; i++) {
	    dq = &DelayQueues.queues[i];

	    if (dq->numPackets) 		/* Something is waiting. */
		SubFromTime(&dq->timer, &timeToNext);
	}

	/* Wait for next event to happen. Q: if time to wait is too small,
	 * should I skip the select (lower bound on waiting) or wait anyway
	 * (upper bound)? I'm choosing to wait anyway.
	 */
	IOMGR_Select(0, 0, 0, 0, &timeToNext);

	/* Find and handle any expired events */
	for (i = 0; i < DelayQueues.count; i++) {
	    dq = &DelayQueues.queues[i];

	    if (dq->numPackets) {		/* Something is waiting. */
		if ((dq->timer.tv_sec == NULL) && (dq->timer.tv_usec == NULL)) {
		    /* Expired event: send packet, set new timer */
		    packetInfo *pp = dq->delayQueue;
		    
		    if (sendto(pp->socket, &pp->pb->Header, 
			       pp->pb->Prefix.LengthOfPacket, 
			       0, (struct sockaddr *)pp->sap, sizeof(struct sockaddr_in))
			!= pp->pb->Prefix.LengthOfPacket) {
			/* problem: Lily left it to me. I leave it for you, DCS */
		    }

		    /* Remove the element */
		    dq->numPackets--;
		    dq->delayQueue = pp->next;
		    if (dq->delayQueue == NULL)
			dq->lastElem = NULL;
		    free(pp->sap); free(pp->pb);
		    free(pp);

		    /* Set timer for next element. */
		    if (dq->delayQueue)
			dq->timer = dq->delayQueue->timeToWait;
		}
	    }
	}
    }
}
    



