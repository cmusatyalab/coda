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
	{
	    struct sockaddr_in *sin = (struct sockaddr_in *)ai->ai_addr;
	    lsb = sin->sin_addr.s_addr ^ sin->sin_port;
	    break;
	}

#if defined(PF_INET6)
    case PF_INET6:
	{
	    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ai->ai_addr;
	    lsb = ((u_int32_t *)&sin6->sin6_addr)[3] ^ sin6->sin6_port;
	    break;
	}
#endif
    default:
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

    he->RTT     = 0;
    he->RTTVar  = 0;
    he->BWlo_out = he->BWhi_out = RPC2_INITIAL_BW;
    he->BWlo_in = he->BWhi_in = RPC2_INITIAL_BW;

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

#if 0 // Since we don't actually have a reaper thread for host entries...
	/* probably dont really want to destroy the host information as soon as
	 * we lose all connections. I guess we need a reaper that kills of very
	 * old entries once in a while */
	/* RefCount == 0 means we're waiting for a reaper to finish us off */
	if ((*whichHost)->RefCount == 0) {
	    *whichHost = NULL;
	    return;
	}
#endif

	/* free resources */
	bucket = HASHHOST((*whichHost)->Addr);
	RPC2_freeaddrinfo((*whichHost)->Addr);
	(*whichHost)->Addr = NULL;

	rpc2_MoveEntry(&rpc2_HostList, &rpc2_HostFreeList, *whichHost,
		       &rpc2_HostCount, &rpc2_HostFreeCount);
	
	/* remove from hash table */
	link = &HostHashTable[bucket];
	while (*link) {
		if (*link == *whichHost) {
			*link = (*whichHost)->HLink;
			break;
		}
		link = &(*link)->HLink;
	}
	*whichHost = NULL;
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

/* Here we update the RTT and Bandwidth estimates,
 * ElapsedTime  is the observed roundtrip-time in microseconds
 * Bytes	is the number of bytes transferred */
void RPC2_UpdateEstimates(struct HEntry *host, RPC2_Unsigned elapsed_us,
			  RPC2_Unsigned InBytes, RPC2_Unsigned OutBytes)
{
    unsigned long avgBWout, avgBWin;
    unsigned long bytes, BR, BW;
    unsigned long rto, rtt, rtt_out, rtt_in;/* estimated roundtrip time(s) */
    long rte;

    if (!host) return;

    /* account for IP/UDP header overhead
     * an IPv4 header is (typically) 20 bytes but can be up to 60 bytes, IPv6
     * headers are 40 bytes but could include additional headers such as an 8
     * byte fragment header. In addition the UDP header adds another 8 bytes.
     * And then there is an additional 18 bytes ethernet header, but that may
     * not exist on PPP links. And of course if we have some compression layer
     * below us none of these numbers make any sense.
     * If we pick 40 we will be slightly reasonably close for a IPv4 ethernet
     * network but underestimate the packet size over a v6 network. At least it
     * brings us a bit closer to reality. If we don't account for this
     * overhead, the delay of a 60 byte RPC2 ping packet is considerably
     * underestimated, which leads to an incorrect bandwidth estimate. */
    InBytes += 40; OutBytes += 40;

    rtt = host->RTT >> RPC2_RTT_SHIFT;

    bytes = OutBytes;
    avgBWout = BW = (host->BWlo_out + host->BWhi_out) >> 1;
    while (bytes > 2048) { bytes >>= 1; BW >>= 1; }
    if (!BW) BW = 1;
    rtt_out = (1000000 * bytes) / BW;

    bytes = InBytes;
    avgBWin = BW = (host->BWlo_in + host->BWhi_in) >> 1;
    while (bytes > 2048) { bytes >>= 1; BW >>= 1; }
    if (!BW) BW = 1;
    rtt_in = (1000000 * bytes) / BW;

    if ((int32_t)elapsed_us < 0) elapsed_us = 0;

    rto = rtt + rtt_out + rtt_in;

    if (RPC2_DebugLevel)
    {
	char addr[RPC2_ADDRSTRLEN];
	RPC2_formataddrinfo(host->Addr, addr, RPC2_ADDRSTRLEN);
	fprintf(rpc2_logfile,
		"uRTT: %s %u %u %u %lu %lu %lu %lu %lu %lu %lu %lu\n",
		addr, elapsed_us, OutBytes, InBytes, rto, rtt,
		avgBWout, host->BWlo_out, host->BWhi_out,
		avgBWin, host->BWlo_in, host->BWhi_in);
    }

    if (elapsed_us >= rto) {
	rte = (elapsed_us - rto) >> 1;
	rtt_out += rte;
	rtt_in += rte;
	rte >>= 1;
	rtt += rte;
    } else {
	rte = elapsed_us >> 1;
	rtt_out = rte;
	rtt_in = rte;
	rte >>= 1;
	rtt = rte;
    }

    /* the RTT & RTT variance are shifted analogous to Jacobson's
     * article in SIGCOMM'88 */
    rtt -= (long)(host->RTT >> RPC2_RTT_SHIFT);
    host->RTT += rtt;
    if (rtt < 0) rtt = -rtt;
    rtt -= (long)(host->RTTVar >> RPC2_RTTVAR_SHIFT);
    host->RTTVar += rtt;

    /* eBR = ( eU * 1000 ) / Bytes ; */
    BR = (rtt_out * 125 / OutBytes) << 3;
    if (!host->BWlo_out) host->BWlo_out = 1;
    BW = 1000000000 / host->BWlo_out;
    if (BR >= BW) BW += (BR - BW) >> RPC2_BW_SHIFT;
    else	  BW -= (BW - BR) >> RPC2_BW_SHIFT;
    if (!BW) BW = 1;
    host->BWlo_out = 1000000000 / BW;

    while (OutBytes > 4096) { OutBytes >>= 1; rtt_out >>= 1; }
    if (!rtt_out) rtt_out = 1;
    BW = 1000000 * OutBytes / rtt_out;
    if (BW >= host->BWhi_out)
	 host->BWhi_out += (BW - host->BWhi_out) >> RPC2_BW_SHIFT;
    else host->BWhi_out -= (host->BWhi_out - BW) >> RPC2_BW_SHIFT;

    BR = (rtt_in * 125 / InBytes) << 3;
    if (!host->BWlo_in) host->BWlo_in = 1;
    BW = 1000000000 / host->BWlo_in;
    if (BR >= BW) BW += (BR - BW) >> RPC2_BW_SHIFT;
    else	  BW -= (BW - BR) >> RPC2_BW_SHIFT;
    if (!BW) BW = 1;
    host->BWlo_in = 1000000000 / BW;

    while (InBytes > 4096) { InBytes >>= 1; rtt_in >>= 1; }
    if (!rtt_in) rtt_in = 1;
    BW = 1000000 * InBytes / rtt_in;
    if (BW >= host->BWhi_in)
	 host->BWhi_in += (BW - host->BWhi_in) >> RPC2_BW_SHIFT;
    else host->BWhi_in -= (host->BWhi_in - BW) >> RPC2_BW_SHIFT;
}

