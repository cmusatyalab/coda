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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rpc2/lists.c,v 4.8 1998/11/24 15:34:35 jaharkes Exp $";
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __linux__
#include <search.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "rpc2.private.h"


/* Routines to allocate and manipulate the doubly-linked circular lists 
	used elsewhere in rpc2 */

void rpc2_Replenish(whichList, whichCount, elemSize, creationCount, magicNumber)
    struct LinkEntry  **whichList;
    long *whichCount;
    long elemSize;	/* size of each element in the list */
    long *creationCount;
    long magicNumber;
    /*  Routine to avoid using malloc() too often.  
	Assumes *whichList is empty and grows it by 1 entry of size elemSize.
	Sets *whichCount to 1.
	Bumps creationCount by 1.
    */
    {    
	   
    *whichList = (struct LinkEntry *)malloc(elemSize);
    CODA_ASSERT(*whichList != NULL);
    bzero(*whichList, elemSize);
    (*whichList)->NextEntry = (*whichList)->PrevEntry = *whichList; /* 1-element circular list */
    (*whichList)->MagicNumber = magicNumber;
    (*whichList)->Qname = whichList;
    *whichCount = 1;
    (*creationCount)++;
    }


/* Generic routine to move elements between lists Assumes p points to
   an entry in the list pointed to by fromPtr.  Moves that entry to
   the list pointed to by toPtr.  If p is NULL, an arbitrary entry in
   the from list is selected as a victim and moved.  If toPtr is NULL,
   the moved entry is made into a singleton list.  In all cases a
   pointer to the moved entry is returned as the value of the
   function.

	*fromCount is decremented by one.
	*toCount is incremented by one.

   Frequently used routine -- optimize the hell out of it.  */
struct LinkEntry *rpc2_MoveEntry(fromPtr, toPtr, p, fromCount, toCount)
    /* pointers to header pointers of from and to lists */
    struct LinkEntry **fromPtr, **toPtr;
    struct LinkEntry *p;	/* pointer to entry to be moved */
    long *fromCount;		/* pointer to count of entries in from list */
    long *toCount;		/* pointer to count of entries in to list */

{
    struct LinkEntry *victim;

    if (p == NULL) 
	    victim = *fromPtr;	
    else 
	    victim = p;
    CODA_ASSERT(victim->Qname == fromPtr);    /* sanity check for list corruption */

    /* first remove element from the first list */
    if (victim == *fromPtr) 
	    *fromPtr = victim->NextEntry;
    remque(victim);
    if (victim == *fromPtr) 
	    *fromPtr = NULL;
    (*fromCount)--;

    /* make victim a singleton list */
    victim->NextEntry = victim->PrevEntry = victim;

    /* then insert into second list */
    if (*toPtr == NULL) 
	    *toPtr = victim;
    else 
	    insque(victim, (*toPtr)->PrevEntry);
	/* PrevEntry because semantics of insque() causes non-FIFO queue */
    victim->Qname = toPtr;
    (*toCount)++;
    return(victim);
}

/* Allocates an SL entry and binds it to slConn */
struct SL_Entry *rpc2_AllocSle(enum SL_Type slType, struct CEntry *slConn)
{
	struct SL_Entry *sl, **tolist;
	long *tocount;
	
	if (rpc2_SLFreeCount == 0)
		{
			rpc2_Replenish((struct LinkEntry **) &rpc2_SLFreeList, &rpc2_SLFreeCount,
				       sizeof(struct SL_Entry), &rpc2_SLCreationCount, OBJ_SLENTRY);
	}

    if (slType == REQ)
	{
	tolist = &rpc2_SLReqList;
	tocount = &rpc2_SLReqCount;
	}
    else
	{
	tolist = &rpc2_SLList;
	tocount = &rpc2_SLCount;
	}

    sl = (struct SL_Entry *)rpc2_MoveEntry((struct LinkEntry **)&rpc2_SLFreeList,
	    (struct LinkEntry **)tolist, NULL, &rpc2_SLFreeCount, tocount);

    CODA_ASSERT(sl->MagicNumber == OBJ_SLENTRY);
    sl->Type = slType;
    if (slType != REQ && slConn != NULL) {
	    slConn->MySl = sl;
	    sl->Conn = slConn->UniqueCID;
    }   else 
	    sl->Conn = 0;

    return(sl);
    }

void rpc2_FreeSle(INOUT sl)
    struct SL_Entry **sl;
    /* Releases the SL_Entry pointed to by sl. Sets sl to NULL.
       Removes binding between sl and its connection */
 
    {
    struct SL_Entry *tsl, **fromlist;
    long *fromcount;
    struct CEntry *ce;
    
    tsl = *sl;
    CODA_ASSERT(tsl->MagicNumber == OBJ_SLENTRY);

    if (tsl->Conn != 0)
	{
	ce = rpc2_FindCEAddr(tsl->Conn);
        CODA_ASSERT(ce != NULL);
	ce->MySl = NULL;
	}

    if (tsl->Type == REQ)
	{
	fromlist = &rpc2_SLReqList;
	fromcount = &rpc2_SLReqCount;
	}
    else
	{
	fromlist = &rpc2_SLList;
	fromcount = &rpc2_SLCount;
	}

    rpc2_MoveEntry((struct LinkEntry **)fromlist,
			(struct LinkEntry **)&rpc2_SLFreeList,
			(struct LinkEntry *)tsl, fromcount, &rpc2_SLFreeCount);
    *sl = NULL;
    }

