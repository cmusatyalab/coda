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


/*

  Autonomous LWP that loops forever.  Effectively the "bottom half" of
  the RPC code.  Driven by timeouts and packet arrivals on the request
  socket.  Communicates with "top half" routines via SL entries.
	
  ALL changes to NextSeqNumber field of connections take place
  entirely within rpc2_SocketListener(). Increments (+2) are performed
  by rpc2_IncrementSeqNumber(), which may be called in the MultiRPC
  case by RPC2_MultiRPC() (if SocketListener return code is WAITING)
  or by rpc2_MSendPacketsReliably() on client-initiated timeout.

*/


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <assert.h>
#include "rpc2.private.h"
#include <rpc2/se.h>
#include "trace.h"

 
void rpc2_IncrementSeqNumber();
static void DelayedAck(struct SL_Entry *sle);
int XlateMcastPacket(RPC2_PacketBuffer *pb);
void HandleInitMulticast();
void rpc2_ProcessPackets(int fd);
void rpc2_ExpireEvents();

static RPC2_PacketBuffer *PullPacket(int fd), *ShrinkPacket();
static struct CEntry *MakeConn(), *FindOrNak();
static struct SL_Entry *FindRecipient();
static int BogusSl(), PacketCame(void);
static void
	    Tell(), HandleSLPacket(), DecodePacket(),
	    HandleCurrentReply(),
	    SendBusy(), HandleBusy(),
	    HandleOldRequest(), HandleNewRequest(), HandleCurrentRequest(),
	    HandleInit1(), HandleInit2(), HandleInit3(), HandleInit4(), 
	    HandleNak(), HandleRetriedBind();

static void SendNak(RPC2_PacketBuffer *pb);

#define BOGUS(p, msg)	do { /* bogus packet; throw it away */\
    say(9, RPC2_DebugLevel, (msg));\
    rpc2_Recvd.Bogus++;\
    RPC2_FreeBuffer(&p); } while (0)

#define NAKIT(p) do { 	/* bogus packet; NAK it and then throw it away */\
    rpc2_Recvd.Bogus++;\
    SendNak(p);\
    RPC2_FreeBuffer(&p); } while (0) 

/* Flag to toggle ip-address/port matching for received packets */
long RPC2_strict_ip_matching = 0;

/* Multiple transports can register a handler with the rpc2 socketlistener
 * when their headers are RPC2 compatible */
#define MAXPACKETHANDLERS 4
static unsigned int nPacketHandlers = 0;
static struct PacketHandlers {
    unsigned int ProtoVersion;
    void (*Handler)(RPC2_PacketBuffer *pb);
} PacketHandlers[MAXPACKETHANDLERS];

void SL_RegisterHandler(unsigned int pv, void (*handler)(RPC2_PacketBuffer *pb))
{
    assert(nPacketHandlers < MAXPACKETHANDLERS);
    PacketHandlers[nPacketHandlers].ProtoVersion = pv;
    PacketHandlers[nPacketHandlers].Handler = handler;
    nPacketHandlers++;
}

void rpc2_SocketListener(void *dummy)
{
    int fd;

    /* just once, at RPC2_Init time, to be nice */
    LWP_DispatchProcess();  

    /* The funny if-do construct  below assures the following:
       1. All packets in the socket buffer are processed before expiring events
       2. The number of select() system calls is kept to bare minimum
    */
    while(TRUE) {
	fd = rpc2_MorePackets();
	if (fd == -1) {
	    do {
		rpc2_ExpireEvents();
		fd = PacketCame();
	    } while (fd == -1);
	}

	rpc2_ProcessPackets(fd);
    }
}

void RPC2_DispatchProcess()
{
	struct timeval tv;
	int fd;

	while ((fd = rpc2_MorePackets()) != -1) {
	    rpc2_ProcessPackets(fd);
	}

	/* keep current time from being too inaccurate */
	(void) FT_GetTimeOfDay(&tv, (struct timezone *) 0);

	/* also check for timed-out events, using current time */
	rpc2_ExpireEvents();

	LWP_DispatchProcess();
	return; 
}


void rpc2_ProcessPackets(int fd)
{
	RPC2_PacketBuffer *pb = NULL;
        unsigned int i, ProtoVersion;
	
	/* We are guaranteed that there is a packet in the socket
           buffer at this point */
	pb = PullPacket(fd);
	if (pb == NULL) 
		return;
	assert(pb->Prefix.Qname == &rpc2_PBList);

        if (pb->Prefix.LengthOfPacket < sizeof(struct RPC2_PacketHeader)) {
            /* avoid memory reference errors */
            BOGUS(pb, "Runt packet\n");
            return;
        }

        ProtoVersion = ntohl(pb->Header.ProtoVersion);
        for (i = 0; i < nPacketHandlers; i++) {
            if (ProtoVersion == PacketHandlers[i].ProtoVersion) {
                PacketHandlers[i].Handler(pb);
                return;
            }
        }

        /* we don't have a ghost of a chance */
        BOGUS(pb, "Wrong version\n");
}