void rpc2_RetryInterval(struct HEntry *host, struct SL_Entry *sl,
			RPC2_Unsigned OutBytes, RPC2_Unsigned InBytes,
			int maxretry, struct timeval *keepalive)
{
    unsigned long rto, rtt, avgBW;
    int i;

    if (!host || !sl) {
	say(1, RPC2_DebugLevel, "RetryInterval: !host || !sl\n");
	return;
    }

    /* Account for IP/UDP header overhead, see rpc2_UpdateEstimates */
    InBytes += 40; OutBytes += 40;

    /* calculate the estimated RTT */
    rto = (host->RTT >> RPC2_RTT_SHIFT); // + host->RTTVar;

    /* rto += 1000000 * bytes / BW; */
    avgBW = (host->BWlo_out + host->BWhi_out) >> 1;
    while (OutBytes > 4096) { OutBytes >>= 1; avgBW >>= 1; }
    if (!avgBW) avgBW = 1;
    rto += 1000000 * OutBytes / avgBW;

    /* rto += 1000000 * bytes / BW; */
    avgBW = (host->BWlo_in + host->BWhi_in) >> 1;
    while (InBytes > 4096) { InBytes >>= 1; avgBW >>= 1; }
    if (!avgBW) avgBW = 1;
    rto += 1000000 * InBytes / avgBW;

    /* account for server processing overhead */
    rto += RPC2_DELACK_DELAY;

    if (sl->RetryIndex != 1) {
	rtt = (keepalive->tv_sec * 1000000 + keepalive->tv_usec) >> 1;

	for (i = maxretry; i > sl->RetryIndex; i--) {
	    rtt >>= 1;
	    if (rtt < rto) break;
	}
	sl->RetryIndex = i;
	if (rtt > rto) rto = rtt;
    }

    /* clamp retry estimate */
    /* we shouldn't need a lower bound because we already account for the
     * server processing delay */
    if (rto > RPC2_MAXRTO) rto = RPC2_MAXRTO;

    sl->RInterval.tv_sec  = rto / 1000000;
    sl->RInterval.tv_usec = rto % 1000000;

    if (RPC2_DebugLevel > 1)
    {
	char addr[RPC2_ADDRSTRLEN];
	RPC2_formataddrinfo(host->Addr, addr, RPC2_ADDRSTRLEN);
	fprintf(rpc2_logfile, "RetryInterval: %s %d %lu.%06lu\n",
	    addr, sl->RetryIndex, sl->RInterval.tv_sec, sl->RInterval.tv_usec);
    }
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

    ce = rpc2_GetConn(handle);
    if (ce == NULL) return(RPC2_NOCONNECTION);

    if (BWlow)  *BWlow = ce->HostInfo->BWlo_out;
    if (BWavg)  *BWavg = (ce->HostInfo->BWlo_out + ce->HostInfo->BWhi_out) >> 1;
    if (BWhigh) *BWhigh = ce->HostInfo->BWhi_out;

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

