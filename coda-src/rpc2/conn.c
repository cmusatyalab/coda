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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "rpc2.private.h"


/* This code assumes that longs are 32 bits in length */
#define  MAXHASHLENGTH  65536	/* of hash table; 2**16; max possible number of connections concurrently in use  */
#define  INITHASHLENGTH 64	/* of hash table; should be power of two; small for testing */
PRIVATE long CurrentLength;	/* of hash table; should be power of two; never decreases */
PRIVATE long EntriesInUse;
PRIVATE RPC2_Handle LastHandleAllocated;

PRIVATE struct CEntry **HashTable;	/* to malloc'ed hash table of size CurrentLength entries */


/* Hash algorithm (courtesy Bob Sidebotham):
	Start table at INITHASHLENGTH and realloc() it, as needed, in powers of two upto MAXHASHLENGTH.
	Use modulo function to compute hash value, but always do it modulo MAXHASHLENGTH, not 
	modulo CurrentLength.  This guarantees that hash values will not change as table gets realloc()ed.
	Computing modulo is cheap since MAXHASHLENGTH is a power of two ==> mask with (MAXHASHLENGTH-1).

	We avoid clashes altogether, since the legal input keys are always created by us.
	If a potential handle would cause a clash, never allocate that handle, but try sequentially thereafter
	    until a suitable one is found.
	If a potential handle would  yield a hash value in the region (MAXHASHLENGTH-CurrentLength), we have two
	    choices: wrap around and try from the beginning or extend the hashtable to the next power of two.
	    Which one we choose depends on what fraction of the current slots are in use.  If this is high we realloc;
	    else we wrap around.

	Limitations: this hash table can allocate at most MAXHASHLENGTH conncurrent connections; over the life of the
	program, however, at most 2**30 entries can be allocated before connection ids wrap around via 0.
*/


void rpc2_InitConn()
    {
    assert((HashTable = (struct CEntry **)malloc(INITHASHLENGTH*sizeof(struct CEntry *))) != NULL);
    bzero(HashTable, INITHASHLENGTH*sizeof(struct CEntry *));	/* all entries initially NULL */
    CurrentLength = INITHASHLENGTH;
    LastHandleAllocated = 0;
    EntriesInUse = 0;
    /* all entries in table have NULL value; malloc() guarantees this */
    }

struct CEntry *rpc2_FindCEAddr(IN whichHandle)
    register RPC2_Handle whichHandle;
    /* Returns pointer to the connection data structure corresponding to whichHandle.
	    Returns NULL if whichHandle does not refer to an existing connection.
    */
    {
    register long index;
    register struct CEntry *ceaddr;
    index = whichHandle & (MAXHASHLENGTH-1);	/* modulo MAXHASHLENGTH */
    if (whichHandle == NULL) return(NULL);
    if (index > CurrentLength-1) return(NULL);
    if ((ceaddr = HashTable[index]) == NULL) return (NULL);
    if (ceaddr->UniqueCID != whichHandle) return (NULL);
    else return(ceaddr);
    }


PRIVATE void Uniquefy(IN ceaddr)
    register struct CEntry *ceaddr;
    /* Allocates a new handle corresponding to ceaddr, and sets the UniqueCID field of ceaddr. */
    {
    register long i;
    long first = (++LastHandleAllocated) & (MAXHASHLENGTH-1);
	       /* very first entry to try; only assignment to it is here */

    for (i = first; i < CurrentLength; i++)
	if (HashTable[i] == NULL)
	    {
	    HashTable[i] = ceaddr; 
	    LastHandleAllocated += i - first;
	    ceaddr->UniqueCID = LastHandleAllocated;
	    EntriesInUse++;
	    return;
	    }
    /* Decide here to realloc() or wrap around */
    if (EntriesInUse < CurrentLength/2 || CurrentLength == MAXHASHLENGTH)
	{/* wrap around */
	LastHandleAllocated = ((LastHandleAllocated >> 16)+1) << 16; /* Next power of two in top half */
	for (i = 0; i < CurrentLength; i++)
	    if (HashTable[i] == NULL)
		{
		HashTable[i] = ceaddr;
		LastHandleAllocated += i;
		ceaddr->UniqueCID = LastHandleAllocated;
		EntriesInUse++;
		return;
		}
	assert(1 == 0);	/* we should never get here! */
	}
    else
	{/* realloc() */
	say(9, RPC2_DebugLevel, ("Reallocing HashTable with EntriesInUse = %ld and CurrentLength = %ld\n",  EntriesInUse, CurrentLength));
	assert((HashTable = (struct CEntry **)realloc(HashTable, 2*CurrentLength*sizeof(struct CEntry *))) != NULL);
	bzero(&HashTable[CurrentLength], CurrentLength*sizeof(struct CEntry *));	/* zero out new allocation */
	HashTable[CurrentLength] = ceaddr;
	LastHandleAllocated += CurrentLength - first;
	ceaddr->UniqueCID = LastHandleAllocated;
	EntriesInUse++;
	CurrentLength *= 2;
	return;
	}
    
    }


