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
                           none currently

#*/

/* 
 * delay.c -- package for delaying packets, to simulate slow networks.
 *           L. Mummert
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>

#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
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
static void AddToTime(struct timeval *top, struct timeval *amtp);
PROCESS DelayLWPPid;

int Delay_Init()
{
    if (DelayQueues.queues)	/* Already initialized */
	return -1;
    
    DelayQueues.queues = (delayQueueInfo *)malloc(4 * sizeof(delayQueueInfo));
    memset(DelayQueues.queues, 0, 4 * sizeof(delayQueueInfo));
    DelayQueues.count = 0;
    DelayQueues.size = 4;	/* Good number to start with! */
    LWP_CreateProcess((PFIC) Delay_LWP, 4096, LWP_NORMAL_PRIORITY,
		      NULL, "Delay_LWP", &DelayLWPPid);
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
int DelayPacket(int latency, int speed, long socket, struct sockaddr_in *sap,
		RPC2_PacketBuffer *pb, int queue)
{
	u_int msec;
	packetInfo *pp;
	delayQueueInfo *dq;
        struct timeval now;

	assert(speed > 0);
	assert(DelayQueues.queues);	/* Make sure Delay_Init was called */

	msec = latency + ((pb->Prefix.LengthOfPacket * 1000 * 8) / speed);

	dq = &DelayQueues.queues[queue];

	dq->numPackets++;

	pp = (packetInfo *)malloc(sizeof(packetInfo));
	pp->timeToWait.tv_sec = msec / 1000;
	pp->timeToWait.tv_usec = (msec % 1000) * 1000;
	pp->socket = socket;
	pp->sap = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	memcpy(pp->sap, sap, sizeof(struct sockaddr_in));
	pp->pb = (RPC2_PacketBuffer *) malloc(pb->Prefix.BufferSize);
	memcpy(pp->pb, pb, pb->Prefix.BufferSize);
	pp->next = NULL;

	/* Insert into queue. */
	if (dq->delayQueue) {		/* If something is already there. */
	    dq->lastElem->next = pp;
	    dq->lastElem = pp;
	} else {
	    /* Start the timer. */
	    assert(!dq->lastElem);
	    dq->delayQueue = dq->lastElem = pp;
	    dq->timer = pp->timeToWait;
            FT_GetTimeOfDay(&now, NULL);
            AddToTime(&dq->timer, &now);
	}
	
	/* wake delay thread, if nothing waiting, who cares? */
	IOMGR_Cancel(DelayLWPPid);

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
	memset(dq, 0, sizeof(delayQueueInfo));
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
	memcpy(tmp, DelayQueues.queues, DelayQueues.size * sizeof(delayQueueInfo));
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


/* Increment/Decrement time fromp by amtp. */
static void AddToTime(struct timeval *top, struct timeval *amtp)
{
        top->tv_sec += amtp->tv_sec;
        top->tv_usec += amtp->tv_usec;
        if (top->tv_usec >= 1000000) {
            top->tv_usec -= 1000000;
            top->tv_sec += 1;
        }
}

static void SubFromTime(struct timeval *fromp, struct timeval *amtp)
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

static int CompareTime(struct timeval *a, struct timeval *b)
{
	if (a->tv_sec < b->tv_sec) return -1;
	if (a->tv_sec > b->tv_sec) return 1;
        /* a->tv_sec == b->tv_sec */
	if (a->tv_usec < b->tv_usec) return -1;
	if (a->tv_usec > b->tv_usec) return 1;
        return 0;
}

static int Delay_LWP()
{
    int i;
    struct timeval timeToNext, now;
    delayQueueInfo *dq, *nextEvent;
    packetInfo *pp;
    
    assert(DelayQueues.queues);	/* Make sure Delay_Init was called */
    
    while (1) {
	/* find smallest remaining time. */
	nextEvent = NULL;
	for (i = 0; i < DelayQueues.count; i++) {
	    dq = &DelayQueues.queues[i];
	    
            /* Something is waiting. */
	    if (dq->numPackets &&
                (!nextEvent || CompareTime(&dq->timer, &nextEvent->timer) < 0))
                nextEvent = dq;
	}

        if (nextEvent) {
            timeToNext = nextEvent->timer;
            FT_GetTimeOfDay(&now, NULL);
            SubFromTime(&timeToNext, &now);
        }

	/* Wait for next event to happen. Q: if time to wait is too small,
	 * should I skip the select (lower bound on waiting) or wait anyway
	 * (upper bound)? I'm choosing to wait anyway.
         * We block, waiting for IOMGR_Cancel when there is nothing to wait
         * for.
	 */
	IOMGR_Select(0, 0, 0, 0, nextEvent ? &timeToNext : NULL);

        /* check how long we were waiting */
        FT_GetTimeOfDay(&now, NULL);

	/* Find and handle any expired events */
	for (i = 0; i < DelayQueues.count; i++) {
again:
	    dq = &DelayQueues.queues[i];

            /* nothing is waiting */
            if (!dq->numPackets) continue;

            /* time has not yet expired */
            if (CompareTime(&dq->timer, &now) > 0) continue;

            /* Expired event: send packet, set new timer */
            pp = dq->delayQueue;

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
            if (dq->delayQueue) {
                AddToTime(&dq->timer, &dq->delayQueue->timeToWait);
                /* reprocess this queue, maybe we need to send this new packet
                 * as well */
                goto again;
            }
	}
    }
}