void rpc2_HandlePacket(RPC2_PacketBuffer *pb)
{
        struct CEntry *ce;
	assert(pb->Prefix.Qname == &rpc2_PBList);

	if (ntohl(pb->Header.Flags) & RPC2_MULTICAST) {
	    rpc2_MRecvd.Total++;
	    rpc2_MRecvd.Bytes += pb->Prefix.LengthOfPacket;
	} else {
	    rpc2_Recvd.Total++;
	    rpc2_Recvd.Bytes += pb->Prefix.LengthOfPacket;
	}

	if ((ntohl(pb->Header.LocalHandle) == -1) || 
	    (ntohl(pb->Header.Opcode) == RPC2_NAKED)) {
		assert(pb->Prefix.Qname == &rpc2_PBList);
		HandleSLPacket(pb);
		return;
	}

	/* If the packet came in on the multicast channel, validate it
	   and translate it onto the point-to-point channel. */
	if (ntohl(pb->Header.Flags) & RPC2_MULTICAST)
		if (! (XlateMcastPacket(pb)))
			return;

	if (ntohl(pb->Header.RemoteHandle) == 0)
		ce = NULL;	/* Ought to be a new bind request */
	else {
		ce = FindOrNak(pb);
		if (ce == NULL) 
			return;
		if (!TestState(ce, CLIENT, C_AWAITINIT2) &&
		    !TestState(ce, SERVER, S_AWAITINIT3) &&
		    !TestState(ce, CLIENT, C_AWAITINIT4))
			rpc2_ApplyD(pb,  ce);
	}

	/* debugging */
	Tell(pb, ce);
	/*  pb is a good packet and ce is a good conn */
	rpc2_ntohp(pb);
    
	/* update the host entry if there is one */
	if (ce && ce->HostInfo) 
	    ce->HostInfo->LastWord = pb->Prefix.RecvStamp;

	/* See if smaller packet buffer will do */
	pb = ShrinkPacket(pb);

	/* maintain causality */
	if (pb->Header.Lamport  > rpc2_LamportClock)
		rpc2_LamportClock = pb->Header.Lamport + 1;	

	say(9, RPC2_DebugLevel, "Decoding opcode %ld\n", pb->Header.Opcode);

	DecodePacket(pb, ce);

	say(9, RPC2_DebugLevel, "Decoding complete\n");
}

void rpc2_ExpireEvents()
{
	int i;
	struct SL_Entry *sl;
	struct TM_Elem *t;

	for (i = TM_Rescan(rpc2_TimerQueue); i > 0; i--) {
		t = TM_GetExpired(rpc2_TimerQueue);
		sl = (struct SL_Entry *)t->BackPointer;
		rpc2_DeactivateSle(sl, TIMEOUT);

		if (sl->Type == REPLY) {
		    FreeHeld(sl);
		} else if (sl->Type == DELACK) {
		    DelayedAck(sl);
		} else {
		    LWP_NoYieldSignal((char *)sl);
		}
	}
}


int rpc2_MorePackets(void)
{
    struct timeval tv;
    fd_set rmask;
    int maxfd;

    /* This ioctl peeks into the socket's receive queue, and reports the amount
     * of data ready to be read. Linux officially uses TIOCINQ, but it's an alias
     * for FIONREAD, so this should work too. --JH */
#if defined(FIONREAD)
    int checked = 0, amount_ready = 0;

    if (rpc2_v4RequestSocket != -1 &&
	ioctl(rpc2_v4RequestSocket, FIONREAD, &amount_ready) == 0)
    {
	checked = 1;
	if (amount_ready != 0)
	    return rpc2_v4RequestSocket;
    }

    if (rpc2_v6RequestSocket != -1 &&
	ioctl(rpc2_v6RequestSocket, FIONREAD, &amount_ready) == 0)
    {
	checked = 1;
	if (amount_ready != 0)
	    return rpc2_v6RequestSocket;
    }
    /* if the ioctl worked on a socket, we know that there probably isn't
     * any data waiting for us */
    if (checked)
	return -1;
#endif
    tv.tv_sec = tv.tv_usec = 0;	    /* do polling select */
    FD_ZERO(&rmask);

    if (rpc2_v4RequestSocket != -1)
	FD_SET(rpc2_v4RequestSocket, &rmask);

    if (rpc2_v6RequestSocket != -1)
	FD_SET(rpc2_v6RequestSocket, &rmask);

    maxfd = rpc2_v4RequestSocket + 1;
    if (rpc2_v6RequestSocket >= maxfd)
	maxfd = rpc2_v6RequestSocket + 1;

    /* We use select rather than IOMGR_Select to avoid
       overheads. This is acceptable only because we are doing a
       polling select */
    if (select(maxfd, &rmask, NULL, NULL, &tv) > 0) {
	if (rpc2_v4RequestSocket != -1 &&
	    FD_ISSET(rpc2_v4RequestSocket, &rmask))
	    return rpc2_v4RequestSocket;

	if (rpc2_v6RequestSocket != -1 &&
	    FD_ISSET(rpc2_v6RequestSocket, &rmask))
	    return rpc2_v6RequestSocket;
    }
    return -1;
}


/*  Await the earliest future event or a packet.
    Returns active fd if packet came, -1 if earliest event expired */