struct CEntry *rpc2_AllocConn()
    {
    struct CEntry *ce;
    
    rpc2_AllocConns++;
    if (rpc2_ConnFreeCount == 0)
	    rpc2_Replenish(&rpc2_ConnFreeList, &rpc2_ConnFreeCount, sizeof(struct CEntry),
	    &rpc2_ConnCreationCount, OBJ_CENTRY);

    ce = (struct CEntry *)rpc2_MoveEntry(&rpc2_ConnFreeList, &rpc2_ConnList,
		 (struct CEntry *)NULL, &rpc2_ConnFreeCount, &rpc2_ConnCount);
    assert (ce->MagicNumber == OBJ_CENTRY);

    /* Initialize */
    ce->State = 0;
    ce->UniqueCID = 0;
    ce->NextSeqNumber = 0;	
    ce->SubsysId = 0;
    ce->Flags = 0;
    ce->SecurityLevel = 0;
    ce->EncryptionType = 0;
    ce->PeerHandle = 0;
    ce->PeerUnique = 0;
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
    bcopy(Retry_Beta, ce->Retry_Beta, sizeof(struct timeval)*(2+Retry_N));
    ce->SaveResponse = SaveResponse;  /* structure assignment */
    ce->MySl = NULL;
    ce->HeldPacket = NULL;
    ce->reqsize = 0;

    /* Then make it unique */
    Uniquefy(ce);
    return(ce);
    }

void rpc2_FreeConn(whichConn)
    RPC2_Handle whichConn;
    /* Frees the connection whichConn */
    {
    register long i;
    RPC2_PacketBuffer *pb;
    struct CEntry *ce;

    ce = rpc2_FindCEAddr(whichConn);
    assert(ce != NULL);
    assert(ce->MagicNumber == OBJ_CENTRY);
    rpc2_FreeConns++;

    free(ce->Retry_Beta);
    if (ce->HeldPacket != NULL)
	RPC2_FreeBuffer(&ce->HeldPacket);
    if (ce->MySl != NULL)
	{
	rpc2_DeactivateSle(ce->MySl);
    	rpc2_FreeSle(&ce->MySl);
	}

    /* Scan the hold queue and purge the request  for this connection */
    pb=rpc2_PBHoldList;
    for (i = 0; i < rpc2_PBHoldCount; i++)
    	{
	if (pb->Header.RemoteHandle != ce->UniqueCID)continue;
	else
	    {
	    say(9, RPC2_DebugLevel, ("Purging request from hold queue\n"));
	    rpc2_UnholdPacket(pb);
	    RPC2_FreeBuffer(&pb);
	    break;  /* there can be at most one in hold queue (RPC) */	    
	    }
	}


    SetRole(ce, FREE);
    rpc2_MoveEntry(&rpc2_ConnList, &rpc2_ConnFreeList, ce,
			&rpc2_ConnCount, &rpc2_ConnFreeCount);
    EntriesInUse--;
    HashTable[whichConn & (MAXHASHLENGTH-1)] = NULL;
    }


PRIVATE PrintHashTable()
    {
    register long i;
    printf("CurrentLength = %ld\tEntriesInUse = %ld\tMAXHASHLENGTH = %ld\n", CurrentLength, EntriesInUse, MAXHASHLENGTH);
    for (i=0; i < CurrentLength; i++)
	printf("HashTable[%ld] = 0x%lx\n", i, (long)HashTable[i]);
    }




void rpc2_SetConnError(IN ce)
    register struct CEntry *ce;
    {
    assert (ce->MagicNumber == OBJ_CENTRY);

    if (TestRole(ce, SERVER)) SetState(ce, S_HARDERROR);
    else SetState(ce, C_HARDERROR);
    
    /* RC should be LWP_SUCCESS or LWP_ENOWAIT */
    LWP_NoYieldSignal((char *)ce);
    }


/* Code to Map Retried Bind Requests to Existing Connections  */

/*
All packets other than Init1 requests have a LocalHandle field which is valid.  On Init1 
we do not have a local handle yet.  Each Init1 packet and its retries have a truly 
random Uniquefier, generated by the client.    The retries also have the  RETRY bit 
set in the packet headers.  The triple (Host,Portal,Uniquefier) is  totally unique even 
across client reboots.  
    
In the worst case the mapping involves a linear search of the connection list.  With 
1000 connections this took about 60 milliseconds on a SUN2.  In practise, many of 
these connections will not be server-end connections so the test will be shorter.  
However, to speed up the lookup we use a trivial LRU cache of recent bind completions.
The RBCache is essentially a way to focus attention on a small subset of the entire
connection list.

To conserve storage we allocate the RBCache only if the number of connections
exceeds a certain threshold.  Setting RPC2_Small suppresses the RBCache mechanism
altogether.

If this doesn't work well enough we may have to go to a hash table data structure 
that maps each  (host, portal, uniquefier) triple to a connection handle.  That 
will almost certainly be more complex to build and maintain.
*/


