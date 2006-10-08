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
#include <sys/file.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
#include "rpc2.private.h"
#include <rpc2/se.h>
#include <rpc2/secure.h>
#include "cbuf.h"
#include "trace.h"

#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif

/* The MSG_CONFIRM flag significantly reduces arp traffic with linux-2.4 and
 * higher. However... a precompiled binary that uses this flag fails to send
 * any packets when run on linux-2.2 kernels. So we initially set msg_confirm
 * to match the flag, but if we get an EINVAL error back from sendto we clear
 * it. This way we lose the first packet after startup, but retransmission
 * should fix that automatically. --JH
 */
static int msg_confirm = MSG_CONFIRM;

static long DefaultRetryCount = 6;
static struct timeval DefaultRetryInterval = {60, 0};

/* Hooks for failure emulation package (libfail)

   Libfail will set these to its predicate routines when initialized.
   If libfail is not linked in, they remain NULL, and nothing happens.
   See documentation for libfail for details.
 */

int (*Fail_SendPredicate)() = NULL,
    (*Fail_RecvPredicate)() = NULL;

static long FailPacket(int (*predicate)(), RPC2_PacketBuffer *pb,
			   struct RPC2_addrinfo *addr, int sock)
{
    long drop;
    unsigned char ip1, ip2, ip3, ip4;
    unsigned char color;
    struct sockaddr_in *sin;
    unsigned char *inaddr;

    if (!predicate)
	return 0;

#warning "fail filters can only handle ipv4 addresses"
    if (addr->ai_family != PF_INET)
	return 0;

    sin = (struct sockaddr_in *)addr->ai_addr;
    inaddr = (unsigned char *)&sin->sin_addr;

    ip1 = inaddr[0]; ip2 = inaddr[1]; ip3 = inaddr[2]; ip4 = inaddr[3];

    ntohPktColor(pb);
    color = GetPktColor(pb);
    htonPktColor(pb);

    drop = ((*predicate)(ip1, ip2, ip3, ip4, color, pb, sin, sock) == 0);
    return drop;
}

void rpc2_XmitPacket(RPC2_PacketBuffer *pb, struct RPC2_addrinfo *addr,
		     int confirm)
{
    static int log_limit = 0;
    int whichSocket, n, flags = 0;
    int delay;

    say(1, RPC2_DebugLevel, "rpc2_XmitPacket()\n");

#ifdef RPC2DEBUG
    if (RPC2_DebugLevel > 9)
	{
	fprintf(rpc2_logfile, "\t");
	rpc2_printaddrinfo(addr, rpc2_logfile);
	if (pb->Prefix.sa && pb->Prefix.sa->encrypt)
	    fprintf(rpc2_logfile, " (secure)");
	fprintf(rpc2_logfile, "\n");
	rpc2_PrintPacketHeader(pb, rpc2_logfile);
	}
#endif

    assert(pb->Prefix.MagicNumber == OBJ_PACKETBUFFER);

    whichSocket = rpc2_v6RequestSocket;

    if (whichSocket == -1 ||
	(rpc2_v4RequestSocket != -1 && addr->ai_family == PF_INET))
	whichSocket = rpc2_v4RequestSocket;

    if (whichSocket == -1)
	return; // RPC2_NOCONNECTION

    TR_XMIT();

    /* Only Internet for now; no name->number translation attempted */

    rpc2_Sent.Total++;
    rpc2_Sent.Bytes += pb->Prefix.LengthOfPacket;

    if (FailPacket(Fail_SendPredicate, pb, addr, whichSocket))
	return;

    delay = LUA_fail_delay(addr, pb, 1);
    if (delay == -1) return; /* drop */
    if (delay > 0 && rpc2_DelayedSend(delay, whichSocket, addr, pb)) return;

    if (confirm)
	flags = msg_confirm;

    n = secure_sendto(whichSocket, &pb->Header,
		      pb->Prefix.LengthOfPacket, flags,
		      addr->ai_addr, addr->ai_addrlen, pb->Prefix.sa);

    if (n == -1 && errno == EAGAIN)
    {
	/* operation failed probably because the send buffer was full. we could
	 * try to select for write and retry, or we could just consider this
	 * packet lost on the network.
	 */
    } else

    if (n == -1 && errno == EINVAL && msg_confirm) {
	/* maybe the kernel didn't like the MSG_CONFIRM flag. */
	msg_confirm = 0;
    }
    else

    if (RPC2_Perror && n != pb->Prefix.LengthOfPacket)
    {
	char msg[100];
	sprintf(msg, "Xmit_Packet socket %d", whichSocket);
	perror(msg);
    }

    /* Log outgoing packets that are larger than the IPv6 MTU
     * (- ipv6 hdr, ipv6 fragment hdr, udp hdr, secure spi/seq/iv/icv)
     *
     * Only log the first 10 oversized packets and only when we know
     * for sure that the headers are still unencrypted and thus, useful */
    if (log_limit < 10 && pb->Prefix.sa &&
	pb->Prefix.LengthOfPacket > (1280 - 40 - 8 - 8 - 24))
    {
	fprintf(rpc2_logfile,
		"XMIT: Sent long packet (subsys %d, opcode %d, length %ld)\n",
		ntohl(pb->Header.SubsysId), ntohl(pb->Header.Opcode),
		pb->Prefix.LengthOfPacket);
	fflush(rpc2_logfile);
	log_limit++;
    }
}