static int PacketCame(void)
{
    struct TM_Elem *t;
    struct timeval *tvp;
    fd_set rmask;
    int nfds;

    /* Obtain earliest event */
    t = TM_GetEarliest(rpc2_TimerQueue);
    if (t == NULL) tvp = NULL;
    else	       tvp = &t->TimeLeft;

    /* Yield control */
    say(999, RPC2_DebugLevel, "About to enter IOMGR_Select()\n");
    FD_ZERO(&rmask);

    if (rpc2_v4RequestSocket != -1)
	FD_SET(rpc2_v4RequestSocket, &rmask);

    if (rpc2_v6RequestSocket != -1)
	FD_SET(rpc2_v6RequestSocket, &rmask);

    nfds = rpc2_v4RequestSocket + 1;
    if (rpc2_v6RequestSocket >= nfds)
	nfds = rpc2_v6RequestSocket + 1;

    if (IOMGR_Select(nfds, &rmask, NULL, NULL, tvp) > 0) {
	if (rpc2_v4RequestSocket != -1 &&
	    FD_ISSET(rpc2_v4RequestSocket, &rmask))
	    return rpc2_v4RequestSocket;

	if (rpc2_v6RequestSocket != -1 &&
	    FD_ISSET(rpc2_v6RequestSocket, &rmask))
	    return rpc2_v6RequestSocket;

    }
    return(-1);
}

static RPC2_PacketBuffer *PullPacket(int fd)
{
	RPC2_PacketBuffer *pb = NULL;

	RPC2_AllocBuffer(RPC2_MAXPACKETSIZE-sizeof(RPC2_PacketBuffer), &pb);
	if ( !pb ) 
		assert(0);
	assert(pb->Prefix.Qname == &rpc2_PBList);
	if (rpc2_RecvPacket(fd, pb) < 0) {
		say(9, RPC2_DebugLevel, "Recv error, ignoring.\n");
		RPC2_FreeBuffer(&pb);
		return(NULL);
	}
	assert(pb->Prefix.Qname == &rpc2_PBList);

#ifdef RPC2DEBUG
	if (RPC2_DebugLevel > 9) {
	    fprintf(rpc2_tracefile, "Packet received from   ");
	    rpc2_printaddrinfo(pb->Prefix.PeerAddr, rpc2_tracefile);
	    fprintf(rpc2_tracefile, "\n");
	}
#endif

	return(pb);
}

static void Tell(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
#ifdef RPC2DEBUG
	if (RPC2_DebugLevel < 10) 
		return;

	fprintf(rpc2_tracefile, "Received packet....\n");
	rpc2_PrintPacketHeader(pb, rpc2_tracefile);

	if (ce == NULL) 
		return;
	fprintf(rpc2_tracefile, "Connection 0x%lx state is ...\n",  
		ce->UniqueCID);
	rpc2_PrintCEntry(ce, rpc2_tracefile);	    
#endif
}


/* Special packet from socketlistener: never encrypted */
static void HandleSLPacket(RPC2_PacketBuffer *pb)
{
	struct CEntry *ce;

	rpc2_ntohp(pb);
	
	ce = rpc2_GetConn(pb->Header.RemoteHandle);
	if (ce == NULL) {
		BOGUS(pb, "HandleSLPacket: ce == NULL\n");
		return;
	}

	switch((int) pb->Header.Opcode) {
	case RPC2_NAKED:
		if (TestState(ce, CLIENT, (C_AWAITREPLY|C_AWAITINIT2)))
			HandleNak(pb, ce);
		else {
			assert(pb->Prefix.Qname == &rpc2_PBList);
			BOGUS(pb, "HandleSLPacket: state != AWAIT\n");
		}
		break;
		
	default: BOGUS(pb, "HandleSLPacket: bogus opcode\n");
		break;
    }
}



static struct CEntry *FindOrNak(RPC2_PacketBuffer *pb)
{
	struct CEntry *ce;
	
	ce = rpc2_GetConn(ntohl(pb->Header.RemoteHandle));

	if (ce == NULL ||
	    TestState(ce, CLIENT, C_HARDERROR) ||
	    TestState(ce, SERVER, S_HARDERROR) ||
        //    pb->Header.LocalHandle != ce->PeerHandle ||

	/* Optionally do a bit stronger checking whether the sender of the
	 * packet is really a match. --JH */
	    (RPC2_strict_ip_matching &&
	     !RPC2_cmpaddrinfo(ce->HostInfo->Addr, pb->Prefix.PeerAddr)))
	{
	    /* NAKIT() expects host order */
	    pb->Header.LocalHandle = ntohl(pb->Header.LocalHandle);
	    NAKIT(pb);
	    return(NULL);
	}

	return(ce);
}