struct RecentBind
    {
    RPC2_HostIdent Host;	/* Remote Host */
    RPC2_PortalIdent Portal;	/* Remote Portal */
    RPC2_Integer Unique;	/* Uniquefier value in Init1 packet */
    RPC2_Handle MyConn;		/* Local handle allocated for this connection */
    };

#define RBSIZE 300	/* max size of RBCache for large RPC */
#define RBCACHE_THRESHOLD 50	/* RBCache never used for less than RBCACHE_THRESHOLD connections */
PRIVATE struct RecentBind *RBCache;	/* Wraps around; reused in LRU order.
					Conditionally allocated. */
PRIVATE int RBWrapped = 0;	/* RBCache is full and has wrapped around */
PRIVATE int NextRB = 0;		/* Index of entry to be used for the next bind */
PRIVATE int RBCacheOn = 0;	/* 0 = RBCacheOff, 1 = RBCacheOn */

rpc2_NoteBinding(whichHost, whichPortal, whichUnique, whichConn)
    register RPC2_HostIdent *whichHost;
    register RPC2_PortalIdent *whichPortal;
    RPC2_Integer whichUnique;
    register RPC2_Handle whichConn;
    /* Adds information about a new bind to the RBCache; throws out the oldest entry if needed */
    {
    if (rpc2_ConnCount <= RBCACHE_THRESHOLD) return;

    if (!RBCacheOn)
    	{/* first use of RBCache- must allocate cache */
	RBCache = (struct RecentBind *) malloc(RBSIZE * sizeof(struct RecentBind));
	RBCacheOn = 1;
    	}

    bzero(&RBCache[NextRB], sizeof(struct RecentBind));
    RBCache[NextRB].Host = *whichHost;		/* struct assignment */
    RBCache[NextRB].Portal = *whichPortal;	/* struct assignment */
    RBCache[NextRB].Unique = whichUnique;
    RBCache[NextRB].MyConn = whichConn;
    
    NextRB++;
    if (NextRB >= RBSIZE)
    	{
	RBWrapped = 1;
	NextRB = 0;
	}
    }

struct CEntry *rpc2_ConnFromBindInfo(whichHost, whichPortal, whichUnique)
    register RPC2_HostIdent *whichHost;
    register RPC2_PortalIdent *whichPortal;
    register RPC2_Integer whichUnique;
    /* Identifies the connection corr to (whichHost, whichPortal) with uniquefier 
	whichUnique, if it exists.
	Returns the address of the connection block, or NULL if no such binding  exists.
	The code first looks in RBCache[] and if that fails, it walks the 
	connection list.

    */
    {
    register struct RecentBind *rbn;
    int next, count;
    register struct CEntry *ce;
    register int i;
    
    /* If RBCache is being used, check it first;  search
     * it backwards, to increase chances of hit on recent binds.
     */

    if (RBCacheOn)
    	{
	next = (NextRB == 0) ? RBSIZE - 1 : NextRB - 1;
	if (RBWrapped) count = RBSIZE;
	else count = NextRB;
	i = 0;

	while (i < count)
	    {
	    rbn = &RBCache[next];
	    if (rbn->Unique == whichUnique		/* do cheapest test first */
		&& rpc2_HostIdentEqual(&rbn->Host, whichHost)
		&& rpc2_PortalIdentEqual(&rbn->Portal, whichPortal))
		    {
		    say(0, RPC2_DebugLevel, ("RBCache hit after %d tries\n", i+1));		
		    return(rpc2_FindCEAddr(rbn->MyConn));
		    }

	    /* Else bump counters and try previous one */
	    i++;
	    if (next == 0) next = RBSIZE - 1;
	    else next--;
	    }

	say(0, RPC2_DebugLevel, ("RBCache miss after %d tries\n", RBSIZE));
    	}
    
    /* It was not in the RBCache; scan the connection list */
    
    for (ce = rpc2_ConnList, i = 0; i < rpc2_ConnCount; i++, ce = ce->NextEntry)
	{
	if (ce->PeerUnique == whichUnique		/* do cheapest test first */
	    && rpc2_HostIdentEqual(&ce->PeerHost, whichHost)
	    && rpc2_PortalIdentEqual(&ce->PeerPortal, whichPortal))
		{
		say(0, RPC2_DebugLevel, ("Match after searching %d connection entries\n", i+1));
		return(ce);
		}
	}
    say(0, RPC2_DebugLevel, ("No match after searching %ld connections\n", rpc2_ConnCount));

    return(NULL);
    }


struct CEntry *rpc2_GetConn(handle)
    RPC2_Handle handle;
    {
    register struct CEntry *ce;
    ce = rpc2_FindCEAddr(handle);
    if (ce == NULL) return(NULL);
    assert(ce->MagicNumber == OBJ_CENTRY);
    return(ce);
    }
