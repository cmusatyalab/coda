/* BLURB lgpl

			Coda File System
			    Release 6

	    Copyright (c) 1987-2006 Carnegie Mellon University
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

    he->RTT = 0;
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

	LUA_drop_hosttable(*whichHost);

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

static void getestimates(struct HEntry *host, uint32_t InB, uint32_t OutB,
			 uint32_t *rtt_lat, uint32_t *rtt_in, uint32_t *rtt_out)
{
    uint32_t avgBW;

    *rtt_lat = host->RTT >> RPC2_RTT_SHIFT;

    avgBW = (host->BWlo_in >> 1) + (host->BWhi_in >> 1);
    while (InB > 2048) { InB >>= 1; avgBW >>= 1; }
    if (!avgBW) avgBW = 1;
    *rtt_in = (1000000 * InB) / avgBW;

    avgBW = (host->BWlo_out >> 1) + (host->BWhi_out >> 1);
    while (OutB > 2048) { OutB >>= 1; avgBW >>= 1; }
    if (!avgBW) avgBW = 1;
    *rtt_out = (1000000 * OutB) / avgBW;
}

static void update_bw(uint32_t *BWlo, uint32_t *BWhi, uint32_t rtt,
		      uint32_t bytes)
{
    uint32_t current, estimated;

    /* eBR = ( eU * 1000 ) / Bytes ; */
    current = (rtt * 125 / bytes) << 3;

    if (!*BWlo) *BWlo = 1;
    estimated = 1000000000 / *BWlo;
    if (current >= estimated)
	 estimated += (current - estimated) >> RPC2_BW_SHIFT;
    else estimated -= (estimated - current) >> RPC2_BW_SHIFT;
    if (!estimated) estimated = 1;
    *BWlo = 1000000000 / estimated;

    while (bytes > 4096) { bytes >>= 1; rtt >>= 1; }
    if (!rtt) rtt = 1;
    current = 1000000 * bytes / rtt;
    if (current >= *BWhi)
	 *BWhi += (current - *BWhi) >> RPC2_BW_SHIFT;
    else *BWhi -= (*BWhi - current) >> RPC2_BW_SHIFT;
}

/* Here we update the RTT and Bandwidth estimates,
 * ElapsedTime  is the observed roundtrip-time in microseconds
 * Bytes	is the number of bytes transferred */
void RPC2_UpdateEstimates(struct HEntry *host, RPC2_Unsigned elapsed_us,
			  RPC2_Unsigned InBytes, RPC2_Unsigned OutBytes)
{
    uint32_t rto, rtt_lat, rtt_out, rtt_in;/* estimated roundtrip time(s) */
    uint32_t rttvar = host->RTTvar >> RPC2_RTTVAR_SHIFT;
    int32_t adjustment;

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
    if ((int32_t)elapsed_us < 0) elapsed_us = 0;

    getestimates(host, InBytes, OutBytes, &rtt_lat, &rtt_in, &rtt_out);
    rto = rtt_lat + rtt_out + rtt_in;

    if (RPC2_DebugLevel)
    {
	char addr[RPC2_ADDRSTRLEN];
	RPC2_formataddrinfo(host->Addr, addr, RPC2_ADDRSTRLEN);
	fprintf(rpc2_logfile, "uRTT: %s %u %u %u %u %u %u %u %u %u\n",
		addr, elapsed_us, OutBytes, InBytes, rto, rtt_lat,
		host->BWlo_out, host->BWhi_out, host->BWlo_in, host->BWhi_in);
    }

    if (elapsed_us >= rto) {
	adjustment = (elapsed_us - rto) / 3;
	rtt_out += adjustment;
	rtt_in += adjustment;
    } else {
	adjustment = elapsed_us / 3;
	rtt_out = adjustment;
	rtt_in = adjustment;
	adjustment -= rtt_lat;
    }

    host->RTT += adjustment;
    update_bw(&host->BWlo_in, &host->BWhi_in, rtt_in, InBytes);
    update_bw(&host->BWlo_out, &host->BWhi_out, rtt_out, OutBytes);

    if (adjustment < 0) adjustment = -adjustment;
    if (adjustment >= rttvar)
	 host->RTTvar += adjustment - rttvar;
    else host->RTTvar -= rttvar - adjustment;

    LUA_rtt_update(host, elapsed_us, OutBytes, InBytes);
}