struct security_association *rpc2_GetSA(uint32_t spi)
{
    struct CEntry *ce= __rpc2_GetConn((RPC2_Handle)spi);
    return ce ? &ce->sa : NULL;
}

/* Reads the next packet from whichSocket into whichBuff, sets its
   LengthOfPacket field, fills in whichHost and whichPort, and
   returns 0; Returns -3 iff a too-long packet arrived.  Returns -1 on
   any other system call error.

   Note that whichBuff should at least be able to accomodate 1 byte
   more than the longest receivable packet.  Only Internet packets are
   dealt with currently.  */
long rpc2_RecvPacket(IN long whichSocket, OUT RPC2_PacketBuffer *whichBuff)
{
    long rc, len;
    socklen_t fromlen;
    struct sockaddr_storage ss;

    say(1, RPC2_DebugLevel, "rpc2_RecvPacket()\n");
    assert(whichBuff->Prefix.MagicNumber == OBJ_PACKETBUFFER);

    len = whichBuff->Prefix.BufferSize - (long)(&whichBuff->Header) + (long)(whichBuff);
    assert(len > 0);

    /* WARNING: only Internet works; no warnings */
    fromlen = sizeof(ss);
    rc = secure_recvfrom(whichSocket, &whichBuff->Header, len, 0,
			 (struct sockaddr *) &ss, &fromlen,
			 &whichBuff->Prefix.sa, rpc2_GetSA);

    if (rc < 0 && errno == EAGAIN) {
	/* the packet might have had a corrupt udp checksum */
	return -1;
    }
    if (rc < 0) {
	    say(10, RPC2_DebugLevel, "Error in recvf from: errno = %d\n", errno);
	    return(-1);
    }

    whichBuff->Prefix.PeerAddr =
	RPC2_allocaddrinfo((struct sockaddr *)&ss, fromlen,
			   SOCK_DGRAM, IPPROTO_UDP);

    TR_RECV();

    if (FailPacket(Fail_RecvPredicate, whichBuff, whichBuff->Prefix.PeerAddr,
		   whichSocket))
    {
	    errno = 0;
	    return (-1);
    }

    whichBuff->Prefix.LengthOfPacket = rc;

    if (rc == len) {
	rpc2_Recvd.Giant++;
	return(-3);
    }

    /* Try to get an accurate arrival time estimate for this packet */
    /* This ioctl might be used on linux systems only, but you never know */
#if 0 // defined(SIOCGSTAMP)
/* Very nice for accurate network RTT estimates, but we don't measure the time
 * it takes for the server to wake up and send back the response. i.e. The
 * client will end up assuming the server is faster than it really is so I've
 * disabled this code -JH */
    rc = ioctl(whichSocket, SIOCGSTAMP, &whichBuff->Prefix.RecvStamp);
    if (rc < 0)
#endif
    {
	FT_GetTimeOfDay(&whichBuff->Prefix.RecvStamp, (struct timezone *)0);
    }

    return(0);
}


