/* BLURB lgpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <assert.h>

#include "rpc2.private.h"

/* various free and in-use lists */
static struct RPC2_LinkEntry *rpc2_SLFreeList, /* free SL_Entry entries */
    *rpc2_SLList; /* in use, of types REPLY or OTHER */
static struct RPC2_LinkEntry *rpc2_SSFreeList; /* free SubsysEntry entries */

/* Routines to allocate and manipulate the doubly-linked circular lists
   used elsewhere in rpc2 */

void rpc2_Replenish(struct RPC2_LinkEntry **whichList, long *whichCount,
                    long elemSize, /* size of each element in the list */
                    long *creationCount, long magicNumber)
/* Routine to avoid using malloc() too often.
   Assumes *whichList is empty and grows it by 1 entry of size elemSize.
   Sets *whichCount to 1.
   Bumps creationCount by 1.
*/
{
    *whichList = (struct RPC2_LinkEntry *)malloc(elemSize);
    assert(*whichList != NULL);
    memset(*whichList, 0, elemSize);
    (*whichList)->Next = (*whichList)->Prev =
        *whichList; /* 1-element circular list */
    (*whichList)->MagicNumber = magicNumber;
    (*whichList)->Queue       = whichList;
    *whichCount               = 1;
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
struct RPC2_LinkEntry *rpc2_MoveEntry(
    /* pointers to header pointers of from and to lists */
    struct RPC2_LinkEntry **fromPtr, struct RPC2_LinkEntry **toPtr,
    struct RPC2_LinkEntry *p, /* pointer to entry to be moved */
    long *fromCount, /* pointer to count of entries in from list */
    long *toCount /* pointer to count of entries in to list */
)
{
    struct RPC2_LinkEntry *victim;

    if (p == NULL)
        victim = *fromPtr;
    else
        victim = p;
    assert(victim->Queue == fromPtr); /* sanity check for list corruption */

    /* first remove element from the first list */
    if (victim == *fromPtr)
        *fromPtr = victim->Next;

    /* remque(victim); */
    victim->Prev->Next = victim->Next;
    victim->Next->Prev = victim->Prev;

    /* make victim a singleton list */
    victim->Prev = victim->Next = victim;

    if (victim == *fromPtr)
        *fromPtr = NULL;
    (*fromCount)--;

    /* then insert into second list */
    if (*toPtr == NULL)
        *toPtr = victim;
    else {
        /* PrevEntry because semantics of insque() causes non-FIFO queue */
        /* insque(victim, (*toPtr)->PrevEntry); */
        victim->Prev         = (*toPtr)->Prev;
        victim->Next         = *toPtr;
        (*toPtr)->Prev->Next = victim;
        (*toPtr)->Prev       = victim;
    }
    victim->Queue = toPtr;
    (*toCount)++;
    return (victim);
}

/* Allocates an SL entry and binds it to slConn */
struct SL_Entry *rpc2_AllocSle(enum SL_Type slType, struct CEntry *slConn)
{
    struct SL_Entry *sl;
    struct RPC2_LinkEntry **tolist;
    long *tocount;

    if (rpc2_SLFreeCount == 0) {
        rpc2_Replenish(&rpc2_SLFreeList, &rpc2_SLFreeCount,
                       sizeof(struct SL_Entry), &rpc2_SLCreationCount,
                       OBJ_SLENTRY);
    }

    if (slType == REQ) {
        tolist  = &rpc2_SLReqList;
        tocount = &rpc2_SLReqCount;
    } else {
        tolist  = &rpc2_SLList;
        tocount = &rpc2_SLCount;
    }

    sl = rpc2_LE2SL(rpc2_MoveEntry(&rpc2_SLFreeList, tolist, NULL,
                                   &rpc2_SLFreeCount, tocount));

    sl->Type = slType;
    if (slType != REQ && slConn != NULL) {
        slConn->MySl = sl;
        sl->Conn     = slConn->UniqueCID;
    } else
        sl->Conn = 0;

    return sl;
}

void rpc2_FreeSle(INOUT struct SL_Entry **sl)
/* Releases the SL_Entry pointed to by sl. Sets sl to NULL.
   Removes binding between sl and its connection */
{
    struct SL_Entry *tsl;
    struct RPC2_LinkEntry **fromlist;
    long *fromcount;
    struct CEntry *ce;

    tsl = *sl;
    assert(tsl->LE.MagicNumber == OBJ_SLENTRY);

    if (tsl->Conn != 0) {
        ce = __rpc2_GetConn(tsl->Conn);
        if (ce)
            ce->MySl = NULL;
    }

    if (tsl->Type == REQ) {
        fromlist  = &rpc2_SLReqList;
        fromcount = &rpc2_SLReqCount;
    } else {
        fromlist  = &rpc2_SLList;
        fromcount = &rpc2_SLCount;
    }

    rpc2_MoveEntry(fromlist, &rpc2_SLFreeList, &tsl->LE, fromcount,
                   &rpc2_SLFreeCount);
    *sl = NULL;
}

void rpc2_ActivateSle(struct SL_Entry *selem, struct timeval *exptime)
{
    struct TM_Elem *t, *oldt;

    assert(selem->LE.MagicNumber == OBJ_SLENTRY);
    selem->TElem.BackPointer = (char *)selem;
    selem->ReturnCode        = WAITING;

    t = &selem->TElem;

    if (exptime == NULL) { /* infinite timeout, don't add to timer chain */
        t->TotalTime.tv_sec  = -1;
        t->TotalTime.tv_usec = -1;
        return;
    }

    t->TotalTime = *exptime; /* structure assignment */

    oldt = TM_GetEarliest(rpc2_TimerQueue);
    /* if the new entry expires before any previous timeout, signal the socket
     * listener to recheck the timerqueue (being able to rely on the
     * availability of timercmp would be nice) */
    if (!oldt || oldt->TimeLeft.tv_sec > t->TotalTime.tv_sec ||
        (oldt->TimeLeft.tv_sec == t->TotalTime.tv_sec &&
         oldt->TimeLeft.tv_usec > t->TotalTime.tv_usec))
        IOMGR_Cancel(rpc2_SocketListenerPID);

    TM_Insert(rpc2_TimerQueue, t);
}

void rpc2_DeactivateSle(struct SL_Entry *sl, enum RetVal rc)
{
    struct timeval *t;

    assert(sl->LE.MagicNumber == OBJ_SLENTRY);

    sl->ReturnCode = rc;
    t              = &sl->TElem.TotalTime;
    if (t->tv_sec == -1 && t->tv_usec == -1)
        return; /* not timed */
    else {
        TM_Remove(rpc2_TimerQueue, &sl->TElem);
        t->tv_sec = t->tv_usec = -1; /* keep routine idempotent */
    }
}

struct SubsysEntry *rpc2_AllocSubsys()
/* Allocates a new subsystem entry and returns a pointer to it.
   Returns NULL if unable to allocate such an entry.
*/
{
    struct SubsysEntry *ss;
    if (rpc2_SSFreeCount == 0)
        rpc2_Replenish(&rpc2_SSFreeList, &rpc2_SSFreeCount,
                       sizeof(struct SubsysEntry), &rpc2_SSCreationCount,
                       OBJ_SSENTRY);
    ss = rpc2_LE2SS(rpc2_MoveEntry(&rpc2_SSFreeList, &rpc2_SSList, NULL,
                                   &rpc2_SSFreeCount, &rpc2_SSCount));
    return ss;
}

void rpc2_FreeSubsys(struct SubsysEntry **whichSubsys)
/* Releases the subsystem  entry pointed to by whichSubsys.
   Sets whichSubsys to NULL;  */
{
    assert((*whichSubsys)->LE.MagicNumber == OBJ_SSENTRY);
    rpc2_MoveEntry(&rpc2_SSList, &rpc2_SSFreeList, &(*whichSubsys)->LE,
                   &rpc2_SSCount, &rpc2_SSFreeCount);
    *whichSubsys = NULL;
}

/* Moves packet whichPB to hold list from inuse list */
void rpc2_HoldPacket(RPC2_PacketBuffer *whichPB)
{
    assert(whichPB->LE.MagicNumber == OBJ_PACKETBUFFER);
    rpc2_MoveEntry(&rpc2_PBList, &rpc2_PBHoldList, &whichPB->LE, &rpc2_PBCount,
                   &rpc2_PBHoldCount);
    if (rpc2_HoldHWMark < rpc2_PBHoldCount)
        rpc2_HoldHWMark = rpc2_PBHoldCount;
}

/* Moves packet whichPB to inuse list from hold list */
void rpc2_UnholdPacket(RPC2_PacketBuffer *whichPB)
{
    assert(whichPB->LE.MagicNumber == OBJ_PACKETBUFFER);
    rpc2_MoveEntry(&rpc2_PBHoldList, &rpc2_PBList, &whichPB->LE,
                   &rpc2_PBHoldCount, &rpc2_PBCount);
}
