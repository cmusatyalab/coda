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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/fail/Attic/fail.c,v 4.1 1997/01/08 21:49:36 rvb Exp $";
#endif /*_BLURB_*/







/*
  Network failure simulation package

  Walter Smith

 */

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "fail.h"
#include "delay.h"

extern void ntohFF(FailFilter *);
extern void htonFF(FailFilter *);

#ifdef DEBUG_FAIL
#define DBG(x) printf x;
#else
#define DBG(x) 
#endif

/* Hooks into network package */
extern int (*Fail_SendPredicate)(), (*Fail_RecvPredicate)();
PRIVATE int StdSendPredicate(), StdRecvPredicate();

/* Hooks for user customization

   int Fail_UserSendPredicate(ip1, ip2, ip3, ip4, color, packetbuffer, sockaddr, socket)
   int Fail_UserRecvPredicate(ip1, ip2, ip3, ip4, color, packetbuffer)
   
   If one of these variables is not NULL, libfail will call the filter routine
   before deciding whether to drop a packet.  Possible returns are:
      0: Drop the packet.
      1: Do not drop the packet.
      2: Let libfail decide.
 */

int (*Fail_UserSendPredicate)(), (*Fail_UserRecvPredicate)();

/* Our globals */

PRIVATE char clientName[MAXNAMELEN];
/* numFilters and theFilters for sendSide and recvSide */
PRIVATE int numFilters[2];
PRIVATE FailFilter *theFilters[2];
PRIVATE int *theQueues[2];

PRIVATE int FilterID = 1;

#ifdef DEBUG_FAIL
#define TEMPDEBUG(msg) \
{ printf(msg); PrintFilters(); }
#else
#define TEMPDEBUG(msg)
#endif DEBUG_FAIL

PRIVATE PrintFilter(f)
register FailFilter *f;
{
    printf("\tip %d.%d.%d.%d color %d len %d-%d factor %d speed %d\n",
	   f->ip1, f->ip2, f->ip3, f->ip4, f->color, f->lenmin,
	   f->lenmax, f->factor, f->speed);
}

PRIVATE PrintFilters()
{
    int side, which;

    for (side = 0; side < 2; side++) {
	printf("Side %d: %d filters\n", side, numFilters[side]);
	for (which = 0; which < numFilters[side]; which++) {
	    printf("\t%-2d:", which);
	    PrintFilter(&theFilters[side][which]);
	}
    }
}


/* Exported routines */

/* Initialize the package. */

int Fail_Initialize(name, flags)
char *name;	/* human-readable identifier for this process */
long flags;	/* for future use (should be 0) */
{
    strcpy(clientName, name);
    Fail_SendPredicate = StdSendPredicate;
    Fail_RecvPredicate = StdRecvPredicate;
    theFilters[(int)sendSide] = (FailFilter *) malloc(0);
    theFilters[(int)recvSide] = (FailFilter *) malloc(0);
    theQueues[(int)sendSide] = (int *) malloc(0);
    theQueues[(int)recvSide] = (int *) malloc(0);
    numFilters[0] = numFilters[1] = 0;
    Delay_Init();
    return 0;
}


/* Gets basic information about the server */

void Fail_GetInfo(name)
char *name;
{
    strcpy(name, clientName);
}


/* Routines for maintaining the filter list */

/* Insert a filter at position 'which', moving existing filters from
   'which' up */