/*
  Initializes default retry intervals given the number of
  retries desired and the keepalive interval.

  Returns 0 on success, -1 on bogus parameters.
*/
long rpc2_InitRetry(IN long HowManyRetries, IN struct timeval *Beta0)
		/*  HowManyRetries" should be less than 30; -1 for default */
		/*  Beta0: NULL for default */
{
    if (HowManyRetries >= 30) return(-1); /* avoid overflow with 32-bit integers */
    Retry_N = (HowManyRetries >= 0) ? HowManyRetries : DefaultRetryCount;
    KeepAlive = Beta0 ? *Beta0 : DefaultRetryInterval;

    return(0);
}


int RPC2_SetTimeout(RPC2_Handle whichConn, struct timeval timeout)
{
    struct CEntry *Conn = rpc2_GetConn(whichConn);
    if (!Conn) return RPC2_NOCONNECTION;
    Conn->KeepAlive = timeout;
    return RPC2_SUCCESS;
}

long rpc2_CancelRetry(struct CEntry *Conn, struct SL_Entry *Sle)
{
    /* 
     * see if we've heard anything from a side effect
     * since we've been asleep. If so, pretend we got
     * a keepalive at that time, and activate a SLE 
     * with a timeout of beta_0 after that time.
     */
    struct timeval now, lastword;

    say(1, RPC2_DebugLevel, "rpc2_CancelRetry()\n");

    if (Conn->SEProcs && Conn->SEProcs->SE_GetSideEffectTime &&
	(Conn->SEProcs->SE_GetSideEffectTime(Conn->UniqueCID, &lastword) == RPC2_SUCCESS) &&
	TIMERISSET(&lastword)) {  /* don't bother unless we've actually heard */
	FT_GetTimeOfDay(&now, (struct timezone *)0);
	SUBTIME(&now, &lastword);
	say(9, RPC2_DebugLevel,
	    "Heard from side effect on %#x %ld.%06ld ago, retry interval was %ld.%06ld\n",
	     Conn->UniqueCID, now.tv_sec, now.tv_usec,
	     Sle->RInterval.tv_sec, Sle->RInterval.tv_usec);
	if (CMPTIME(&now, &Sle->RInterval, <)) {
	    Sle->RInterval.tv_sec  = Conn->KeepAlive.tv_sec / 3;
	    Sle->RInterval.tv_usec = Conn->KeepAlive.tv_usec / 3;
	    SUBTIME(&Sle->RInterval, &now);

	    say(/*9*/4, RPC2_DebugLevel,
		"Supressing retry %d at %ld on %#x, new timeout = %ld.%06ld\n",
		 Sle->RetryIndex, rpc2_time(), Conn->UniqueCID,
		 Sle->RInterval.tv_sec, Sle->RInterval.tv_usec);

	    rpc2_Sent.Cancelled++;
	    Sle->RetryIndex = 0;
	    rpc2_ActivateSle(Sle, &Sle->RInterval);
	    return(1);
	}
    }
    return(0);
}