uint32_t rpc2_GetRTO(struct HEntry *he, uint32_t outbytes, uint32_t inbytes)
{
    uint32_t rto, rtt_lat, rtt_in, rtt_out, rttvar;

    rto = LUA_rtt_getrto(he, outbytes, inbytes);
    if ((int32_t)rto <= 0) {
	rttvar = he->RTTvar >> RPC2_RTTVAR_SHIFT;
	getestimates(he, inbytes, outbytes, &rtt_lat, &rtt_in, &rtt_out);
	rto = rtt_lat + rtt_out + rtt_in + (rttvar << 1);
	say(4, RPC2_DebugLevel,
	    "rpc2_GetRTO: rto %u, lat %u, out %u, in %u, var %u\n",
	    rto, rtt_lat, rtt_out, rtt_in, rttvar);
    }
    return rto;
}

int rpc2_RetryInterval(struct CEntry *ce, int retry, struct timeval *tv,
		       RPC2_Unsigned OutBytes, RPC2_Unsigned InBytes)
{
    uint32_t rto, maxrtt;
    int i = 0;

    if (!ce) {
	say(1, RPC2_DebugLevel, "RetryInterval: !conn\n");
	return -1;
    }

    /* Account for IP/UDP header overhead, see rpc2_UpdateEstimates */
    InBytes += 40; OutBytes += 40;

    /* calculate the estimated RTT */
    rto = LUA_rtt_retryinterval(ce->HostInfo, retry, OutBytes, InBytes);
    if (rto == 0) {
	rto = rpc2_GetRTO(ce->HostInfo, OutBytes, InBytes);

	if (retry) {
	    maxrtt = ce->KeepAlive.tv_sec * 1000000 + ce->KeepAlive.tv_usec;

	    for (i = Retry_N; i >= 0; i--) {
		maxrtt >>= 1;
		if (rto > maxrtt) break;
	    }
	    rto = maxrtt;
	}
	if (i + retry > Retry_N) return -1;
	rto <<= retry;
    }

    /* account for server processing overhead */
    rto += RPC2_DELACK_DELAY;

    /* clamp retry estimate */
    /* we shouldn't need a lower bound because we already account for the
     * server processing delay */
    if (rto > RPC2_MAXRTO) rto = RPC2_MAXRTO;

    tv->tv_sec  = rto / 1000000;
    tv->tv_usec = rto % 1000000;

    if (RPC2_DebugLevel > 1)
    {
	char addr[RPC2_ADDRSTRLEN];
	RPC2_formataddrinfo(ce->HostInfo->Addr, addr, RPC2_ADDRSTRLEN);
	fprintf(rpc2_logfile, "RetryInterval: %s %d %d %ld.%06lu\n",
		addr, retry, i, tv->tv_sec, tv->tv_usec);
    }
    return 0;
}

int RPC2_GetRTT(RPC2_Handle handle, unsigned long *RTT, unsigned long *RTTvar)
{
    struct CEntry *ce;

    ce = rpc2_GetConn(handle);
    if (ce == NULL)
	return(RPC2_NOCONNECTION);

    if (RTT)    *RTT    = ce->HostInfo->RTT    >> RPC2_RTT_SHIFT;
    if (RTTvar) *RTTvar = ce->HostInfo->RTTvar >> RPC2_RTTVAR_SHIFT;

    return(RPC2_SUCCESS);
}

int RPC2_GetBandwidth(RPC2_Handle handle, unsigned long *BWlow,
		      unsigned long *BWavg, unsigned long *BWhigh)
{
    struct CEntry *ce;
    uint32_t bw_lo, bw_avg, bw_hi;

    ce = rpc2_GetConn(handle);
    if (ce == NULL) return(RPC2_NOCONNECTION);

    if (LUA_rtt_getbandwidth(ce->HostInfo, &bw_avg, NULL))
	bw_lo = bw_hi = bw_avg;
    else
    {
	bw_lo = ce->HostInfo->BWlo_out;
	bw_avg = (ce->HostInfo->BWlo_out + ce->HostInfo->BWhi_out) >> 1;
	bw_hi = ce->HostInfo->BWhi_out;
    }

    if (BWlow)  *BWlow = bw_lo;
    if (BWavg)  *BWavg = bw_avg;
    if (BWhigh) *BWhigh = bw_hi;

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