static void DecodePacket(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
	switch ((int) pb->Header.Opcode) {
	case RPC2_BUSY: {
		if (ce == NULL) {
			NAKIT(pb);
			return;
		}
		if (!TestState(ce, CLIENT, (C_AWAITINIT2|C_AWAITREPLY))) { 
			BOGUS(pb, "DecodePacket(RPC2_BUSY): state != AWAIT\n"); 
			return; 
		}
		/*
		 * If the state is C_AWAITINIT2, the server is processing
		 * a bind request. Sequence numbers are 0 during the bind,
		 * but a busy during bind will have a sequence number of -1.
		 */
		if (TestState(ce, CLIENT, C_AWAITREPLY) &&
		    ce->NextSeqNumber != pb->Header.SeqNumber-1) { 
			BOGUS(pb, "DecodePacket(RPC2_BUSY): bad seqno\n"); 
			return; 
		}
		if (TestState(ce, CLIENT, C_AWAITINIT2) &&
		    pb->Header.SeqNumber != -1) { 
			BOGUS(pb, "DecodePacket(RPC2_BUSY): bad bind seqno\n"); 
			return; 
		}

		HandleBusy(pb, ce);
		return;
	}

	case RPC2_REPLY: {
		if (ce == NULL) {
			NAKIT(pb); 
			return;
		}
		if (!TestState(ce, CLIENT, C_AWAITREPLY)) { 
			BOGUS(pb, "DecodePacket(RPC2_REPLY): state != AWAIT\n"); 
			return; 
		}
		if (ce->NextSeqNumber != pb->Header.SeqNumber-1) { 
			BOGUS(pb, "DecodePacket(RPC2_REPLY): bad seqno\n"); 
			return; 
		}

		HandleCurrentReply(pb, ce);
		return;
	}


	case RPC2_INIT1OPENKIMONO:
	case RPC2_INIT1AUTHONLY:
	case RPC2_INIT1HEADERSONLY:
	case RPC2_INIT1SECURE: {
		if (ce != NULL) {
			NAKIT(pb); 
			return;
		}
		HandleInit1(pb);
		return;
	}

	case RPC2_INIT2: {
		if (ce == NULL) {
			NAKIT(pb); 
			return;
		}
		if (TestState(ce, CLIENT, C_AWAITINIT2)) {
			HandleInit2(pb, ce);
			return;
		}
		/* anything else */
		BOGUS(pb, "DecodePacket(RPC2_INIT2): state != AWAIT\n"); 
		return;
	}

	case RPC2_INIT4: {
		if (ce == NULL) {
			NAKIT(pb); 
			return;
		}
		if (TestState(ce, CLIENT, C_AWAITINIT4)) {
			HandleInit4(pb, ce);
			return;
		}
		BOGUS(pb, "DecodePacket(RPC2_INIT4): state != AWAIT\n");
		return;
	}

	case RPC2_INIT3: {
		if (ce == NULL || (!TestRole(ce, SERVER))) {
			NAKIT(pb); 
			return;
		}
		HandleInit3(pb, ce);	/* This could be a retry */
		return;
	}

	case RPC2_INITMULTICAST: {
		if (ce == NULL) {
			NAKIT(pb); 
			return;
		}
	    
		if (TestState(ce, SERVER, S_AWAITENABLE)) {
			say(0, RPC2_DebugLevel, "Connection not enabled\n");
			BOGUS(pb, "DecodePacket(INITMC): connection not enabled\n"); 
			return;
		}

		if (TestState(ce, SERVER, S_AWAITREQUEST) &&
		    ce->NextSeqNumber == pb->Header.SeqNumber)
			{
				HandleInitMulticast(pb, ce);
				return;
			}

		if (TestState(ce, SERVER, S_AWAITREQUEST) && 
		    ce->NextSeqNumber == pb->Header.SeqNumber + 2)
			{
				HandleOldRequest(pb, ce);
				return;
			}

		/* We can't have the "HandleCurrentRequest()" case because we don't
		   yield control while processing an InitMulticast request.  Retries
		   are thus always seen as "old requests."
		   We don't special case the MultiRPC "packets ahead of sequence"
		   situation because InitMulticast requests cannot be multicasted. */

		/* Anything else */
		BOGUS(pb, "DecodePacket(INITMC): anything else\n");	
		return;
	}
	/* cannot be any negative opcode XXXXXXXXXXXXXXXX */
	default: {
		if (ce == NULL)	{
			NAKIT(pb);
			return;
		}

		if (TestState(ce, SERVER, S_AWAITENABLE)) {
			say(0, RPC2_DebugLevel, "Connection not enabled\n");
			BOGUS(pb, "DecodePacket: connection not enabled\n"); 
			return;
		}

		if (TestState(ce, SERVER, S_AWAITREQUEST) &&
		    ce->NextSeqNumber == pb->Header.SeqNumber) {
			HandleNewRequest(pb, ce);
			return;
		}

		if (TestState(ce, SERVER, S_AWAITREQUEST) && 
		    ce->NextSeqNumber == pb->Header.SeqNumber + 2) {
			HandleOldRequest(pb, ce);
			return;
		}

		if (TestState(ce, SERVER, (S_PROCESS | S_INSE | S_REQINQUEUE))
		    && ce->NextSeqNumber == pb->Header.SeqNumber + 2) {
			HandleCurrentRequest(pb, ce);
			return;
		}

		/* fix for MultiRPC; nak request packets ahead of sequence */
		if ((pb->Header.SeqNumber > ce->NextSeqNumber + 2) &&
		    (pb->Header.SeqNumber & 1) == 0) {
			NAKIT(pb);
			return;
		}
		/* Anything else */
		BOGUS(pb, "DecodePacket: anything else\n");	
		return;
	}

	}
}



static RPC2_PacketBuffer *ShrinkPacket(RPC2_PacketBuffer *pb)
{
	RPC2_PacketBuffer *pb2 = 0;
	
	if (pb->Prefix.LengthOfPacket <= MEDIUMPACKET) {
		RPC2_AllocBuffer(pb->Header.BodyLength, &pb2);
		if ( !pb2 ) 
			return pb;
		pb2->Prefix.PeerAddr = pb->Prefix.PeerAddr;
		pb->Prefix.PeerAddr = NULL;
		pb2->Prefix.RecvStamp = pb->Prefix.RecvStamp;
		pb2->Prefix.LengthOfPacket = pb->Prefix.LengthOfPacket;
		memcpy(&pb2->Header, &pb->Header, pb->Prefix.LengthOfPacket);
		RPC2_FreeBuffer(&pb);
		return(pb2);
	} else 
		return(pb);
}