long rpc2_SendReliably(struct CEntry *Conn, struct SL_Entry *Sle,
		       RPC2_PacketBuffer *Packet, struct timeval *TimeOut)
{
    struct SL_Entry *tlp;
    long hopeleft, finalrc;

    say(1, RPC2_DebugLevel, "rpc2_SendReliably()\n");

    TR_SENDRELIABLY();

    if (TimeOut != NULL)
    {/* create a time bomb */
	tlp = rpc2_AllocSle(OTHER, NULL);
	rpc2_ActivateSle(tlp, TimeOut);
    }
    else tlp = NULL;

    Conn->reqsize = Packet->Prefix.LengthOfPacket;
    Sle->RetryIndex = 1;
    rpc2_RetryInterval(Conn->HostInfo, Sle, Packet->Prefix.LengthOfPacket,
	    /* XXX we should have the size of the expected reply packet,
	    * somewhere... */ sizeof(struct RPC2_PacketHeader),
		       Conn->Retry_N, &Conn->KeepAlive);

    /* Do an initial send of the packet */
    say(9, RPC2_DebugLevel, "Sending try at %ld on %#x (timeout %ld.%06ld)\n",
			     rpc2_time(), Conn->UniqueCID,
			     Sle->RInterval.tv_sec, Sle->RInterval.tv_usec);

    if (TestRole(Conn, CLIENT))   /* stamp the outgoing packet */
	Packet->Header.TimeStamp = htonl(rpc2_MakeTimeStamp());

    rpc2_XmitPacket(Packet, Conn->HostInfo->Addr, 0);

    /* Initialize the SL Entry */
    rpc2_ActivateSle(Sle, &Sle->RInterval);

    finalrc = RPC2_SUCCESS;
    do
	{
	hopeleft = 0;
	LWP_WaitProcess((char *)Sle);  /* SocketListener will awaken me */

	if (tlp && tlp->ReturnCode == TIMEOUT)
	    {
	    /* Overall timeout expired: clean up state and quit */
	    rpc2_IncrementSeqNumber(Conn);
	    SetState(Conn, C_THINK);
	    finalrc = RPC2_TIMEOUT;
	    break;  /* while */
	    }

	switch(Sle->ReturnCode)
	    {
	    case NAKED:
	    case ARRIVED:
		break;		/* switch */

	    case KEPTALIVE:
		hopeleft = 1;
		Sle->RetryIndex = 0;
		Sle->RInterval.tv_sec  = Conn->KeepAlive.tv_sec / 3;
		Sle->RInterval.tv_usec = Conn->KeepAlive.tv_usec / 3;
		rpc2_ActivateSle(Sle, &Sle->RInterval);
		break;	/* switch */

	    case TIMEOUT:
		if ((hopeleft = rpc2_CancelRetry(Conn, Sle)))
		    break;      /* switch; we heard from side effect recently */
		if (Sle->RetryIndex >= Conn->Retry_N)
		    break;	/* switch; note hopeleft must be 0 */
		/* else retry with the next Beta value  for timeout */
		Sle->RetryIndex += 1;
		rpc2_RetryInterval(Conn->HostInfo, Sle,
				   Packet->Prefix.LengthOfPacket,
		    /* XXX we should have the size of the expected reply
		     * packet, somewhere... */ sizeof(struct RPC2_PacketHeader),
				   Conn->Retry_N, &Conn->KeepAlive);

		if (Sle->RInterval.tv_sec < 0 || Sle->RInterval.tv_usec < 0)
		    break;
		else hopeleft = 1;
		rpc2_ActivateSle(Sle, &Sle->RInterval);
		say(9, RPC2_DebugLevel,
		    "Sending retry %d at %ld on %#x (timeout %ld.%06ld)\n",
		     Sle->RetryIndex, rpc2_time(), Conn->UniqueCID,
		     Sle->RInterval.tv_sec, Sle->RInterval.tv_usec);
		Packet->Header.Flags = htonl((ntohl(Packet->Header.Flags) | RPC2_RETRY));
		if (TestRole(Conn, CLIENT))   /* restamp retries if client */
		    Packet->Header.TimeStamp = htonl(rpc2_MakeTimeStamp());
		rpc2_Sent.Retries += 1;
		rpc2_XmitPacket(Packet, Conn->HostInfo->Addr, 0);
		break;	/* switch */

	    default: assert(FALSE);
	    }
	}
    while (hopeleft);

    if (tlp)
    	{
	rpc2_DeactivateSle(tlp, 0);  	/* delete  time bomb */
	rpc2_FreeSle(&tlp);
	}

    return(finalrc);
    }


