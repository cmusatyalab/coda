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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rpc2/host.c,v 4.2 98/04/14 21:06:59 braam Exp $";
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


/*
                    IBM COPYRIGHT NOTICE

This file contains  some code identical to or  derived from
the 1986 version of  the Andrew File System, which is owned
by  the  IBM  Corporation.  Carnegie Mellon  University has
obtained permission to distribute this code under the terms
of the Coda License.

*/

/* Code to track host liveness 

   We track liveness by host-portal pair, as we're really interested
   in the liveness of the server process.  Response times are recorded
   in the appropriate host-portal pair, and exported to applications.
   This is similar to the "Lastword" shared between an RPC connection
   and its side effect.  However, unlike the "Lastword" sharing, these
   times cannot be used to suppress retransmissions _between_
   connections, because of the possibility of lost request packets.

   Entries are kept in a hash table. Since most hosts have multiple
   connections to a give server or service, and services are published
   on certain well-known portal ids, we hash on a combination of the
   low byte of the host address and the low byte of the portal.  Both
   are in network order, and are assumed to be numbers, not names.  */
 
#define HOSTHASHBUCKETS 64
#define HASHHOST(h,p) (((p & 0x0f00) >> 4 | (h & 0x0f000000) >> 24) \
		       & (HOSTHASHBUCKETS-1))
                                        /* mod HOSTHASHBUCKETS */
PRIVATE struct HEntry **HostHashTable;	/* to malloc'ed hash table of size CurrentLength entries */

void rpc2_InitHost()
{
    HostHashTable = (struct HEntry **)malloc(HOSTHASHBUCKETS*sizeof(struct HEntry *));
    assert(HostHashTable != 0);
    /* all entries initially NULL */
    bzero(HostHashTable, HOSTHASHBUCKETS*sizeof(struct HEntry *));	
}

struct HEntry *rpc2_FindHEAddr(IN whichHost, IN whichPortal)
    register unsigned long whichHost;
    register unsigned short whichPortal;
    /* Returns pointer to the host entry corresponding to (hostid, portalid)
	    Returns NULL if (host,portal) does not exist.
    */
    {
    register long bucket;
    register struct HEntry *headdr;

    if (whichHost == 0 || whichPortal == 0) return(NULL);

    bucket = HASHHOST(whichHost,whichPortal);
    headdr = HostHashTable[bucket];
    while (headdr) 
        {
	if ((headdr->Host == whichHost) && (headdr->Portal == whichPortal))
	    return(headdr);
	headdr = headdr->HLink;
        }

    return(NULL);
    }


/* 
 * search the table looking for an HEntry with the given host
 * and type.  This is approximate in that there may be more than
 * one; this routine returns the first one it finds.
 * This would be a nice spot for overloading.
 */
struct HEntry *rpc2_FindHEAddrByType(IN whichHost, IN type)
    register unsigned long whichHost;
    HEType type;
    /* Returns pointer to a host entry corresponding to (hostid, portalid)
       with a matching type. Returns NULL if (host,portal) does not exist.
    */
    {
    register long bucket;
    register struct HEntry *headdr;

    if (whichHost == 0 || type == UNSET_HE) return(NULL);

    for (bucket = 0; bucket < HOSTHASHBUCKETS; bucket++) {
	headdr = HostHashTable[bucket];
	while (headdr) {
	    if ((headdr->Host == whichHost) && (headdr->Type == type))
		return(headdr);
	    headdr = headdr->HLink;
	}
    }

    return(NULL);
    }


struct HEntry *rpc2_GetHost(host, portal) 
    RPC2_HostIdent *host;
    RPC2_PortalIdent *portal;
    {
    register struct HEntry *he;
    he = rpc2_FindHEAddr(host->Value.InetAddress, portal->Value.InetPortNumber);
    if (he == NULL) return(NULL);
    assert(he->MagicNumber == OBJ_HENTRY);
    return(he);
    }


struct HEntry *rpc2_GetHostByType(host, type) 
    RPC2_HostIdent *host;
    HEType type;
    {
    register struct HEntry *he;
    he = rpc2_FindHEAddrByType(host->Value.InetAddress, type);
    if (he == NULL) return(NULL);
    assert(he->MagicNumber == OBJ_HENTRY);
    return(he);
    }