int Fail_InsertFilter(side, id, filter)
FailFilterSide side;
int id;
FailFilter *filter;
{
    int which = -1;
    int i;

    if (id == 0) 
        which = 0;
    else
        for (i = 0; i < numFilters[(int)side]; i++)
            if (theFilters[(int)side][i].id == id)
                which = i+1;

    if (which < 0 || which > numFilters[(int)side])
	return -1;

    /* Currently, nothing is done to slow filters on the receive side. */
    if (side == recvSide && (filter->speed > 0) && (filter->speed < MAXNETSPEED))
	return -2;	/* -2 is a HACK */
    
    filter->id = FilterID++;
    numFilters[(int)side]++;
    theFilters[(int)side] =
	(FailFilter *) realloc(theFilters[(int)side],
			       numFilters[(int)side] * sizeof(FailFilter));
    theQueues[(int)side] = (int *)realloc(theQueues[(int)side],
					  numFilters[(int)side] * sizeof(int));
    
    if (which < numFilters[(int)side]) {
	/* Little known fact that bcopy checks for this sort of thing */
	bcopy(&theFilters[(int)side][which], &theFilters[(int)side][which+1],
	      (numFilters[(int)side] - which - 1) * sizeof(FailFilter));
	bcopy(&theQueues[(int)side][which], &theQueues[(int)side][which+1],
	      (numFilters[(int)side] - which - 1) * sizeof(int));
    }
    theFilters[(int)side][which] = *filter;

    /* Find the delay queue that applies to this filter */
    if (filter->speed < MAXNETSPEED) {
	int myq = FindQueue(filter->ip1, filter->ip2, filter->ip3, filter->ip4);
	
	if (myq == -1)
	    myq = MakeQueue(filter->ip1, filter->ip2, filter->ip3, filter->ip4);

	theQueues[(int)side][which] = myq;	
    }
TEMPDEBUG("InsertFilter!\n")
    return (filter->id);
}


/* Remove the filter with id 'id' */

