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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <assert.h>
#include "rpc2.private.h"

/* HASHLENGTH should be a power of two, because we use modulo HASHLENGTH-1 to
 * find the appropriate hash bucket */
#define HASHLENGTH 512

/* the hash table of size HASHLEN buckets */
static struct dllist_head HashTable[HASHLENGTH];

static long EntriesInUse = -1;

void rpc2_InitConn()
{
    int i;
    
    /* safety check, never initialize twice */
    assert(EntriesInUse == -1);

    for (i = 0; i < HASHLENGTH; i++)
    {
        list_head_init(&HashTable[i]);
    }

    EntriesInUse = 0;
}

/* Returns pointer to the connection data structure corresponding to
   whichHandle.  Returns NULL if whichHandle does not refer to an
   existing connection.  */
struct CEntry *rpc2_GetConn(RPC2_Handle handle)
{
    long                i;
    struct dllist_head *ptr;
    struct CEntry      *ceaddr;

    if (handle == 0) return(NULL);

    /* bucket is handle modulo HASHLENGTH */
    i= handle & (HASHLENGTH-1);	

    /* and walk the chain */
    for (ptr = HashTable[i].next; ptr != &HashTable[i]; ptr = ptr->next)
    {
        /* compare the entry to our handle */
        ceaddr = list_entry(ptr, struct CEntry, Chain);
        assert(ceaddr->MagicNumber == OBJ_CENTRY);

        if (ceaddr->UniqueCID == handle) 
        {
            /* we are likely to see more lookups for this CEntry, so put it at
             * the front of the chain */
            list_del(ptr); list_add(ptr, &HashTable[i]);

	    /* keep the grim reaper out */
	    ceaddr->LastRef = time(NULL);
            return (ceaddr);
        }
    }

    return (NULL);
}


/* Allocates a new handle corresponding to ceaddr, and sets the
   UniqueCID field of ceaddr. */
static void Uniquefy(IN struct CEntry *ceaddr)
{
    long handle, index;

    /* rpc2_NextRandom will only return int's up to 2^30, effectively we will
     * have broken down before we use this many entries on either the time it
     * takes to find an available handle, or the memory usage. Still, I don't
     * want this function to get stuck into an endless search. --JH */
    assert(EntriesInUse < (1073741824 >> 1)); /* 50% utilization */

    /* this might take some time once we get a lot of used handles. But even
     * with a `full' table (within the constraint above), we should, on
     * average, find a free handle after walking two chains. Advice for those
     * who are afraid of long bucket chain lookups: increase HASHLENGTH */
    do
    {
        handle = rpc2_NextRandom(NULL);

	/* skip the handles which have special meaning */
	if (handle == 0 || handle == -1)
	    continue;
    }
    while(rpc2_GetConn(handle) != NULL);

    /* set the handle */
    ceaddr->UniqueCID = handle;

    /* add to the bucket */
    index = handle & (HASHLENGTH-1);
    list_add(&ceaddr->Chain, &HashTable[index]);
    
    EntriesInUse++;
}


struct CEntry *rpc2_AllocConn()
{
    struct CEntry *ce;

    rpc2_AllocConns++;
    if (rpc2_ConnFreeCount == 0)
        rpc2_Replenish(&rpc2_ConnFreeList, &rpc2_ConnFreeCount,
                       sizeof(struct CEntry), &rpc2_ConnCreationCount,
                       OBJ_CENTRY);

    ce = (struct CEntry *)rpc2_MoveEntry(&rpc2_ConnFreeList, &rpc2_ConnList,
		 (struct CEntry *)NULL, &rpc2_ConnFreeCount, &rpc2_ConnCount);
    assert (ce->MagicNumber == OBJ_CENTRY);

    /* Initialize */
    ce->State = 0;
    ce->UniqueCID = 0;
    ce->NextSeqNumber = 0;
    ce->SubsysId = 0;
    list_head_init(&ce->Chain);
    ce->Flags = 0;
    ce->SecurityLevel = 0;
    ce->EncryptionType = 0;
    ce->PeerHandle = 0;
    ce->PeerUnique = 0;
    ce->LastRef = time(NULL);
    ce->SEProcs = NULL;
    ce->sebroken = 0;
    ce->Mgrp = (struct MEntry *)NULL;
    ce->PrivatePtr = NULL;
    ce->SideEffectPtr = NULL;
    ce->Color = 0;
    ce->RTT = 0;
    ce->RTTVar = 0;
    ce->LowerLimit= LOWERLIMIT;  /* usec */
    ce->Retry_N = Retry_N;
    ce->Retry_Beta = (struct timeval *)malloc(sizeof(struct timeval)*(2+Retry_N));
    memcpy(ce->Retry_Beta, Retry_Beta, sizeof(struct timeval)*(2+Retry_N));
    ce->SaveResponse = SaveResponse;  /* structure assignment */
    ce->MySl = NULL;
    ce->HeldPacket = NULL;
    ce->reqsize = 0;