int rpc2_FilterMatch(whichF, whichP)
    RPC2_RequestFilter *whichF;	
    RPC2_PacketBuffer *whichP;
    /* Returns TRUE iff whichP matches whichF; FALSE otherwise */
    {
    say(999, RPC2_DebugLevel, "rpc2_FilterMatch()\n");
    switch(whichF->OldOrNew)
	{
	case OLD:
		switch((int) whichP->Header.Opcode)
		    {
		    case RPC2_INIT1OPENKIMONO:
		    case RPC2_INIT1AUTHONLY:
		    case RPC2_INIT1HEADERSONLY:
		    case RPC2_INIT1SECURE:
			return(FALSE);

		    default:	break;
		    }
		break;
		
	case NEW:	
		switch((int) whichP->Header.Opcode)
		    {
		    case RPC2_INIT1OPENKIMONO:
		    case RPC2_INIT1AUTHONLY:
		    case RPC2_INIT1HEADERSONLY:
		    case RPC2_INIT1SECURE:
			break;

		    default:	return(FALSE);
		    }
		break;

	case OLDORNEW:	break;

	default:    assert(FALSE);
	}
	
    switch(whichF->FromWhom)
	{
	case ANY:	return(TRUE);
	
	case ONECONN:	if (whichF->ConnOrSubsys.WhichConn == whichP->Header.RemoteHandle) return(TRUE);
			else return(FALSE);
	
	case ONESUBSYS: if (whichF->ConnOrSubsys.SubsysId == whichP->Header.SubsysId) return(TRUE);
			else return(FALSE);
			
	default:	assert(FALSE);
	}
    assert(0);
    return FALSE;
    /*NOTREACHED*/
    }



static void SendBusy(struct CEntry *ce, int doEncrypt)
{
    RPC2_PacketBuffer *pb;
    unsigned int delta;

    rpc2_Sent.Busies++;

    RPC2_AllocBuffer(0, &pb);
    rpc2_InitPacket(pb, ce, 0);

    delta = TSDELTA(rpc2_MakeTimeStamp(), ce->RequestTime);
    pb->Header.TimeStamp = (unsigned int)ce->TimeStampEcho + delta;
    pb->Header.SeqNumber    = ce->NextSeqNumber-1;
    pb->Header.Opcode	    = RPC2_BUSY;

    rpc2_htonp(pb);
    if (doEncrypt) rpc2_ApplyE(pb, ce);

    rpc2_XmitPacket(pb, ce->HostInfo->Addr, 1);
    RPC2_FreeBuffer(&pb);
}

/* Keep alive for current request */
static void HandleBusy(RPC2_PacketBuffer *pb,  struct CEntry *ce)
{
	struct SL_Entry *sl;

	say(0, RPC2_DebugLevel, "HandleBusy()\n");

	rpc2_Recvd.Busies++;
	if (BogusSl(ce, pb)) 
		return;

	/* update rtt/bw measurements */
	ce->respsize = pb->Prefix.LengthOfPacket;
	rpc2_UpdateRTT(pb, ce);

	/* now continue waiting for the real reply again */
	ce->respsize = 0;

	rpc2_Recvd.GoodBusies++;
	sl = ce->MySl;
	rpc2_DeactivateSle(sl, KEPTALIVE);
	LWP_NoYieldSignal((char *)sl);
	RPC2_FreeBuffer(&pb);
}


/* client receives a reply and bundles it off to the right sle */
static void HandleCurrentReply(pb, ce)
	RPC2_PacketBuffer *pb;
struct CEntry *ce;
{
	struct SL_Entry *sl;

	say(0, RPC2_DebugLevel, "HandleCurrentReply()\n");
	rpc2_Recvd.Replies++;
	/* should this assert ?? XXXX */
	if (BogusSl(ce, pb)) 
		return;
	ce->respsize = pb->Prefix.LengthOfPacket;
	rpc2_UpdateRTT(pb, ce);
	rpc2_Recvd.GoodReplies++;
	sl = ce->MySl;
	sl->Packet = pb;
	SetState(ce, C_THINK);
	rpc2_IncrementSeqNumber(ce);
	rpc2_DeactivateSle(sl, ARRIVED);

	LWP_NoYieldSignal((char *)sl);
}