int Fail_RemoveFilter(side, id)
FailFilterSide side;
int id;
{
    int which = -1;
    int i;

    for (i = 0; i < numFilters[(int)side]; i++)
        if (theFilters[(int)side][i].id == id)
            which = i;

    if (which < 0 || which >= numFilters[(int)side])
	return -1;


    /* DCS -- is this if statement necessary? I don't think so. */
    if (which < numFilters[(int)side] - 1) {
	bcopy(&theFilters[(int)side][which+1], &theFilters[(int)side][which],
	      (numFilters[(int)side] - which - 1) * sizeof(FailFilter));
	bcopy(&theQueues[(int)side][which+1], &theQueues[(int)side][which],
	      (numFilters[(int)side] - which - 1) * sizeof(int));
    }
    numFilters[(int)side]--;
    theFilters[(int)side] =
	(FailFilter *) realloc(theFilters[(int)side],
			       numFilters[(int)side] * sizeof(FailFilter));
    theQueues[(int)side] = (int *)realloc(theQueues[(int)side],
					  numFilters[(int)side] * sizeof(int));

#ifdef NOTDEF
    if (theQueues[(int)side][which]) {
	DecQueue(theQueues[(int)side][which], filter->ip1, filter->ip2, filter->ip3, filter->ip4);
	theQueues[(int)side][which] = 0;
#endif NOTDEF    
TEMPDEBUG("RemoveFilter!\n")
    return 0;
}

    
#include <stdio.h>
/* Remove all filters on a particular side */
int Fail_PurgeFilters(side)
     FailFilterSide side;
{
    switch (side) {
      case sendSide:
	theFilters[(int)sendSide] = (FailFilter *)realloc(0);
	numFilters[(int)sendSide] = 0;
#ifdef NOTDEF
	for (i = 0; i < numFilters[(int)sendSide]; i++)
	    if (theQueues[(int)sendSide][i]) {
		DecQueue(theQueues[(int)sendSide][i], filter->ip1, filter->ip2,
			 filter->ip3, filter->ip4);
		theQueues[(int)sendSide][i] = 0;
#endif NOTDEF
	theQueues[(int)sendSide] = (int *)realloc(0);
	break;

      case recvSide:
	theFilters[(int)recvSide] = (FailFilter *)realloc(0);
	numFilters[(int)recvSide] = 0;
#ifdef NOTDEF
	for (i = 0; i < numFilters[(int)recvside]; i++)
	    if (theQueues[(int)recvside][i]) {
		DecQueue(theQueues[(int)recvside][i], filter->ip1, filter->ip2,
			 filter->ip3, filter->ip4);
		theQueues[(int)recvside][i] = 0;
#endif NOTDEF    
	theQueues[(int)recvSide] = (int *)realloc(0);
	break;

      case noSide:
	theFilters[(int)sendSide] = (FailFilter *)realloc(0);
	numFilters[(int)sendSide] = 0;
	theFilters[(int)recvSide] = (FailFilter *)realloc(0);
	numFilters[(int)recvSide] = 0;
#ifdef NOTDEF
	for (i = 0; i < numFilters[(int)sendSide]; i++)
	    if (theQueues[(int)sendSide][i]) {
		DecQueue(theQueues[(int)sendSide][i], filter->ip1, filter->ip2,
			 filter->ip3, filter->ip4);
		theQueues[(int)sendSide][i] = 0;
	for (i = 0; i < numFilters[(int)recvside]; i++)
	    if (theQueues[(int)recvside][i]) {
		DecQueue(theQueues[(int)recvside][i], filter->ip1, filter->ip2,
			 filter->ip3, filter->ip4);
		theQueues[(int)recvside][i] = 0;
#endif NOTDEF
	theQueues[(int)sendSide] = (int *)realloc(0);
	theQueues[(int)recvSide] = (int *)realloc(0);
	break;

      default: {
	  int *x = 0;                                                     
	  fclose(stdout);                                                 
	  fprintf(stderr, "Assert at line \"%s\", line %d\n", __FILE__, __LINE__);
	  fclose(stderr);                                                 
	  *x = 1;  /* Cause a real SIGTRAP to happen, allow zombie */     
      }	
    }
    return 0;
}

/* Replace the filter with id 'id' */

int Fail_ReplaceFilter(side, id, filter)
FailFilterSide side;
int id;
FailFilter *filter;
{
    int which = -1;
    int i;

    for (i = 0; i < numFilters[(int)side]; i++)
        if (theFilters[(int)side][i].id == id)
            which = i;

    if (which < 0 || which >= numFilters[(int)side])
	return -1;
    theFilters[(int)side][which] = *filter;

#ifdef NOTDEF
    if (theQueues[(int)side][which]) {
	DecQueue(theQueues[(int)side][which], filter->ip1, filter->ip2, filter->ip3, filter->ip4);
	theQueues[(int)side][which] = 0;
#endif NOTDEF    

    /* Find the delay queue that applies to this filter */
    if (filter->speed < MAXNETSPEED) {
	int myq = FindQueue(filter->ip1, filter->ip2, filter->ip3, filter->ip4);
	
	if (myq == -1)
	    myq = MakeQueue(filter->ip1, filter->ip2, filter->ip3, filter->ip4);

	theQueues[(int)side][which] = myq;	
    }
	
TEMPDEBUG("ReplaceFilter!\n")
    return 0;
}


/* Return the filter array */

int Fail_GetFilters(side, filters)
FailFilterSide side;
RPC2_BoundedBS *filters;
{
    int ourlen;
    int i;
    FailFilter *ff;

    ourlen = numFilters[(int)side] * sizeof(FailFilter);
    if (filters->MaxSeqLen < ourlen)
	return -1;
    filters->SeqLen = ourlen;
    bcopy(theFilters[(int)side], filters->SeqBody, ourlen);
    for (i = 0; i < numFilters[(int)side]; i++) {
	ff = (FailFilter *)&(filters->SeqBody[i * sizeof(FailFilter)]);
	htonFF(ff);
    }
    return 0;
}

/* convert FailFilter to net order */

void htonFF(ff)
FailFilter *ff;
{
    int i;
    int *tmp;
    tmp = (int *)ff;
    for (i = 0; i < sizeof(FailFilter)/sizeof(RPC2_Integer); i++) 
	tmp[i] = htonl(tmp[i]);
}

/* convert FailFilter to host order */

void ntohFF(ff) 
FailFilter *ff;
{
    int i;
    int *tmp;
    tmp = (int *)ff;
    for (i = 0; i < sizeof(FailFilter)/sizeof(RPC2_Integer); i++)
	tmp[i] = ntohl(tmp[i]);
}

/* Return the number of filters */

int Fail_CountFilters(side)
FailFilterSide side;
{
    return numFilters[(int)side];
}


/* Utilities for packet filtering */

/* Determine whether a packet matches a filter */

PRIVATE int PacketMatch(filter, ip1, ip2, ip3, ip4, color, length)
FailFilter *filter;
unsigned char ip1, ip2, ip3, ip4, color;
int length;
{
    int result;

    result = ((filter->ip1 == -1 || ip1 == filter->ip1) &&
	      (filter->ip2 == -1 || ip2 == filter->ip2) &&
	      (filter->ip3 == -1 || ip3 == filter->ip3) &&
	      (filter->ip4 == -1 || ip4 == filter->ip4) &&
	      (filter->color == -1 || color == filter->color) &&
	      length >= filter->lenmin &&
	      length <= filter->lenmax);

DBG(("PacketMatch: %s\n", result ? "yes" : "no"))

    return result;
}


/* Determine whether to drop a packet based on its filter factor */

PRIVATE int FlipCoin(factor)
int factor;
{
    long random();

    return ((random() % MAXPROBABILITY) < factor);
}


/* Standard packet filters */

PRIVATE int StdSendPredicate(ip1, ip2, ip3, ip4, color, pb, addr, sock)
unsigned char ip1, ip2, ip3, ip4, color;
RPC2_PacketBuffer *pb;
struct sockaddr_in *addr;
int sock;
{
    int action, f, length;

    length = pb->Prefix.LengthOfPacket;

DBG(("StdSendPredicate: ip %d.%d.%d.%d color %d len %d\n",
     ip1, ip2, ip3, ip4, color, length))

    action = 2;		/* decide for ourselves by default */

    /* Give the user a chance to customize our behavior */
    if (Fail_UserSendPredicate)
	action = (*Fail_UserSendPredicate)
	    (ip1, ip2, ip3, ip4, color, pb, addr, sock);

    if (action == 2) {
	action = 1;		/* send by default */
	if (color != FAIL_IMMUNECOLOR)
	    for (f = 0; f < numFilters[(int)sendSide]; f++) {
		DBG(("Matching "));
#ifdef DEBUG_FAIL
		PrintFilter(&theFilters[(int)sendSide][f]);
#endif
		if (PacketMatch(&theFilters[(int)sendSide][f],
				ip1, ip2, ip3, ip4, color, length)) {
		    action = FlipCoin(theFilters[(int)sendSide][f].factor);
		    if (action != 0)
			    action = DelayPacket(theFilters[(int)sendSide][f].speed, 
						 sock, addr, pb, theQueues[(int)sendSide][f]);
		    break;
		}
	    }
    }

    return action;
}

PRIVATE int StdRecvPredicate(ip1, ip2, ip3, ip4, color, pb)
unsigned char ip1, ip2, ip3, ip4, color;
RPC2_PacketBuffer *pb;
/* we also have addr and sock, but we're ignoring them */
{
    int action, f, length;

    length = pb->Prefix.LengthOfPacket;

DBG(("StdRecvPredicate: ip %d.%d.%d.%d color %d len %d\n",
     ip1, ip2, ip3, ip4, color, length))

    action = 2;		/* decide for ourselves by default */

    /* Give the user a chance to customize our behavior */
    if (Fail_UserRecvPredicate)
	action = (*Fail_UserRecvPredicate)
	    (ip1, ip2, ip3, ip4, color, pb);

    if (action == 2) {
	action = 1;		/* send by default */
	if (color != FAIL_IMMUNECOLOR)
	    for (f = 0; f < numFilters[(int)recvSide]; f++) {
		DBG(("Matching "));
#ifdef DEBUG_FAIL
		PrintFilter(&theFilters[(int)recvSide][f]);
#endif
		if (PacketMatch(&theFilters[(int)recvSide][f],
				ip1, ip2, ip3, ip4, color, length)) {
		    action = FlipCoin(theFilters[(int)recvSide][f].factor);
		    break;
		}
	    }
    }

    return action;
}