/* For converting packet headers to/from network order */
void rpc2_htonp(RPC2_PacketBuffer *p)
{
	p->Header.ProtoVersion = htonl(p->Header.ProtoVersion);
	p->Header.RemoteHandle = htonl(p->Header.RemoteHandle);
	p->Header.LocalHandle = htonl(p->Header.LocalHandle);
	p->Header.Flags = htonl(p->Header.Flags);
	p->Header.BodyLength = htonl(p->Header.BodyLength);
	p->Header.SeqNumber = htonl(p->Header.SeqNumber);
	p->Header.Opcode = htonl(p->Header.Opcode);
	p->Header.SEFlags = htonl(p->Header.SEFlags);
	p->Header.SEDataOffset = htonl(p->Header.SEDataOffset);
	p->Header.SubsysId = htonl(p->Header.SubsysId);
	p->Header.ReturnCode = htonl(p->Header.ReturnCode);
	p->Header.Lamport = htonl(p->Header.Lamport);
	p->Header.Uniquefier = htonl(p->Header.Uniquefier);
	p->Header.TimeStamp = htonl(p->Header.TimeStamp);
	p->Header.BindTime = htonl(p->Header.BindTime);
}

void rpc2_ntohp(RPC2_PacketBuffer *p)
{
	p->Header.ProtoVersion = ntohl(p->Header.ProtoVersion);
	p->Header.RemoteHandle = ntohl(p->Header.RemoteHandle);
	p->Header.LocalHandle = ntohl(p->Header.LocalHandle);
	p->Header.Flags = ntohl(p->Header.Flags);
	p->Header.BodyLength = ntohl(p->Header.BodyLength);
	p->Header.SeqNumber = ntohl(p->Header.SeqNumber);
	p->Header.Opcode = ntohl(p->Header.Opcode);
	p->Header.SEFlags = ntohl(p->Header.SEFlags);
	p->Header.SEDataOffset = ntohl(p->Header.SEDataOffset);
	p->Header.SubsysId = ntohl(p->Header.SubsysId);
	p->Header.ReturnCode = ntohl(p->Header.ReturnCode);
	p->Header.Lamport = ntohl(p->Header.Lamport);
	p->Header.Uniquefier = ntohl(p->Header.Uniquefier);
	p->Header.TimeStamp = ntohl(p->Header.TimeStamp);
	p->Header.BindTime = ntohl(p->Header.BindTime);
}

void rpc2_InitPacket(RPC2_PacketBuffer *pb, struct CEntry *ce, long bodylen)
{
	assert(pb);

	memset(&pb->Header, 0, sizeof(struct RPC2_PacketHeader));
	pb->Header.ProtoVersion = RPC2_PROTOVERSION;
	pb->Header.Lamport	= RPC2_LamportTime();
	pb->Header.BodyLength   = bodylen;
	pb->Prefix.LengthOfPacket = sizeof(struct RPC2_PacketHeader) + bodylen;
	memset(&pb->Prefix.RecvStamp, 0, sizeof(struct timeval));
	if (ce)	{
		pb->Prefix.sa = &ce->sa;
		pb->Header.RemoteHandle = ce->PeerHandle;
		pb->Header.LocalHandle  = ce->UniqueCID;
		pb->Header.SubsysId  = ce->SubsysId;
		pb->Header.Uniquefier = ce->PeerUnique;
		SetPktColor(pb, ce->Color);
	}
}