/* Looks like server code */
static void HandleNewRequest(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
	struct SL_Entry *sl;

	say(0, RPC2_DebugLevel, "HandleNewRequest()\n");

	if (IsMulticast(pb) && ce->Mgrp == NULL) {
		say(0, RPC2_DebugLevel, "Multicast packet received without Mgroup\n");
		BOGUS(pb, "HandleNewRequest: mc packet received w/o mgroup\n");
		return;
	}

	ce->TimeStampEcho = pb->Header.TimeStamp;
	TVTOTS(&pb->Prefix.RecvStamp, ce->RequestTime);

	say(15, RPC2_DebugLevel, "handlenewrequest TS %u RQ %u\n",
	    ce->TimeStampEcho, ce->RequestTime);

	if (IsMulticast(pb)) {
		rpc2_MRecvd.Requests++;
		rpc2_MRecvd.GoodRequests++;
	}    else	{
		rpc2_Recvd.Requests++;
		rpc2_Recvd.GoodRequests++;
	}

	sl = ce->MySl;
	/* Free held packet and SL entry */
	if (sl != NULL) {
		rpc2_DeactivateSle(sl, 0);
		FreeHeld(sl);
	}

	rpc2_IncrementSeqNumber(ce);
	if (IsMulticast(pb))
		ce->Mgrp->NextSeqNumber += 2;

	{ /* set up a timer to send a unsolicited ack response (actually an
	     RCP2_BUSY) within 200ms. */
	    struct timeval tv;
	    tv.tv_sec = 0;
	    tv.tv_usec = 200000;
	    sl = rpc2_AllocSle(DELACK, ce);
	    rpc2_ActivateSle(sl, &tv);
	}

	/* Look for a waiting recipient */
	sl = FindRecipient(pb);
	if (sl != NULL) {
		assert(sl->MagicNumber == OBJ_SLENTRY);
		SetState(ce, S_PROCESS);
		if (IsMulticast(pb)) {
			assert(ce->Mgrp != NULL);
			SetState(ce->Mgrp, S_PROCESS);
		}
		rpc2_DeactivateSle(sl, ARRIVED);
		sl->Packet = pb;
		LWP_NoYieldSignal((char *)sl);
	}  else	{
		/* hold for a future RPC2_GetRequest() */
		rpc2_HoldPacket(pb);
		SetState(ce, S_REQINQUEUE);
		if (IsMulticast(pb)) {
			assert(ce->Mgrp != NULL);
			SetState(ce->Mgrp, S_REQINQUEUE);
		}
	}
}

/* Find a server REQ sle with matching filter for this packet */
static struct SL_Entry *FindRecipient(RPC2_PacketBuffer *pb)
	
{
	long i;
	struct SL_Entry *sl;

	sl = rpc2_SLReqList;
	for (i=0; i < rpc2_SLReqCount; i++) {
		if (sl->ReturnCode == WAITING && 
		    rpc2_FilterMatch(&sl->Filter, pb)) {
			return(sl);
		} else 
			sl = sl->NextEntry;
	}
	return(NULL);
}

static void HandleCurrentRequest(RPC2_PacketBuffer *pbX, struct CEntry *ceA)
{
	say(0, RPC2_DebugLevel, "HandleCurrentRequest()\n");

	if (IsMulticast(pbX))
		rpc2_MRecvd.Requests++;
	else
		rpc2_Recvd.Requests++;

	ceA->TimeStampEcho = pbX->Header.TimeStamp;
	/* if we're modifying the TimeStampEcho, we also have to reset the
	 * RequestTime */
	TVTOTS(&pbX->Prefix.RecvStamp, ceA->RequestTime);

	say(15, RPC2_DebugLevel, "handlecurrentrequest TS %u RQ %u\n",
	    ceA->TimeStampEcho, ceA->RequestTime);

	SendBusy(ceA, TRUE);
	RPC2_FreeBuffer(&pbX);
}


static void HandleInit1(RPC2_PacketBuffer *pb)
{
	struct CEntry *ce;
	struct SL_Entry *sl;

	say(0, RPC2_DebugLevel, "HandleInit1()\n");

	rpc2_Recvd.Requests++;

	/* Have we seen this bind request before? */
	if (pb->Header.Flags & RPC2_RETRY) {
		ce = rpc2_ConnFromBindInfo(pb->Prefix.PeerAddr,
					   pb->Header.Uniquefier);
		if (ce)	{
			ce->TimeStampEcho = pb->Header.TimeStamp;
			/* if we're modifying the TimeStampEcho, we also have
			 * to reset the RequestTime */
			TVTOTS(&pb->Prefix.RecvStamp, ce->RequestTime);

			say(15, RPC2_DebugLevel, "handleinit1 TS %u RQ %u\n",
			    ce->TimeStampEcho, ce->RequestTime);

			HandleRetriedBind(pb, ce);
			return;
		}
	}

	/* Create a connection entry */
	ce = MakeConn(pb);
	if (ce == NULL) {
		/* Packet must have been bogus in some way */
		BOGUS(pb, "HandleInit1: MakeConn failed\n");
		return;
	}

	/* Now fix packet header so that it has a real RemoteHandle */
	pb->Header.RemoteHandle = ce->UniqueCID;

	/* Find a willing LWP in RPC2_GetRequest() and tap him on the shoulder */
	sl = FindRecipient(pb);
	if (sl != NULL) {
		assert(sl->MagicNumber == OBJ_SLENTRY);
		rpc2_DeactivateSle(sl, ARRIVED);
		sl->Packet = pb;
		LWP_NoYieldSignal((char *)sl);
	} else {
		/* hold for a future RPC2_GetRequest() */
		rpc2_HoldPacket(pb);
	}
}