void rpc2_ActivateSle (selem, exptime)
    struct SL_Entry *selem;
    struct timeval *exptime;
    {
    struct TM_Elem *t, *oldt;
    long delta;

    CODA_ASSERT(selem->MagicNumber == OBJ_SLENTRY);
    selem->TElem.BackPointer = (char *)selem;
    selem->ReturnCode = WAITING;

    t = &selem->TElem;

    if (exptime == NULL)
    	{/* infinite timeout, don't add to timer chain */
	t->TotalTime.tv_sec = -1;
	t->TotalTime.tv_usec = -1;
	return;
	}

    t->TotalTime = *exptime; /* structure assignment */

    oldt = TM_GetEarliest(rpc2_TimerQueue);
    if (oldt != NULL)
	{
		/* remove oldt if it isn't the soonest anymore */
	delta = (oldt->TimeLeft.tv_sec - t->TotalTime.tv_sec)*1000000 +
		    (oldt->TimeLeft.tv_usec - t->TotalTime.tv_usec);
	if (delta > 0) 	IOMGR_Cancel(rpc2_SocketListenerPID);
	}
    else  /* why the heck remove something that isn't there? */
	    IOMGR_Cancel(rpc2_SocketListenerPID);
    TM_Insert(rpc2_TimerQueue, t);
    }

void rpc2_DeactivateSle(sl, rc)
    struct SL_Entry *sl;
    enum RetVal rc;
    {
    struct timeval *t;

    CODA_ASSERT(sl->MagicNumber == OBJ_SLENTRY);

    sl->ReturnCode = rc;
    t = &sl->TElem.TotalTime;
    if (t->tv_sec == -1 && t->tv_usec == -1) return; /* not timed */
    else {
	TM_Remove(rpc2_TimerQueue, &sl->TElem);
	t->tv_sec = t->tv_usec = -1;	/* keep routine idempotent */
    }
    }


struct SubsysEntry *rpc2_AllocSubsys()
    /* Allocates a new subsystem entry and returns a pointer to it.
    	Returns NULL if unable to allocate such an entry.
    */
    {
    struct SubsysEntry *ss;
    if (rpc2_SSFreeCount == 0)
    	rpc2_Replenish((struct LinkEntry **)&rpc2_SSFreeList,
		&rpc2_SSFreeCount, sizeof(struct SubsysEntry),
		&rpc2_SSCreationCount, OBJ_SSENTRY);
    ss = (struct SubsysEntry *)rpc2_MoveEntry((struct LinkEntry **)&rpc2_SSFreeList,
	 (struct LinkEntry **)&rpc2_SSList, NULL, &rpc2_SSFreeCount, &rpc2_SSCount);
    CODA_ASSERT(ss->MagicNumber == OBJ_SSENTRY);
    return(ss);
    }

void rpc2_FreeSubsys(whichSubsys)
    struct SubsysEntry **whichSubsys;
    /* Releases the subsystem  entry pointed to by whichSubsys.
	Sets whichSubsys to NULL;  */
	{
	CODA_ASSERT((*whichSubsys)->MagicNumber == OBJ_SSENTRY);
	rpc2_MoveEntry((struct LinkEntry **)&rpc2_SSList,
		    (struct LinkEntry **)&rpc2_SSFreeList,
		    (struct LinkEntry *)*whichSubsys,
		    &rpc2_SSCount, &rpc2_SSFreeCount);
	*whichSubsys = NULL;
	}



/* Moves packet whichPB to hold list from inuse list */
void rpc2_HoldPacket(RPC2_PacketBuffer *whichPB)
{
	CODA_ASSERT(whichPB->Prefix.MagicNumber == OBJ_PACKETBUFFER);
	rpc2_MoveEntry((struct LinkEntry **)&rpc2_PBList,
		       (struct LinkEntry **)&rpc2_PBHoldList,
		       (struct LinkEntry *)whichPB,
		       &rpc2_PBCount, &rpc2_PBHoldCount);
	if (rpc2_HoldHWMark < rpc2_PBHoldCount) 
		rpc2_HoldHWMark = rpc2_PBHoldCount;
}

/* Moves packet whichPB to inuse list from hold list */
void rpc2_UnholdPacket(RPC2_PacketBuffer *whichPB)
{
	CODA_ASSERT(whichPB->Prefix.MagicNumber == OBJ_PACKETBUFFER);
	rpc2_MoveEntry((struct LinkEntry **)&rpc2_PBHoldList,
		       (struct LinkEntry **)&rpc2_PBList,
		       (struct LinkEntry *)whichPB,
		       &rpc2_PBHoldCount, &rpc2_PBCount);
}


