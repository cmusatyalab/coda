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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "rpc2.private.h"

/* Code to track host liveness 

   We track liveness by host ip-address, as we're really interested
   in the bandwidth and latency of connection between us and the server
   machine.
   Response times and bandwidth/latency estimates are calculated and
   exported to applications, and other parts of the rpc2/sftp code.
   This is similar to the "Lastword" shared between an RPC connection
   and its side effect.  However, unlike the "Lastword" sharing, these
   times cannot be used to suppress retransmissions _between_
   connections, because of the possibility of lost request packets.
   Hosts are kept in a hash table.
   We hash on the low bytes of the host address (in network order) */
 
#define HOSTHASHBUCKETS 64
/* try to grab the low-order bits, assuming all are stored big endian */
int HASHHOST(struct RPC2_addrinfo *ai)
{
    int lsb = 0;
    switch(ai->ai_family) {
    case PF_INET:
	lsb = ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr;
	break;

    case PF_INET6:
	lsb = ((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr.in6_u.u6_addr32[3];
	break;
    }
    return lsb & (HOSTHASHBUCKETS-1);
}

static struct HEntry **HostHashTable;	/* malloc'ed hash table static size */

void rpc2_InitHost()
{
	HostHashTable = (struct HEntry **)malloc(HOSTHASHBUCKETS*sizeof(struct HEntry *));
	assert(HostHashTable != 0);
	memset(HostHashTable, 0, HOSTHASHBUCKETS*sizeof(struct HEntry *));	
}

/* Returns pointer to the host entry corresponding to addr. Addr should point
 * at only a single addrinfo structure. */
struct HEntry *rpc2_GetHost(struct RPC2_addrinfo *addr)
{
    struct HEntry *he;
    long bucket;

    if (!addr)
	return NULL;

    assert(addr->ai_next == NULL);
    bucket = HASHHOST(addr);
    he = HostHashTable[bucket];

    for (; he; he = he->HLink) {
	if (RPC2_cmpaddrinfo(he->Addr, addr)) {
	    assert(he->MagicNumber == OBJ_HENTRY);
	    he->RefCount++;
	    return(he);
	}
    }

    /* failed to find a pre-existing entry, allocate a new one */
    if (rpc2_HostFreeCount == 0)
	rpc2_Replenish(&rpc2_HostFreeList, &rpc2_HostFreeCount,
		       sizeof(struct HEntry), &rpc2_HostCreationCount,
		       OBJ_HENTRY);

    he = (struct HEntry *)rpc2_MoveEntry(&rpc2_HostFreeList,
					 &rpc2_HostList,
					 (struct HEntry *)NULL,
					 &rpc2_HostFreeCount,
					 &rpc2_HostCount);
    assert (he->MagicNumber == OBJ_HENTRY);

    /* Initialize */
    he->Addr = RPC2_copyaddrinfo(addr);
    he->LastWord.tv_sec = he->LastWord.tv_usec = 0;

    /* clear the network measurement logs */
    rpc2_ClearHostLog(he, RPC2_MEASUREMENT);
    rpc2_ClearHostLog(he, SE_MEASUREMENT);

    he->RTT       = 0;
    he->RTTVar    = 0;
    he->BR        = 1000 << RPC2_BR_SHIFT;
    he->BRVar     = 0;

    /* insert into hash table */
    he->HLink = HostHashTable[bucket];
    HostHashTable[bucket] = he;

    he->RefCount++;
    return(he);
}

/* releases the host entry pointed to by whichHost.
   Sets whichHost to NULL. */
void rpc2_FreeHost(struct HEntry **whichHost)
{
	long bucket;
	struct HEntry **link;
	
	assert((*whichHost)->MagicNumber == OBJ_HENTRY);

	if (--(*whichHost)->RefCount > 0) {
	    *whichHost = NULL;
	    return;
	}
	/* probably dont really want to destroy the host information as soon as
	 * we lose all connections. I guess we need a reaper that kills of very
	 * old entries once in a while */

	/* RefCount == 0 means we're waiting for a reaper to finish us off */
	if ((*whichHost)->RefCount == 0) {
	    *whichHost = NULL;
	    return;
	}

	/* free resources */
	bucket = HASHHOST((*whichHost)->Addr);
	RPC2_freeaddrinfo((*whichHost)->Addr);
	(*whichHost)->Addr = NULL;

	rpc2_MoveEntry((struct LinkEntry **)&rpc2_HostList,
		       (struct LinkEntry **)&rpc2_HostFreeList,
		       (struct LinkEntry *)*whichHost,
		       &rpc2_HostCount, &rpc2_HostFreeCount);
	
	/* remove from hash table */
	link = &HostHashTable[bucket];
	while (*link) {
		if (*link == *whichHost) {
			*link = (*whichHost)->HLink;
			*whichHost = NULL;
			break;
		}
		link = &(*link)->HLink;
	}
}


void rpc2_GetHostLog(struct HEntry *whichHost, RPC2_NetLog *log,
		     NetLogEntryType type)
{
	unsigned long quantum = 0;
	unsigned int  tail, ix, left = log->NumEntries;
	RPC2_NetLogEntry  *Log;
	unsigned int  NumEntries;
	
	assert(whichHost->MagicNumber == OBJ_HENTRY);

	if (type == RPC2_MEASUREMENT) {
	    Log = whichHost->RPC2_Log;
	    NumEntries = whichHost->RPC2_NumEntries;
	} else {
	    Log = whichHost->SE_Log;
	    NumEntries = whichHost->SE_NumEntries;
	}

	/* figure out how many entries to send back */
	if (left > RPC2_MAXLOGLENGTH) left = RPC2_MAXLOGLENGTH;
	if (left > NumEntries)        left = NumEntries; /* we have less */

	log->ValidEntries = 0;
	if (left == 0) return; 	/* don't touch anything */
	
	tail = NumEntries - 1;	
	
	/* walk back through log and copy them out */
	while (left--) {
		ix = tail & (RPC2_MAXLOGLENGTH-1);

		log->Entries[log->ValidEntries++] = Log[ix];

		switch(Log[ix].Tag)  {
		case RPC2_MEASURED_NLE:
			quantum += Log[ix].Value.Measured.ElapsedTime;
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


#define GOOD_NLE(e) ((e->Tag == RPC2_STATIC_NLE || e->Tag == RPC2_MEASURED_NLE))

/* 
 * dumb routine to add a record to the host log.  Returns 1 if it
 * actually appended the entry, 0 if it didn't.
 * 
 * In the interests of avoiding variable length argument lists, 
 * assumes the variable length part of the record  has been 
 * constructed elsewhere. 
 */
int rpc2_AppendHostLog(struct HEntry *whichHost, RPC2_NetLogEntry *entry,
		       NetLogEntryType type)
{
	unsigned long ix;
	RPC2_NetLogEntry *Log;
	unsigned int *NumEntries;

	assert(whichHost->MagicNumber == OBJ_HENTRY);
	
	if (!GOOD_NLE(entry)) 
		return(0);

	if (type == RPC2_MEASUREMENT) {
	    Log = whichHost->RPC2_Log;
	    NumEntries = &whichHost->RPC2_NumEntries;
	} else {
	    Log = whichHost->SE_Log;
	    NumEntries = &whichHost->SE_NumEntries;
	}

	ix = *NumEntries & (RPC2_MAXLOGLENGTH-1);
	Log[ix] = *entry; /* structure assignment */

	FT_GetTimeOfDay(&(Log[ix].TimeStamp), (struct timezone *)0);

	(*NumEntries)++;

	return(1);
}


/* clear the log */
void rpc2_ClearHostLog(struct HEntry *whichHost, NetLogEntryType type)
{
	assert(whichHost->MagicNumber == OBJ_HENTRY);

	if (type == RPC2_MEASUREMENT) {
	    whichHost->RPC2_NumEntries = 0;
	    memset(whichHost->RPC2_Log, 0,
		   RPC2_MAXLOGLENGTH * sizeof(RPC2_NetLogEntry));
	} else {
	    whichHost->SE_NumEntries = 0;
	    memset(whichHost->SE_Log, 0,
		   RPC2_MAXLOGLENGTH * sizeof(RPC2_NetLogEntry));
	}
}

/* Here we update the RTT and Bandwidth (or more precise byterate in ns per
 * byte) estimates,
 * ElapsedTime  is the observed roundtrip-time in microseconds
 * Bytes        is the number of bytes transferred */
void RPC2_UpdateEstimates(struct HEntry *host, RPC2_Unsigned elapsed_us,
			  RPC2_Unsigned InBytes, RPC2_Unsigned OutBytes)
{
    long	   eRTT;     /* estimated null roundtrip time */
    long	   eBR;      /* estimated byterate (ns/B) */
    unsigned long  eU;       /* temporary unsigned variable */
    long	   eL;       /* temporary signed variable */
    char addr[RPC2_ADDRSTRLEN];

    if (!host) return;

    say(0, RPC2_DebugLevel, "uRTT: 0x%lx %lu %lu %lu\n", elapsed_us, elapsed_us, InBytes, OutBytes);
    
    if ((long)elapsed_us < 0) elapsed_us = 0;

    /* we need to clamp elapsed elapsed_us to about 16 seconds to avoid
     * overflows with the 31 bit calculations below */
    if (elapsed_us > 0x00ffffff) elapsed_us = 0x00ffffff;

    /* get the estimated rtt */
    eRTT = host->RTT >> RPC2_RTT_SHIFT;

    if (elapsed_us > eRTT) eU = elapsed_us - eRTT;
    else		   eU = 0;

    /* eBR = ( eU * 1000 ) / Bytes ; */
    eBR = ((eU << 7) / (InBytes + OutBytes)) << 3;
    eBR -= (host->BR >> RPC2_BR_SHIFT);

    /* HACK! to avoid small packets with RTT's that are not within the current
     * window of variance to have a strong effect on the byterate estimate, we
     * halve the difference of the new measurement, if it falls outside of the
     * variance window */
    if      (eBR >  (host->BRVar >> RPC2_BRVAR_SHIFT)) eBR >>= 1;
    else if (eBR < -(host->BRVar >> RPC2_BRVAR_SHIFT)) eBR >>= 1;
    /* HACK! */

    host->BR += eBR;

    if (eBR < 0) eBR = -eBR;

    /* Invariant: eBR contains the absolute difference between the previous
     * calculated byterate and the new measurement */

    eBR -= (host->BRVar >> RPC2_BRVAR_SHIFT);
    host->BRVar += eBR;

    /* get a new RTT estimate in elapsed_us */
    /* from here on eBR contains a lower estimate on the effective
     * byterate, eRTT will contain a updated RTT estimate */
    eBR = (host->BR >> RPC2_BR_SHIFT) + ((host->BR >> RPC2_BRVAR_SHIFT) >> 1);

    /* eU = ( eBR * (InBytes + OutBytes ) / 1000 ; */
    eU = ((eBR >> 4) * (InBytes + OutBytes)) >> 6;

    if (elapsed_us > eU) eL = elapsed_us - eU;
    else		 eL = 0;

    /* the RTT & RTT variance are shifted analogous to Jacobson's
     * article in SIGCOMM'88 */
    eL -= (host->RTT >> RPC2_RTT_SHIFT);
    host->RTT += eL;

    if (eL < 0) eL = -eL;

    eL -= (host->RTTVar >> RPC2_RTTVAR_SHIFT);
    host->RTTVar += eL;

    RPC2_formataddrinfo(host->Addr, addr, RPC2_ADDRSTRLEN);
    say(0, RPC2_DebugLevel,
	"Est: %s %4ld.%06lu/%-5lu<%-5lu RTT:%lu/%lu us BR:%lu/%lu ns/B\n",
	    addr, elapsed_us / 1000000, elapsed_us % 1000000,
	    InBytes, OutBytes, host->RTT>>RPC2_RTT_SHIFT, host->RTTVar>>RPC2_RTTVAR_SHIFT,
	    (host->BR>>RPC2_BR_SHIFT), host->BRVar>>RPC2_BRVAR_SHIFT);
    return;
}

void rpc2_RetryInterval(RPC2_Handle whichConn, RPC2_Unsigned InBytes,
			RPC2_Unsigned OutBytes, int *retry,
			int maxretry, struct timeval *tv)
{
    struct CEntry *ce;
    unsigned long rto, rtt;
    long          effBR;
    int		  i;

    ce = rpc2_GetConn(whichConn);

    if (!ce || !tv) {
	say(0, RPC2_DebugLevel, "RetryInterval: !ce || !tv\n");
	return;
    }

    /* calculate the estimated RTT */

    rto = (ce->HostInfo->RTT >> RPC2_RTT_SHIFT) + (ce->HostInfo->RTTVar >> 1);

    /* because we have subtracted the time to took to transfer data from our
     * RTT estimate (it is latency estimate), we have to add in the time to
     * send our packet into the estimated RTO */

    effBR = (ce->HostInfo->BR >> RPC2_BR_SHIFT);

    /* rto += ( effBR * (InBytes + OutBytes) ) / 1000 ; */
    rto += ((effBR >> 3) * (InBytes + OutBytes)) >> 7;
    
    if (*retry != 1) {
	rtt = ce->MaxRetryInterval.tv_sec * 1000000 +
	      ce->MaxRetryInterval.tv_usec;

	for (i = maxretry; i > *retry; i--) {
	    rtt >>= 1;
	    if (rtt < rto) break;
	}
	*retry = i;
	if (rtt > rto) rto = rtt;
    }
    
    /* clamp retry estimates */
    if      (rto < RPC2_MINRTO) rto = RPC2_MINRTO;
    else if (rto > RPC2_MAXRTO) rto = RPC2_MAXRTO;

    tv->tv_sec  = rto / 1000000;
    tv->tv_usec = rto % 1000000;

    say(0, RPC2_DebugLevel, "RetryInterval: %lu.%06lu\n",
	tv->tv_sec, tv->tv_usec);

    return;
}

int RPC2_GetRTT(RPC2_Handle handle, unsigned long *RTT, unsigned long *RTTvar)
{
    struct CEntry *ce;

    ce = rpc2_GetConn(handle);
    if (ce == NULL) 
	return(RPC2_NOCONNECTION);

    if (RTT)    *RTT    = ce->HostInfo->RTT    >> RPC2_RTT_SHIFT;
    if (RTTvar) *RTTvar = ce->HostInfo->RTTVar >> RPC2_RTTVAR_SHIFT;

    return(RPC2_SUCCESS);
}

int RPC2_GetBandwidth(RPC2_Handle handle, unsigned long *BWlow,
		      unsigned long *BWavg, unsigned long *BWhigh)
{
    struct CEntry *ce;
    unsigned long BR, BRVar, tBR;

    ce = rpc2_GetConn(handle);
    if (ce == NULL) return(RPC2_NOCONNECTION);

    BR    = ce->HostInfo->BR    >> RPC2_BR_SHIFT;
    BRVar = ce->HostInfo->BRVar >> RPC2_BRVAR_SHIFT;

    if (BWlow) {
	tBR = BR + BRVar;
	/* Need at least 1 to avoid divide by zero errors --JH */
	if (!tBR) tBR = 1;

	*BWlow = 1000000000 / tBR;
    }
    if (BWavg) {
	tBR = BR;
	/* Need at least 1 to avoid divide by zero errors --JH */
	if (!tBR) tBR = 1;

	*BWavg = 1000000000 / tBR;
    }
    if (BWhigh) {
	if (BR > BRVar)
	    tBR = BR - BRVar;
	else
	    tBR = 1;

	*BWhigh = 1000000000 / tBR;
    }

    return(RPC2_SUCCESS);
}

int RPC2_GetLastObs(RPC2_Handle handle, struct timeval *tv)
{
    struct CEntry *ce;

    ce = rpc2_GetConn(handle);
    if (ce == NULL) 
	return(RPC2_NOCONNECTION);

    if (tv) *tv = ce->HostInfo->LastWord;

    return(RPC2_SUCCESS);
}