static void HandleRetriedBind(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
    
	say(0, RPC2_DebugLevel, "HandleRetriedBind()\n");
    
	if (!TestRole(ce, SERVER)) {
		BOGUS(pb, "HandleRetriedBind: not server\n");
		return;
	}

	/* The original bind request could be:
	   (1) in the hold queue,
	   (2) in RPC2_GetRequest() on some LWP,
	   (3) already completed.
	*/

	if (TestState(ce, SERVER, S_STARTBIND)) {
		/* Cases (1) and (2) */
		say(0, RPC2_DebugLevel, "Busying Init1 on 0x%lx\n", ce->UniqueCID);

		SendBusy(ce, FALSE);
		RPC2_FreeBuffer(&pb);
		return;
	}
	if (ce->SecurityLevel == RPC2_OPENKIMONO && ce->HeldPacket != NULL) {
		/* Case (3): The Init2 must have been dropped; resend it */
		say(0, RPC2_DebugLevel, "Resending Init2 0x%lx\n",  ce->UniqueCID);
		ce->HeldPacket->Header.TimeStamp = htonl(ce->TimeStampEcho);
		rpc2_XmitPacket(ce->HeldPacket, ce->HostInfo->Addr, 1);
		RPC2_FreeBuffer(&pb);
		return;
	}
	/* This retry is totally bogus */
	BOGUS(pb, "HandleRetriedBind: anything else\n");
	return;
}



static void HandleInit2(RPC2_PacketBuffer *pb,     struct CEntry *ce)
{
	struct SL_Entry *sl;

	say(0, RPC2_DebugLevel, "HandleInit2()\n");

	rpc2_Recvd.Requests++;

	if (BogusSl(ce, pb)) 
		return;
	ce->respsize = pb->Prefix.LengthOfPacket;
	rpc2_UpdateRTT(pb, ce);
	sl = ce->MySl;
	sl->Packet = pb;
	if (ce->SecurityLevel == RPC2_OPENKIMONO)  	
		SetState(ce, C_THINK);
	else 
		SetState(ce, C_AWAITINIT4);
	rpc2_DeactivateSle(sl, ARRIVED);
	LWP_NoYieldSignal((char *)sl);
}

static void HandleInit4(pb, ce)
    RPC2_PacketBuffer *pb;
    struct CEntry *ce;
    {
    struct SL_Entry *sl;

    say(0, RPC2_DebugLevel, "HandleInit4()\n");

    rpc2_Recvd.Requests++;

    if (BogusSl(ce, pb)) return;
    ce->respsize = pb->Prefix.LengthOfPacket;
    rpc2_UpdateRTT(pb, ce);
    sl = ce->MySl;
    sl->Packet = pb;
    SetState(ce, C_THINK);
    rpc2_DeactivateSle(sl, ARRIVED);
    LWP_NoYieldSignal((char *)sl);
    }


static void HandleInit3(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
	struct SL_Entry *sl;

	say(0, RPC2_DebugLevel, "HandleInit3()\n");

	rpc2_Recvd.Requests++;

	/* Am I expecting this packet? */
	if (!TestState(ce, SERVER, S_AWAITINIT3)) {
		if (ce->HeldPacket != NULL) {
			/* My Init4 must have got lost; resend it */
			ce->HeldPacket->Header.TimeStamp = htonl(pb->Header.TimeStamp);    
			rpc2_XmitPacket(ce->HeldPacket, ce->HostInfo->Addr, 1);
		}  else 
			say(0, RPC2_DebugLevel, "Bogus Init3\n");
		/* Throw packet away anyway */
		RPC2_FreeBuffer(&pb);
		return;	
	}

	/* Expected Init3 */
	if (BogusSl(ce, pb)) return;
	ce->TimeStampEcho = pb->Header.TimeStamp;
	TVTOTS(&pb->Prefix.RecvStamp, ce->RequestTime);

	say(15, RPC2_DebugLevel, "handleinit3 TS %u RQ %u\n",
	    ce->TimeStampEcho, ce->RequestTime);

	sl = ce->MySl;
	sl->Packet = pb;
	SetState(ce, S_FINISHBIND);
	rpc2_DeactivateSle(sl, ARRIVED);
	LWP_NoYieldSignal((char *)sl);
}


static void HandleNak(RPC2_PacketBuffer *pb,  struct CEntry *ce)
{
	say(0, RPC2_DebugLevel, "HandleNak()\n");
    
	rpc2_Recvd.Naks++;

	if (BogusSl(ce, pb)) 
		return;
	rpc2_SetConnError(ce);
	rpc2_DeactivateSle(ce->MySl, NAKED);
	LWP_NoYieldSignal((char *)ce->MySl);
	RPC2_FreeBuffer(&pb);
}


/* Sends a NAK packet for remoteHandle on (whichHost, whichPort) pair */
static void SendNak(RPC2_PacketBuffer *pb)
{
	RPC2_PacketBuffer *nakpb;
	RPC2_Handle remoteHandle  = pb->Header.LocalHandle;

	say(0, RPC2_DebugLevel, "Sending NAK\n");
	RPC2_AllocBuffer(0, &nakpb);
	rpc2_InitPacket(nakpb, NULL, 0);
	nakpb->Header.RemoteHandle = remoteHandle; 
	nakpb->Header.LocalHandle = -1;	/* "from SocketListener" */
	nakpb->Header.Opcode = RPC2_NAKED;

	rpc2_htonp(nakpb);
	rpc2_XmitPacket(nakpb, pb->Prefix.PeerAddr, 1);
	RPC2_FreeBuffer(&nakpb);
	rpc2_Sent.Naks++;
}