struct HEntry *rpc2_AllocHost(host, portal, type)
    RPC2_HostIdent *host;
    RPC2_PortalIdent *portal;
    HEType type;
    {
    register long bucket;
    struct HEntry *he;

    if (rpc2_HostFreeCount == 0)
	    rpc2_Replenish(&rpc2_HostFreeList, &rpc2_HostFreeCount, sizeof(struct HEntry),
	    &rpc2_HostCreationCount, OBJ_HENTRY);

    he = (struct HEntry *)rpc2_MoveEntry(&rpc2_HostFreeList, &rpc2_HostList,
		 (struct HEntry *)NULL, &rpc2_HostFreeCount, &rpc2_HostCount);
    assert (he->MagicNumber == OBJ_HENTRY);

    /* Initialize */
    he->Host = host->Value.InetAddress;
    he->Portal = portal->Value.InetPortNumber;
    he->Type = type;
    he->LastWord.tv_sec = he->LastWord.tv_usec = 0;
    bzero(he->Log, RPC2_MAXLOGLENGTH*sizeof(RPC2_NetLogEntry));
    he->NumEntries = 0;

    /* insert into hash table */
    bucket = HASHHOST(he->Host, he->Portal);
    he->HLink = HostHashTable[bucket];
    HostHashTable[bucket] = he;

    return(he);
    }

void rpc2_FreeHost(whichHost)
    register struct HEntry **whichHost;
    /* releases the host entry pointed to by whichHost.
       Sets whichHost to NULL. */
    {
    register long bucket;
    struct HEntry **link;

    assert((*whichHost)->MagicNumber == OBJ_HENTRY);
    rpc2_MoveEntry((struct LinkEntry **)&rpc2_HostList,
		(struct LinkEntry **)&rpc2_HostFreeList,
		(struct LinkEntry *)*whichHost,
		&rpc2_HostCount, &rpc2_HostFreeCount);

    /* remove from hash table */
    bucket = HASHHOST((*whichHost)->Host,(*whichHost)->Portal);
    link = &HostHashTable[bucket];
    while (*link) {
	if (*link == *whichHost) {
	    *link = (*whichHost)->HLink;
	    *whichHost = NULL;
	}
	link = &(*link)->HLink;
    }
    }


void rpc2_GetHostLog(whichHost, log)
    register struct HEntry *whichHost;
    RPC2_NetLog *log;
    {
    unsigned long quantum = 0;
    unsigned wontexceed = RPC2_MAXLOGLENGTH;  /* as many as we can return */
    int head, tail, ix;

    assert(whichHost->MagicNumber == OBJ_HENTRY);

    /* figure out how many entries to send back */
    if (wontexceed > log->NumEntries) 
	wontexceed = log->NumEntries;  		/* asked for less */
    if (wontexceed > whichHost->NumEntries)
	wontexceed = whichHost->NumEntries;	/* we have less */
    if (wontexceed == 0) return; 	/* don't touch anything */

    head = whichHost->NumEntries - wontexceed;
    tail = whichHost->NumEntries - 1;	

    /* walk back through log and copy them out */
    while (tail >= head) {
	ix = tail & (RPC2_MAXLOGLENGTH-1);
	log->Entries[log->ValidEntries++] = whichHost->Log[ix];
	switch(whichHost->Log[ix].Tag) 
	    {
	    case RPC2_MEASURED_NLE:
		quantum += whichHost->Log[ix].Value.Measured.ElapsedTime;
		break;
	    case RPC2_STATIC_NLE:	/* static estimates are free */
	    default:
		break;
	    }

	if (quantum >= log->Quantum)
	    break;

	tail--;
    }
    }


#define GOOD_NLE(e) ((e->Tag == RPC2_STATIC_NLE || \
		      e->Tag == RPC2_MEASURED_NLE))

/* 
 * dumb routine to add a record to the host log.  Returns 1 if it
 * actually appended the entry, 0 if it didn't.
 * 
 * In the interests of avoiding variable length argument lists, 
 * assumes the variable length part of the record  has been 
 * constructed elsewhere. 
 */
rpc2_AppendHostLog(whichHost, entry)
    register struct HEntry *whichHost;
    RPC2_NetLogEntry *entry;
    {
    unsigned long ix = whichHost->NumEntries & (RPC2_MAXLOGLENGTH-1);

    assert(whichHost->MagicNumber == OBJ_HENTRY);

    if (!GOOD_NLE(entry)) return(0);

    whichHost->Log[ix] = *entry;	/* structure assignment */

    /* stamp it. use the approximate version */
    FT_GetTimeOfDay(&(whichHost->Log[ix].TimeStamp),
		     (struct timezone *)0);
    whichHost->NumEntries++;
    return(1);
    }


/* clear the log */
void rpc2_ClearHostLog(whichHost)
    register struct HEntry *whichHost;
    {
    assert(whichHost->MagicNumber == OBJ_HENTRY);
    whichHost->NumEntries = 0;
    bzero(whichHost->Log, RPC2_MAXLOGLENGTH*sizeof(RPC2_NetLogEntry));
    }