    /* Then make it unique */
    Uniquefy(ce);

    return(ce);
}

/* Frees the connection whichConn */
void rpc2_FreeConn(RPC2_Handle whichConn)
{
    long i;
    RPC2_PacketBuffer *pb;
    struct CEntry *ce;

    ce = rpc2_GetConn(whichConn);
    assert(ce != NULL);
    assert(ce->MagicNumber == OBJ_CENTRY);
    rpc2_FreeConns++;
    list_del(&ce->Chain);

    free(ce->Retry_Beta);
    if (ce->HeldPacket != NULL)
	RPC2_FreeBuffer(&ce->HeldPacket);
    if (ce->MySl != NULL) {
	rpc2_DeactivateSle(ce->MySl);
    	rpc2_FreeSle(&ce->MySl);
    }

    /* Scan the hold queue and purge the request for this connection */
    pb=rpc2_PBHoldList;
    for (i = 0; i < rpc2_PBHoldCount; i++) {
	    if (pb->Header.RemoteHandle == ce->UniqueCID) {
		    say(9, RPC2_DebugLevel, "Purging request from hold queue\n");
		    rpc2_UnholdPacket(pb);
		    RPC2_FreeBuffer(&pb);
		    break;  /* there can be at most one in hold queue (RPC) */	    
	    }
    }

    SetRole(ce, FREE);
    rpc2_MoveEntry(&rpc2_ConnList, &rpc2_ConnFreeList, ce,
			&rpc2_ConnCount, &rpc2_ConnFreeCount);
    EntriesInUse--;
}

/* Reap connections that have not seen any activity in the past 15 minutes */
#define RPC2_DEAD_CONN_TIMEOUT 900
void rpc2_ReapDeadConns(void)
{
	struct CEntry *ce, *next;
	time_t now;

	now = time(NULL);

	next = (struct CEntry *)(rpc2_ConnList->NextEntry);

	while(next != (struct CEntry *)rpc2_ConnList) {
		ce = next; next = ce->NextEntry;
		
		if (!ce->PrivatePtr &&
		    ce->LastRef + RPC2_DEAD_CONN_TIMEOUT < now) {
			say(0, RPC2_DebugLevel, "Reaping dead connection %ld\n",
			    ce->UniqueCID);
			RPC2_Unbind(ce->UniqueCID);
		}
	}
}

#if 0
static void PrintHashTable()
{
    struct dllist_head *ptr;
    struct CEntry      *ce;
    long                i;

    printf("EntriesInUse = %ld\tHASHLENGTH = %d\n", EntriesInUse, HASHLENGTH);

    for (i=0; i < HASHLENGTH; i++)
    {
        printf("HashTable[%ld] =", i);

        for (ptr = HashTable[i].next; ptr != &HashTable[i]; ptr = ptr->next)
        {
            ce = list_entry(ptr, struct CEntry, Chain);
            assert(ce->MagicNumber == OBJ_CENTRY);

            printf(" %s:%d:%d",
		   inet_ntoa(ce->PeerHost.Value.InetAddress),
		   ntohl(ce->PeerPort.Value.InetPortNumber),
		   ntohl(ce->PeerUnique));
        }

        printf("\n");
    }
}
#endif

void rpc2_SetConnError(IN struct CEntry *ce)
{
    assert (ce->MagicNumber == OBJ_CENTRY);

    if (TestRole(ce, SERVER)) 
	    SetState(ce, S_HARDERROR);
    else 
	    SetState(ce, C_HARDERROR);
    
    /* RC should be LWP_SUCCESS or LWP_ENOWAIT */
    LWP_NoYieldSignal((char *)ce);
}


/* Code to Map Retried Bind Requests to Existing Connections  */

/* All packets other than Init1 requests have a LocalHandle field
which is valid.  On Init1 we do not have a local handle yet.  Each
Init1 packet and its retries have a truly random Uniquefier, generated
by the client.  The retries also have the RETRY bit set in the packet
headers.  The triple (Host,Port,Uniquefier) is totally unique even
across client reboots.
    
In the worst case the mapping involves a linear search of the
connection list.  With 1000 connections this took about 60
milliseconds on a SUN2.  In practice, many of these connections will
not be server-end connections so the test will be shorter.  However,
to speed up the lookup we use a trivial LRU cache of recent bind
completions.  The RBCache is essentially a way to focus attention on a
small subset of the entire connection list.

To conserve storage we allocate the RBCache only if the number of
connections exceeds a certain threshold.  Setting RPC2_Small
suppresses the RBCache mechanism altogether.

If this doesn't work well enough we may have to go to a hash table
data structure that maps each (host, port, uniquefier) triple to a
connection handle.  That will almost certainly be more complex to
build and maintain.  */