/* Make a new server connection for incoming Init1 packet pb. Returns
   pointer to the connection made.  FakeBody fields are left in
   network order for RP2Gen code to deal with properly.  See comment
   in RPC2_GetRequest() about this.  All other fields of Init1Body are
   converted to host order.  Returns NULL without allocating
   connection if packet turns out to be bogus in some way */

static struct CEntry *MakeConn(struct RPC2_PacketBuffer *pb)
{
	struct Init1Body *ib1;
	struct CEntry *ce;

	say(9, RPC2_DebugLevel, " Request on brand new connection\n");

	ib1 = (struct Init1Body *)(pb->Body);
	if ( (pb->Header.BodyLength < sizeof(struct Init1Body) - 
	      sizeof(ib1->Text))
	     || (pb->Header.BodyLength < sizeof(struct Init1Body) - 
		 sizeof(ib1->Text)+ ntohl(ib1->FakeBody.ClientIdent.SeqLen))) {
		/* avoid memory reference errors from bogus packets */
		say(0, RPC2_DebugLevel, "Ignoring short Init1 packet\n");
		return(NULL);
	}

	ce = rpc2_AllocConn(pb->Prefix.PeerAddr);
	ce->TimeStampEcho = pb->Header.TimeStamp;
	TVTOTS(&pb->Prefix.RecvStamp, ce->RequestTime);

	say(15, RPC2_DebugLevel, "makeconn TS %u RQ %u\n",
	    ce->TimeStampEcho, ce->RequestTime);

	switch((int) pb->Header.Opcode) {
	case RPC2_INIT1OPENKIMONO:
		ce->SecurityLevel = RPC2_OPENKIMONO;
		ce->NextSeqNumber = 0;
		ce->EncryptionType = 0;
		break;

	case RPC2_INIT1AUTHONLY:
		ce->SecurityLevel = RPC2_AUTHONLY;
		ce->NextSeqNumber = rpc2_NextRandom(0);
		ce->EncryptionType = ntohl(ib1->FakeBody.EncryptionType);
		break;
		
	case RPC2_INIT1HEADERSONLY:
		ce->SecurityLevel = RPC2_HEADERSONLY;
		ce->NextSeqNumber = rpc2_NextRandom(0);
		ce->EncryptionType = ntohl(ib1->FakeBody.EncryptionType);
		break;

	case RPC2_INIT1SECURE:
		ce->SecurityLevel = RPC2_SECURE;
		ce->NextSeqNumber = rpc2_NextRandom(0);
		ce->EncryptionType = ntohl(ib1->FakeBody.EncryptionType);
		break;
		
	default:  assert(FALSE);
	}
	
	SetRole(ce, SERVER);
	SetState(ce, S_STARTBIND);
	ce->PeerHandle = pb->Header.LocalHandle;
	ce->SubsysId   = pb->Header.SubsysId;
	ce->PeerUnique = pb->Header.Uniquefier;
	ce->SEProcs = NULL;
	ce->Color = GetPktColor(pb);

#ifdef RPC2DEBUG
	if (RPC2_DebugLevel > 9) {
		printf("New Connection %p......\n",  ce);
		rpc2_PrintCEntry(ce, rpc2_tracefile);
		(void) fflush(rpc2_tracefile);
	}
#endif
	
	rpc2_NoteBinding(pb->Prefix.PeerAddr, pb->Header.Uniquefier, ce->UniqueCID);
	return(ce);
}

void rpc2_IncrementSeqNumber(struct CEntry *ce)
{
	ce->NextSeqNumber += 2;
}



static void HandleOldRequest(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
	say(0, RPC2_DebugLevel, "HandleOldRequest()\n");

	if (IsMulticast(pb))
		rpc2_MRecvd.Requests++;
	else
		rpc2_Recvd.Requests++;

	if (ce->HeldPacket != NULL) {
			ce->HeldPacket->Header.TimeStamp = htonl(pb->Header.TimeStamp);
			rpc2_XmitPacket(ce->HeldPacket, ce->HostInfo->Addr, 1);
		}
	RPC2_FreeBuffer(&pb);
}

void DelayedAck(struct SL_Entry *sle)
{
	struct CEntry *ce;
    
	ce = rpc2_GetConn(sle->Conn);
	SendBusy(ce, TRUE);
	rpc2_FreeSle(&ce->MySl);
}

void FreeHeld(struct SL_Entry *sle)
{
	struct CEntry *ce;
    
	ce = rpc2_GetConn(sle->Conn);
	RPC2_FreeBuffer(&ce->HeldPacket);
	rpc2_FreeSle(&ce->MySl);
}

static int BogusSl(struct CEntry *ce, RPC2_PacketBuffer *pb)
{
	struct SL_Entry *sl;

	sl = ce->MySl;
	if (sl == NULL){ 
		BOGUS(pb, "BogusSL: sl == NULL\n"); 
		return(-1); 
	}
	if (sl->Conn != ce->UniqueCID) { 
		BOGUS(pb, "BogusSL: sl->Conn != ce->UniqueCID\n"); 
		return(-1); 
	}
	if (sl->ReturnCode != WAITING && sl->ReturnCode != KEPTALIVE){
		BOGUS(pb, "BogusSL: sl->ReturnCode != WAITING && sl->ReturnCode != KEPTALIVE\n"); 
		return(-1); 
	}
	return(0);
}