struct RecentBind
{
    RPC2_HostIdent Host;	/* Remote Host */
    RPC2_PortIdent Port;	/* Remote Port */
    RPC2_Integer Unique;	/* Uniquefier value in Init1 packet */
    RPC2_Handle MyConn;		/* Local handle allocated for this connection */
};

#define RBSIZE 300	/* max size of RBCache for large RPC */
#define RBCACHE_THRESHOLD 50	/* RBCache never used for less than RBCACHE_TRESHOLD connections */
static struct RecentBind *RBCache;	/* Wraps around; reused in LRU order.
					Conditionally allocated. */
static int RBWrapped = 0;	/* RBCache is full and has wrapped around */
static int NextRB = 0;		/* Index of entry to be used for the next bind */
static int RBCacheOn = 0;	/* 0 = RBCacheOff, 1 = RBCacheOn */

/* Adds information about a new bind to the RBCache; throws out the
   oldest entry if needed */
void rpc2_NoteBinding(RPC2_HostIdent *whichHost, RPC2_PortIdent *whichPort, 
		 RPC2_Integer whichUnique, RPC2_Handle whichConn)
{
    if (rpc2_ConnCount <= RBCACHE_THRESHOLD)
            return;

    if (!RBCacheOn) {
	    /* first use of RBCache- must allocate cache */
	    RBCache = (struct RecentBind *) malloc(RBSIZE * sizeof(struct RecentBind));
	    RBCacheOn = 1;
    }

    memset(&RBCache[NextRB], 0, sizeof(struct RecentBind));
    RBCache[NextRB].Host = *whichHost;	/* struct assignment */
    RBCache[NextRB].Port = *whichPort;	/* struct assignment */
    RBCache[NextRB].Unique = whichUnique;
    RBCache[NextRB].MyConn = whichConn;
    
    NextRB++;
    if (NextRB >= RBSIZE) {
	    RBWrapped = 1;
	    NextRB = 0;
    }
}

/* Identifies the connection corr to (whichHost, whichPort) with
   uniquefier whichUnique, if it exists.  Returns the address of the
   connection block, or NULL if no such binding exists.  The code
   first looks in RBCache[] and if that fails, it walks the connection
   list.    */

struct CEntry *
rpc2_ConnFromBindInfo(RPC2_HostIdent *whichHost, RPC2_PortIdent *whichPort, 
		      RPC2_Integer whichUnique)
{
    struct RecentBind *rbn;
    int next, count;
    struct CEntry *ce;
    struct dllist_head *ptr;
    int i, j;
    
    /* If RBCache is being used, check it first; search it backwards,
     * to increase chances of hit on recent binds.  */

    if (RBCacheOn) {
	    next = (NextRB == 0) ? RBSIZE - 1 : NextRB - 1;
	    if (RBWrapped) 
		    count = RBSIZE;
	    else 
		    count = NextRB;
	    i = 0;

	    while (i < count) {
		    rbn = &RBCache[next];
		    /* do cheapest test first */
		    if (rbn->Unique == whichUnique		
			&& rpc2_HostIdentEqual(&rbn->Host, whichHost)
			&& rpc2_PortIdentEqual(&rbn->Port, whichPort)) {
			    say(0, RPC2_DebugLevel, "RBCache hit after %d tries\n", i+1);
			    return(rpc2_GetConn(rbn->MyConn));
		    }

	    /* Else bump counters and try previous one */
		    i++;
		    if (next == 0) 
			    next = RBSIZE - 1;
		    else next--;
	    }
	    
	    say(0, RPC2_DebugLevel, "RBCache miss after %d tries\n", RBSIZE);
    }
    
    /* It was not in the RBCache; scan all the connections */
    
    for (i=0, j=0; i < HASHLENGTH; i++)
    {
        for (ptr = HashTable[i].next; ptr != &HashTable[i]; ptr = ptr->next)
        {
            ce = list_entry(ptr, struct CEntry, Chain);
            assert(ce->MagicNumber == OBJ_CENTRY);

            j++; /* count # searched connections */

            if (ce->PeerUnique == whichUnique  /* do cheapest test first */
                && rpc2_HostIdentEqual(&ce->PeerHost, whichHost)
                && rpc2_PortIdentEqual(&ce->PeerPort, whichPort))
            {
                say(0, RPC2_DebugLevel, "Match after searching %d connection entries\n", j);
		list_del(ptr); list_add(ptr, &HashTable[i]);
                return(ce);
            }
        }
    }
    say(0, RPC2_DebugLevel, "No match after searching %ld connections\n",
        rpc2_ConnCount);

    return(NULL);
}
