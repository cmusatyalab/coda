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
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <assert.h>
#include <lwp/lwp.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <rpc2/multi.h>
#include "rpc2.private.h"
#include "trace.h"
#include "cbuf.h"



/*
Contains the hard core of the major RPC runtime routines.

Protocol
========
    Client sends request to server.
    Retries from client provoke BUSY until server finishes servicing request.
    Server sends reply to client and holds the reply packet for a fixed amount of
	time after that. Server LWP is released immediately.
    Retries from client cause saved reply packet to be sent out.
    Reply packet is released on next request or on a timeout.  Further client retries
	are ignored.
	

Connection Invariants:
=====================
	1. State:	  
	                  Modified in GetRequest, SendResponse,
	                  MakeRPC, MultiRPC, Bind and SocketListener.
	                  Always C_THINK in client code.  In
	                  S_AWAITREQUEST when not servicing a request,
	                  in server code.  In S_PROCESS between a
	                  GetRequest and a SendResponse in server
	                  code.  Other values used only during bind
	                  sequence.  Set to S_HARDERROR or C_HARDERROR
	                  on a permanent error.

	2. NextSeqNumber: Initialized by connection creation code in
			GetRequest.  ALWAYS updated by SocketListener,
			except in the RPC2_MultiRPC case.  Updated in
			RPC2_MultiRPC() if SocketListener return code
			is WAITING.  On client side, in state C_THINK,
			this value is the next outgoing request's
			sequence number.  On server side, in state
			S_AWAITREQUEST, this value is the next
			incoming request's sequence number.

NOTE 1
======

    The code works with the LWP preemption package.  All user-callable
    RPC2 routines are in critical sections.  The independent LWPs such
    as SocketListener, side-effect LWPs, etc. are totally
    non-preemptible, since they do not do a PRE_PreemptMe() call.  The
    only lower-level RPC routines that have to be explicitly bracketed
    by critical sections are the randomize and encryption routines
    which are useful independent of RPC2.  
*/

#define HAVE_SE_FUNC(xxx) (ce->SEProcs && ce->SEProcs->xxx)

void SavePacketForRetry();
static int InvokeSE(), ServerHandShake();
static void SendOKInit2(), RejectBind(), Send4AndSave();
static RPC2_PacketBuffer *Send2Get3();
static RPC2_PacketBuffer *HeldReq(RPC2_RequestFilter *filter, struct CEntry **ce);
static int GetFilter(RPC2_RequestFilter *inf, RPC2_RequestFilter *outf);
static long GetNewRequest(IN RPC2_RequestFilter *filter, IN struct timeval *timeout, OUT struct RPC2_PacketBuffer **pb, OUT struct CEntry **ce);
static long MakeFake(INOUT RPC2_PacketBuffer *pb, IN struct CEntry *ce, RPC2_Integer *AuthenticationType, OUT long *xrand, OUT RPC2_CountedBS *cident);
static long Test3(RPC2_PacketBuffer *pb, struct CEntry *ce, long yrand, RPC2_EncryptionKey ekey);

FILE *rpc2_logfile;
FILE *rpc2_tracefile;
extern struct timeval SaveResponse;

void RPC2_SetLog(FILE *file, int level)
{
	if (file) { 
		rpc2_logfile = file;
		rpc2_tracefile = file;
	}
	RPC2_DebugLevel = level;
}

static void rpc2_StampPacket(struct CEntry *ce, struct RPC2_PacketBuffer *pb)
{
    unsigned int delta;
    
    assert(ce->RequestTime);

    delta = TSDELTA(rpc2_MakeTimeStamp(), ce->RequestTime);
    pb->Header.TimeStamp = (unsigned int)ce->TimeStampEcho + delta;

    say(15, RPC2_DebugLevel, "TSin %u delta %u TSout %lu\n",
	ce->TimeStampEcho, delta, pb->Header.TimeStamp);
}

long RPC2_SendResponse(IN RPC2_Handle ConnHandle, IN RPC2_PacketBuffer *Reply)
{
    RPC2_PacketBuffer *preply, *pretry;
    struct CEntry *ce;
    long rc;

    rpc2_Enter();
    say(0, RPC2_DebugLevel, "RPC2_SendResponse()\n");
    assert(!Reply || Reply->Prefix.MagicNumber == OBJ_PACKETBUFFER);

    /* Perform sanity checks */
    ce = rpc2_GetConn(ConnHandle);
    if (!ce) rpc2_Quit(RPC2_NOCONNECTION);
    if (!TestState(ce, SERVER, S_PROCESS)) 	rpc2_Quit(RPC2_NOTWORKER);

    /* set connection state */
    SetState(ce, S_AWAITREQUEST);
    if (ce->Mgrp) SetState(ce->Mgrp, S_AWAITREQUEST);

    /* return if we have no reply to send */
    if (!Reply) rpc2_Quit(RPC2_FAIL);

    TR_SENDRESPONSE();

    { /* Cancel possibly pending delayed ack response, the error code is
       * ignored, but RPC2_ABANDONED just looked nice :) */
	if (ce->MySl) {
	    rpc2_DeactivateSle(ce->MySl, RPC2_ABANDONED);
	    rpc2_FreeSle(&ce->MySl);
	}
    }
    
    preply = Reply;	/* side effect routine usually does not reallocate
			 * packet. preply will be the packet actually sent
			 * over the wire */

    rc = preply->Header.ReturnCode; /* InitPacket clobbers it */
    rpc2_InitPacket(preply, ce, preply->Header.BodyLength);
    preply->Header.ReturnCode = rc;
    preply->Header.Opcode = RPC2_REPLY;
    preply->Header.SeqNumber = ce->NextSeqNumber-1;
    			/* SocketListener has already updated NextSeqNumber */

    rc = RPC2_SUCCESS;	/* tentative, for sendresponse */
    /* Notify side effect routine, if any */
    if (HAVE_SE_FUNC(SE_SendResponse))
	rc = (*ce->SEProcs->SE_SendResponse)(ConnHandle, &preply);

    /* Allocate retry packet before encrypting Bodylength */ 
    RPC2_AllocBuffer(preply->Header.BodyLength, &pretry); 

    if (ce->TimeStampEcho) /* service time is now-requesttime */
	rpc2_StampPacket(ce, preply);

    /* Sanitize packet */
    rpc2_htonp(preply);
    rpc2_ApplyE(preply, ce);

    /* Send reply */
    say(9, RPC2_DebugLevel, "Sending reply\n");
    rpc2_XmitPacket(rpc2_RequestSocket, preply, ce->HostInfo->Addr, 1);

    /* Save reply for retransmission */
    memcpy(&pretry->Header, &preply->Header, preply->Prefix.LengthOfPacket);
    pretry->Prefix.LengthOfPacket = preply->Prefix.LengthOfPacket;
    SavePacketForRetry(pretry, ce);
    
    if (preply != Reply) RPC2_FreeBuffer(&preply);  /* allocated by SE routine */
    rpc2_Quit(rc);
}


long RPC2_GetRequest(IN RPC2_RequestFilter *Filter,
		     OUT RPC2_Handle *ConnHandle,
		     OUT RPC2_PacketBuffer **Request,
		     IN struct timeval *BreathOfLife,
		     IN RPC2_GetKeys_func *GetKeys,
		     IN long EncryptionTypeMask,
		     IN RPC2_AuthFail_func *AuthFail)
{
	struct CEntry *ce;
	RPC2_RequestFilter myfilter;
	RPC2_PacketBuffer *pb;
	RPC2_Integer AuthenticationType;
	RPC2_CountedBS cident;
	long rc, saveXRandom;

	rpc2_Enter();
	say(0, RPC2_DebugLevel, "RPC2_GetRequest()\n");
	    
	TR_GETREQUEST();

/* worthless request */
#define DROPIT()  do { \
	    rpc2_SetConnError(ce); \
	    RPC2_FreeBuffer(Request); \
	    (void) RPC2_Unbind(*ConnHandle); \
	    goto ScanWorkList; \
	} while(0);


	if (!GetFilter(Filter, &myfilter)) 
		rpc2_Quit(RPC2_BADFILTER);

 ScanWorkList:
	pb = HeldReq(&myfilter, &ce);
	if (!pb) {
		/* await a proper request */
		rc = GetNewRequest(&myfilter, BreathOfLife, &pb, &ce);
		if (rc != RPC2_SUCCESS) 
			rpc2_Quit(rc);
	}

	if (!TestState(ce, SERVER, S_STARTBIND)) {
		SetState(ce, S_PROCESS);
		if (IsMulticast(pb)) {
			assert(ce->Mgrp != NULL);
			SetState(ce->Mgrp, S_PROCESS);
		}
	}

	/* Invariants here:
	   (1) pb points to a request packet, decrypted and nettohosted
	   (2) ce is the connection associated with pb
	   (3) ce's state is S_STARTBIND if this is a new bind, 
	   S_PROCESS otherwise
	*/

    *Request = pb;
    *ConnHandle = ce->UniqueCID;

    if (!TestState(ce, SERVER, S_STARTBIND))
	{/* not a bind request */
	say(9, RPC2_DebugLevel, "Request on existing connection\n");

	rc = RPC2_SUCCESS;
	
	/* Notify side effect routine, if any */
	if (HAVE_SE_FUNC(SE_GetRequest))
	    {
	    rc = (*ce->SEProcs->SE_GetRequest)(*ConnHandle, *Request);
	    if (rc != RPC2_SUCCESS)
	    	{
	    	RPC2_FreeBuffer(Request);
	    	if (rc < RPC2_FLIMIT) rpc2_SetConnError(ce);
	    	}
	    }
	rpc2_Quit(rc);
	}

    /* Bind packet on a brand new connection
       Extract relevant fields from Init1 packet and then
	make it a fake NEWCONNECTION packet */

    rc = MakeFake(pb, ce, &saveXRandom, &AuthenticationType, &cident);
    if (rc < RPC2_WLIMIT) {DROPIT();}

    /* Do rest of bind protocol */
    if (ce->SecurityLevel == RPC2_OPENKIMONO)
    {
        RPC2_EncryptionKey SharedSecret;
        /* Abort if we cannot get `keys' for a NULL client */
        if (GetKeys && GetKeys(&AuthenticationType, NULL, SharedSecret,
			       ce->SessionKey) != 0)
	{
            RejectBind(ce, (long) sizeof(struct Init2Body), (long) RPC2_INIT2);
            rc = RPC2_NOTAUTHENTICATED;
	    DROPIT();
	}
	SendOKInit2(ce);
    }
    else
	{
	rc = ServerHandShake(ce, AuthenticationType, &cident, saveXRandom, GetKeys, EncryptionTypeMask);
	if (rc!= RPC2_SUCCESS)
	    {
	    if (AuthFail)
		{/* Client could be iterating through keys; log this */
		    RPC2_HostIdent Host; RPC2_PortIdent Port;
		    rpc2_splitaddrinfo(&Host, &Port, ce->HostInfo->Addr);
		    (*AuthFail)(AuthenticationType, &cident, ce->EncryptionType,
				&Host, &Port);
		    if (Host.Tag == RPC2_HOSTBYADDRINFO)
			RPC2_freeaddrinfo(Host.Value.AddrInfo);
		}
	    DROPIT();
	    }
	}

    /* Do final processing: we need is RPC2_Enable() */
    SetState(ce, S_AWAITENABLE); 

    /* Call side effect routine if present */
    if (HAVE_SE_FUNC(SE_NewConnection))
    {
	rc = (*ce->SEProcs->SE_NewConnection)(*ConnHandle, &cident);
	if (rc < RPC2_FLIMIT) { DROPIT(); }
    }

    /* And now we are really done */
    if (ce->Flags & CE_OLDV)
    {
	char addr[RPC2_ADDRSTRLEN];
	RPC2_formataddrinfo(ce->HostInfo->Addr, addr, RPC2_ADDRSTRLEN);
	say(-1, RPC2_DebugLevel, "Request from %s, Old rpc2 version\n", addr);

	/* Get rid of allocated connection entry. */
	DROPIT();
    }
    else rpc2_Quit(RPC2_SUCCESS);

#undef DROPIT
}

long RPC2_MakeRPC(RPC2_Handle ConnHandle, RPC2_PacketBuffer *Request,
		  SE_Descriptor *SDesc, RPC2_PacketBuffer **Reply,
		  struct timeval *BreathOfLife, long EnqueueRequest)
/* 'RPC2_PacketBuffer *Request' Gets clobbered during call: BEWARE */
{
    struct CEntry *ce;
    struct SL_Entry *sl;
    RPC2_PacketBuffer *preply = NULL;
    RPC2_PacketBuffer *preq;
    long rc, secode = RPC2_SUCCESS, finalrc, opcode;

    rpc2_Enter();
    say(0, RPC2_DebugLevel, "RPC2_MakeRPC()\n");
	    
    TR_MAKERPC();

    /* Perform sanity checks */
    assert(Request->Prefix.MagicNumber == OBJ_PACKETBUFFER);
    
    /* Zero out reply pointer */
    *Reply = NULL;

    /* verify and set connection state, with possible enqueueing;
	cannot factor out the verification, since other LWPs may grab
	ConnHandle in race after wakeup */
    while(TRUE)
	{
	ce = rpc2_GetConn(ConnHandle);
	if (!ce) rpc2_Quit(RPC2_NOCONNECTION);
	if (TestState(ce, CLIENT, C_HARDERROR)) rpc2_Quit(RPC2_FAIL);
	if (TestState(ce, CLIENT, C_THINK)) break;
	if (SDesc && ce->sebroken) rpc2_Quit(RPC2_SEFAIL2);

	if (!EnqueueRequest) rpc2_Quit(RPC2_CONNBUSY);
	say(0, RPC2_DebugLevel, "Enqueuing on connection 0x%lx\n",ConnHandle);
	LWP_WaitProcess((char *)ce);
	say(0, RPC2_DebugLevel, "Dequeueing on connection 0x%lx\n", ConnHandle);
	}
    /* XXXXXX race condition with preemptive threads */
    SetState(ce, C_AWAITREPLY);

	

    preq = Request;	/* side effect routine usually does not reallocate packet */
			/* preq will be the packet actually sent over the wire */

    /* Complete  header fields and sanitize */
    opcode = preq->Header.Opcode;   /* InitPacket clobbers it */
    rpc2_InitPacket(preq, ce, preq->Header.BodyLength);
    preq->Header.SeqNumber = ce->NextSeqNumber;
    preq->Header.Opcode = opcode;
    preq->Header.BindTime = ce->RTT >> RPC2_RTT_SHIFT;  /* bind time on 1st rpc */
    if (ce->RTT && preq->Header.BindTime == 0) preq->Header.BindTime = 1;  /* ugh */

    /* Notify side effect routine, if any */
    if (SDesc && HAVE_SE_FUNC(SE_MakeRPC1))
	if ((secode = (*ce->SEProcs->SE_MakeRPC1)(ConnHandle, SDesc, &preq)) != RPC2_SUCCESS)
	    {
	    if (secode > RPC2_FLIMIT)
		{
		rpc2_Quit(RPC2_SEFAIL1);
		}
	    rpc2_SetConnError(ce);	/* does signal on ConnHandle also */
	    rpc2_Quit(RPC2_SEFAIL2);
	    }


    rpc2_htonp(preq);
    rpc2_ApplyE(preq, ce);

    /* send packet and await reply*/

    say(9, RPC2_DebugLevel, "Sending request on  0x%lx\n", ConnHandle);
    /* create call entry */
    sl = rpc2_AllocSle(OTHER, ce);
    rc = rpc2_SendReliably(ce, sl, preq, BreathOfLife);
    
    switch((int) rc)
	{
	case RPC2_SUCCESS:	break;

	case RPC2_TIMEOUT:	/* don't destroy the connection */
				say(9, RPC2_DebugLevel,
				    "rpc2_SendReliably()--> %s on 0x%lx\n",
				    RPC2_ErrorMsg(rc), ConnHandle);
				rpc2_FreeSle(&sl);
				/* release packet allocated by SE routine */
				if (preq != Request) RPC2_FreeBuffer(&preq);
				goto SendReliablyError;

	default:		assert(FALSE);
	}

    
    switch(sl->ReturnCode)
	{
	case ARRIVED:
		say(9, RPC2_DebugLevel, 
			"Request reliably sent on 0x%lx\n", ConnHandle);
		*Reply = preply = sl->Packet;
		rpc2_FreeSle(&sl);
		/* release packet allocated by SE routine */
		if (preq != Request) RPC2_FreeBuffer(&preq);
		rc = RPC2_SUCCESS;
		break;
	
	case TIMEOUT:
		say(9, RPC2_DebugLevel,	"Request failed on 0x%lx\n", ConnHandle);
		rpc2_FreeSle(&sl);
		rpc2_SetConnError(ce);	/* does signal on ConnHandle also */
		/* release packet allocated by SE routine */
		if (preq != Request) RPC2_FreeBuffer(&preq);
		rc = RPC2_DEAD;
		break;

	case NAKED:
		say(9, RPC2_DebugLevel,
		    "Request NAK'ed on 0x%lx\n", ConnHandle);
		rpc2_FreeSle(&sl);
		rpc2_SetConnError(ce);	/* does signal on ConnHandle also */
		/* release packet allocated by SE routine */
		if (preq != Request) RPC2_FreeBuffer(&preq);
		rc = RPC2_NAKED;
		break;

	default: assert(FALSE);
	}

    /* At this point, if rc == RPC2_SUCCESS, the final reply has been received.
	SocketListener has already decrypted it */

    /* Notify side effect routine, if any.  It may modify the received packet. */
SendReliablyError:
    if (SDesc && HAVE_SE_FUNC(SE_MakeRPC2))
	{
	secode = (*ce->SEProcs->SE_MakeRPC2)(ConnHandle, SDesc, (rc == RPC2_SUCCESS)? preply : NULL);

	if (secode < RPC2_FLIMIT)
	    {
	    ce->sebroken = TRUE;
	    finalrc = secode;
	    goto QuitMRPC;
	    }
	}

    if (rc == RPC2_SUCCESS)
	{
	if (SDesc != NULL && (secode != RPC2_SUCCESS || 
    		SDesc->LocalStatus == SE_FAILURE || SDesc->RemoteStatus == SE_FAILURE))
	    {
	    finalrc = RPC2_SEFAIL1;
	    }
	else finalrc = RPC2_SUCCESS;
	}
    else finalrc = rc;
    
QuitMRPC: /* finalrc has been correctly set by now */

    /* wake up any enqueue'd threads */
    LWP_NoYieldSignal((char *)ce);

    rpc2_Quit(finalrc);
}
    

    
    
long RPC2_NewBinding(IN RPC2_HostIdent *Host, IN RPC2_PortIdent *Port,
		     IN RPC2_SubsysIdent *Subsys, IN RPC2_BindParms *Bparms,
		     IN RPC2_Handle *ConnHandle)
{
    struct CEntry *ce;	/* equal to *ConnHandle after allocation */
    RPC2_PacketBuffer *pb;
    long i;
    struct Init1Body *ib;
    struct Init2Body *ib2;
    struct Init3Body *ib3;
    struct Init4Body *ib4;    
    struct SL_Entry *sl;
    long rc, init2rc, init4rc, savexrandom = 0, saveyrandom = 0, bsize;
    struct RPC2_addrinfo *addr, *peeraddrs;

#define DROPCONN()\
	    {rpc2_SetConnError(ce); (void) RPC2_Unbind(*ConnHandle); *ConnHandle = 0;}

    rpc2_Enter();
    say(0, RPC2_DebugLevel, "In RPC2_NewBinding()\n");

    TR_BIND();

    switch ((int) Bparms->SecurityLevel) {
    case RPC2_OPENKIMONO:
	    break;

    case RPC2_AUTHONLY:
    case RPC2_HEADERSONLY:
    case RPC2_SECURE:
	    /* unknown encryption type */
	    if ((Bparms->EncryptionType & RPC2_ENCRYPTIONTYPES) == 0) 
		    rpc2_Quit (RPC2_FAIL); 	
	    /* tell me just one */
	    if (MORETHANONEBITSET(Bparms->EncryptionType)) 
		    rpc2_Quit(RPC2_FAIL);
	    break;
	    
    default:	
	    rpc2_Quit(RPC2_FAIL);	/* bogus security level */
    }
    
    
    /* Step 0: Resolve bind parameters */
    peeraddrs = rpc2_resolve(Host, Port);
    if (!peeraddrs)
	rpc2_Quit(RPC2_NOBINDING);

    say(9, RPC2_DebugLevel, "Bind parameters successfully resolved\n");

try_next_addr:
    addr = peeraddrs;
    peeraddrs = addr->ai_next;
    addr->ai_next = NULL;

    /* Step 1: Obtain and initialize a new connection */
    ce = rpc2_AllocConn(addr);
    *ConnHandle = ce->UniqueCID;
    say(9, RPC2_DebugLevel, "Allocating connection 0x%lx\n", *ConnHandle);
    SetRole(ce, CLIENT);
    SetState(ce, C_AWAITINIT2);
    ce->PeerUnique = rpc2_NextRandom(NULL);

    switch((int) Bparms->SecurityLevel)  {
    case RPC2_OPENKIMONO:
	    ce->SecurityLevel = RPC2_OPENKIMONO;
	    ce->NextSeqNumber = 0;
	    ce->EncryptionType = 0;
	    break;
		
    case RPC2_AUTHONLY:
    case RPC2_HEADERSONLY:
    case RPC2_SECURE:
	    ce->SecurityLevel = Bparms->SecurityLevel;
	    ce->EncryptionType = Bparms->EncryptionType;
	    /* NextSeqNumber will be filled in in the last step of the handshake */
	    break;	
    }

    /* Obtain pointer to appropriate set of side effect routines */
    if (Bparms->SideEffectType != 0) {
	    for (i = 0; i < SE_DefCount; i++)
		    if (SE_DefSpecs[i].SideEffectType == Bparms->SideEffectType) 
			    break;
	    if (i >= SE_DefCount) {
		    DROPCONN();
		    rpc2_Quit (RPC2_FAIL);	/* bogus side effect */
	    }
	    ce->SEProcs = &SE_DefSpecs[i];
    }
    else 
	    ce->SEProcs = NULL;

    /* Call side effect routine if present */
    if (HAVE_SE_FUNC(SE_Bind1)) {
	    rc = (*ce->SEProcs->SE_Bind1)(*ConnHandle, Bparms->ClientIdent);
	    if (rc != RPC2_SUCCESS) {
		    DROPCONN();
		    rpc2_Quit(rc);
	    }
    }

    assert(Subsys->Tag == RPC2_SUBSYSBYID);
    ce->SubsysId = Subsys->Value.SubsysId;

    /* Step 2: Construct Init1 packet */
    bsize = sizeof(struct Init1Body)  - sizeof(ib->Text) + 
		    (Bparms->ClientIdent == NULL ? 0 : Bparms->ClientIdent->SeqLen);
    RPC2_AllocBuffer(bsize, &pb);
    rpc2_InitPacket(pb, ce, bsize);
    SetPktColor(pb, Bparms->Color);

    switch((int) Bparms->SecurityLevel)
	{
	case RPC2_OPENKIMONO:
	    pb->Header.Opcode = RPC2_INIT1OPENKIMONO;
	    break;

	case RPC2_AUTHONLY:
	    pb->Header.Opcode = RPC2_INIT1AUTHONLY;
	    break;

	case RPC2_HEADERSONLY:
	    pb->Header.Opcode = RPC2_INIT1HEADERSONLY;
	    break;

	case RPC2_SECURE:
	    pb->Header.Opcode = RPC2_INIT1SECURE;
	    break;

	default:
	    RPC2_FreeBuffer(&pb);
	    DROPCONN();
	    rpc2_Quit(RPC2_FAIL);	/* bogus security level  specified */
	}

    /* Fill in the body */
    ib = (struct Init1Body *)pb->Body;
    ib->FakeBody.SideEffectType = htonl(Bparms->SideEffectType);
    ib->FakeBody.SecurityLevel = htonl(Bparms->SecurityLevel);
    ib->FakeBody.EncryptionType = htonl(Bparms->EncryptionType);
    ib->FakeBody.AuthenticationType = htonl(Bparms->AuthenticationType);
    if (Bparms->ClientIdent == NULL)
	ib->FakeBody.ClientIdent.SeqLen = 0;
    else
	{
	ib->FakeBody.ClientIdent.SeqLen = htonl(Bparms->ClientIdent->SeqLen);
	ib->FakeBody.ClientIdent.SeqBody = ib->Text;    /* not really meaningful: this is pointer has to be reset on other side */
	memcpy(ib->Text, Bparms->ClientIdent->SeqBody, Bparms->ClientIdent->SeqLen);
	}
    assert(sizeof(RPC2_VERSION) < sizeof(ib->Version));
    (void) strcpy(ib->Version, RPC2_VERSION);

    rpc2_htonp(pb);	/* convert header to network order */

    if (Bparms->SecurityLevel != RPC2_OPENKIMONO)
	{
	savexrandom = rpc2_NextRandom(NULL);
	say(9, RPC2_DebugLevel, "XRandom = %ld\n",savexrandom);
	ib->XRandom = htonl(savexrandom);

	rpc2_Encrypt((char *)&ib->XRandom, (char *)&ib->XRandom, sizeof(ib->XRandom),
		(char *)Bparms->SharedSecret, Bparms->EncryptionType);	/* in-place encryption */
	}

    /* Step3: Send INIT1 packet  and wait for reply */

    /* send packet and await positive acknowledgement (i.e., RPC2_INIT2 packet) */

    say(9, RPC2_DebugLevel, "Sending INIT1 packet on 0x%lx\n", *ConnHandle);
    /* create call entry */
    sl = rpc2_AllocSle(OTHER, ce);
    rpc2_SendReliably(ce, sl, pb, (struct timeval *)&ce->Retry_Beta[0]);

    switch(sl->ReturnCode)
	{
	case ARRIVED:
		say(9, RPC2_DebugLevel, "Received INIT2 packet on 0x%lx\n", *ConnHandle);
		RPC2_FreeBuffer(&pb);	/* release the Init1 Packet */
		pb = sl->Packet;		/* and get the Init2 Packet */
		rpc2_FreeSle(&sl);
		break;
	
	case NAKED:
	case TIMEOUT:
	case KEPTALIVE:
		/* free connection, buffers, and quit */
		say(9, RPC2_DebugLevel, "Failed to send INIT1 packet on 0x%lx\n", *ConnHandle);
		RPC2_FreeBuffer(&pb);	/* release the Init1 Packet */
		rc = sl->ReturnCode == NAKED ? RPC2_NAKED : RPC2_NOBINDING;
		rpc2_FreeSle(&sl);
		DROPCONN();
		if (rc == RPC2_NOBINDING && peeraddrs)
		    goto try_next_addr;
		rpc2_Quit(rc);
		
	default:	assert(FALSE);
	}

    /* At this point, pb points to the Init2 packet */

    /* Step3: Examine Init2 packet, get bind info (at least
       PeerHandle) and continue with handshake sequence */
    /* is usually RPC2_SUCCESS or RPC2_OLDVERSION */
    init2rc = pb->Header.ReturnCode;	

    /* Authentication failure typically */
    if (init2rc  < RPC2_ELIMIT) {
	    DROPCONN();
	    RPC2_FreeBuffer(&pb);
	    rpc2_Quit(init2rc);	
    }

    /* We have a good INIT2 packet in pb */
    if (Bparms->SecurityLevel != RPC2_OPENKIMONO) {
	    ib2 = (struct Init2Body *)pb->Body;
	    rpc2_Decrypt((char *)ib2, (char *)ib2, sizeof(struct Init2Body), 
			 (char *)Bparms->SharedSecret,
			 Bparms->EncryptionType);
	    ib2->XRandomPlusOne = ntohl(ib2->XRandomPlusOne);
	    say(9, RPC2_DebugLevel, "XRandomPlusOne = %ld\n", 
		ib2->XRandomPlusOne);
	    if (savexrandom+1 != ib2->XRandomPlusOne) {
		    DROPCONN();
		    RPC2_FreeBuffer(&pb);
		    rpc2_Quit(RPC2_NOTAUTHENTICATED);
	    }
	    saveyrandom = ntohl(ib2->YRandom);
	    say(9, RPC2_DebugLevel, "YRandom = %ld\n", saveyrandom);	
    }

    ce->PeerHandle = pb->Header.LocalHandle;
    say(9, RPC2_DebugLevel, "PeerHandle for local 0x%lx is 0x%lx\n", 
	*ConnHandle, ce->PeerHandle);
    RPC2_FreeBuffer(&pb);	/* Release INIT2 packet */
    if (Bparms->SecurityLevel == RPC2_OPENKIMONO) 
	    goto BindOver;	/* skip remaining phases of handshake */
    

    /* Step4: Construct Init3 packet and send it */

    RPC2_AllocBuffer(sizeof(struct Init3Body), &pb);
    rpc2_InitPacket(pb, ce, sizeof(struct Init3Body));
    pb->Header.Opcode = RPC2_INIT3;

    rpc2_htonp(pb);

    ib3 = (struct Init3Body *)pb->Body;
    ib3->YRandomPlusOne = htonl(saveyrandom+1);
    rpc2_Encrypt((char *)ib3, (char *)ib3, sizeof(struct Init3Body), (char *)Bparms->SharedSecret, Bparms->EncryptionType);	/* in-place encryption */

    /* send packet and await positive acknowledgement (i.e., RPC2_INIT4 packet) */

    say(9, RPC2_DebugLevel, "Sending INIT3 packet on 0x%lx\n", *ConnHandle);

    /* create call entry */
    sl = rpc2_AllocSle(OTHER, ce);
    rpc2_SendReliably(ce, sl, pb, (struct timeval *)&ce->Retry_Beta[0]);

    switch(sl->ReturnCode)
	{
	case ARRIVED:
		say(9, RPC2_DebugLevel, "Received INIT4 packet on 0x%lx\n", *ConnHandle);
		RPC2_FreeBuffer(&pb);	/* release the Init3 Packet */
		pb = sl->Packet;	/* and get the Init4 Packet */
		rpc2_FreeSle(&sl);
		break;
	
	case NAKED:
	case TIMEOUT:
		/* free connection, buffers, and quit */
		say(9, RPC2_DebugLevel, "Failed to send INIT3 packet on 0x%lx\n", *ConnHandle);
		RPC2_FreeBuffer(&pb);	/* release the Init3 Packet */
		rpc2_FreeSle(&sl);
		DROPCONN();
		if (peeraddrs)
		    goto try_next_addr;
		rpc2_Quit(RPC2_NOBINDING);
		
	default: assert(FALSE);
	}

    /* Step5: Verify Init4 packet; pb points to it. */
    init4rc = pb->Header.ReturnCode;	/* should be RPC2_SUCCESS */

    if (init4rc != RPC2_SUCCESS)
	{/* Authentication failure typically */
	DROPCONN();
	RPC2_FreeBuffer(&pb);
	rpc2_Quit(init4rc);	
	}

    /* We have a good INIT4 packet in pb */
    ib4 = (struct Init4Body *)pb->Body;
    rpc2_Decrypt((char *)ib4, (char *)ib4, sizeof(struct Init4Body), (char *)Bparms->SharedSecret, Bparms->EncryptionType);
    ib4->XRandomPlusTwo = ntohl(ib4->XRandomPlusTwo);
//    say(9, RPC2_DebugLevel, "XRandomPlusTwo = %l\n", ib4->XRandomPlusTwo);
    if (savexrandom+2 != ib4->XRandomPlusTwo)
    {
       DROPCONN();
       RPC2_FreeBuffer(&pb);
       rpc2_Quit(RPC2_NOTAUTHENTICATED);
    }
    ce->NextSeqNumber = ntohl(ib4->InitialSeqNumber);
    memcpy(ce->SessionKey, ib4->SessionKey, sizeof(RPC2_EncryptionKey));
    RPC2_FreeBuffer(&pb);	/* Release Init4 Packet */

    /* The security handshake is now over */

BindOver:
    /* Call side effect routine if present */
    if (HAVE_SE_FUNC(SE_Bind2)) {
	RPC2_Unsigned BindTime;

	BindTime = ce->RTT >> RPC2_RTT_SHIFT;
	if (BindTime == 0) BindTime = 1;  /* ugh. zero is overloaded. */
	if ((rc = (*ce->SEProcs->SE_Bind2)(*ConnHandle, BindTime)) != RPC2_SUCCESS)
	    {
	    DROPCONN();
	    rpc2_Quit(rc);
	    }
    }

    SetState(ce, C_THINK);

    say(9, RPC2_DebugLevel, "Bind complete for 0x%lx\n", *ConnHandle);
    rpc2_Quit(init2rc);	/* RPC2_SUCCESS or RPC2_OLDVERSION */
    /* quit */
}

long RPC2_InitSideEffect(IN RPC2_Handle ConnHandle, IN SE_Descriptor *SDesc)
{
    say(0, RPC2_DebugLevel, "RPC2_InitSideEffect()\n");

    TR_INITSE();

    rpc2_Enter();
    rpc2_Quit(InvokeSE(1, ConnHandle, SDesc, 0));
}

long RPC2_CheckSideEffect(IN RPC2_Handle ConnHandle, 
			  INOUT SE_Descriptor *SDesc, IN long Flags)
{
    say(0, RPC2_DebugLevel, "RPC2_CheckSideEffect()\n");

    TR_CHECKSE();
    
    rpc2_Enter();
    rpc2_Quit(InvokeSE(2, ConnHandle, SDesc, Flags));
}

/* CallType: 1 ==> Init, 2==> Check */
static int InvokeSE(long CallType, RPC2_Handle ConnHandle, 
		     SE_Descriptor *SDesc, long Flags)
{
    long rc;
    struct CEntry *ce;

    ce = rpc2_GetConn(ConnHandle);
    if (!ce) rpc2_Quit(RPC2_NOCONNECTION);
    if (!TestState(ce, SERVER, S_PROCESS)) return(RPC2_FAIL);
    if (ce->sebroken) return(RPC2_SEFAIL2);  

    if (!SDesc || !ce->SEProcs) return(RPC2_FAIL);

    if (CallType == 1) {
	if (!ce->SEProcs->SE_InitSideEffect) return(RPC2_FAIL);
	SetState(ce, S_INSE);
	rc = (*ce->SEProcs->SE_InitSideEffect)(ConnHandle, SDesc);
    } else {
	if (!ce->SEProcs->SE_CheckSideEffect) return(RPC2_FAIL);
	SetState(ce, S_INSE);
	rc = (*ce->SEProcs->SE_CheckSideEffect)(ConnHandle, SDesc, Flags);
    }
    if (rc < RPC2_FLIMIT) ce->sebroken = TRUE;
    SetState(ce, S_PROCESS);
    return(rc);
}

long RPC2_Unbind(RPC2_Handle whichConn)
{
	struct CEntry *ce;
	struct MEntry *me;
	
	say(0, RPC2_DebugLevel, "RPC2_Unbind()\n");
	
	TR_UNBIND();

	rpc2_Enter();
	rpc2_Unbinds++;

	ce = rpc2_GetConn(whichConn);
	if (ce == NULL) 
		rpc2_Quit(RPC2_NOCONNECTION);
	if (TestState(ce, CLIENT, ~(C_THINK|C_HARDERROR)) ||
	    TestState(ce, SERVER, ~(S_AWAITREQUEST|S_REQINQUEUE|S_PROCESS|S_HARDERROR)) ||
	    (ce->MySl != NULL && ce->MySl->ReturnCode != WAITING)) {
		rpc2_Quit(RPC2_CONNBUSY);
	}

	/* Call side effect routine and ignore return code */
	if (HAVE_SE_FUNC(SE_Unbind))
		(*ce->SEProcs->SE_Unbind)(whichConn);
	
	/* Remove ourselves from our Mgrp if we have one. */
	me = ce->Mgrp;
	if (me != NULL) 
		rpc2_RemoveFromMgrp(me, ce);

	rpc2_FreeConn(whichConn);
	rpc2_Quit(RPC2_SUCCESS);
}


time_t rpc2_time()
{
    return FT_ApproxTime();
}


void SavePacketForRetry(RPC2_PacketBuffer *pb, struct CEntry *ce)
{
    struct SL_Entry *sl;

    pb->Header.Flags = htonl((ntohl(pb->Header.Flags) | RPC2_RETRY));
    ce->HeldPacket = pb;

    if (ce->MySl) 
	say(-1, RPC2_DebugLevel, "BUG: Pending DELAYED ACK response still queued!?");

    sl = rpc2_AllocSle(REPLY, ce);
    rpc2_ActivateSle(sl, &ce->SaveResponse);
}


static int GetFilter(RPC2_RequestFilter *inf, RPC2_RequestFilter *outf)
{
	struct SubsysEntry *ss;
	struct CEntry *ce;
	long i;

	if (inf == NULL) {
		outf->FromWhom = ANY;
		outf->OldOrNew = OLDORNEW;
	}
    else
	{
	*outf = *inf;    /* structure assignment */
	}

    switch (outf->FromWhom)
	{
	case ANY:  break;

	case ONESUBSYS:
	    for (i = 0, ss = rpc2_SSList; i < rpc2_SSCount; i++, ss = ss->Next)
		if (ss->Id == outf->ConnOrSubsys.SubsysId) break;
	    if (i >= rpc2_SSCount) return(FALSE);	/* no such subsystem */
	    break;
    
	case ONECONN:
	    ce = rpc2_GetConn(outf->ConnOrSubsys.WhichConn);
	    if (ce == NULL) return(FALSE);
	    if (!TestState(ce, SERVER, S_AWAITREQUEST | S_REQINQUEUE)) 
		    return(FALSE);
	    break;
	}
	
    return(TRUE);
    }


static RPC2_PacketBuffer *HeldReq(RPC2_RequestFilter *filter, struct CEntry **ce)
{
	RPC2_PacketBuffer *pb;
	long i;

	do {
		say(9, RPC2_DebugLevel, "Scanning hold queue\n");
		pb = rpc2_PBHoldList;
		for (i = 0; i < rpc2_PBHoldCount; i++) {
			if (rpc2_FilterMatch(filter, pb)) 
				break;
			else 
				pb = (RPC2_PacketBuffer *)pb->Prefix.Next;
		}
		if (i >= rpc2_PBHoldCount) 
			return(NULL);

		rpc2_UnholdPacket(pb);

		*ce = rpc2_GetConn(pb->Header.RemoteHandle);
		/* conn nuked; throw away and rescan */
		if (*ce == NULL) {
			say(9, RPC2_DebugLevel, "Conn missing, punting request\n");
			RPC2_FreeBuffer(&pb);
		}
	}
	while (!(*ce));

	return(pb);
}


static long GetNewRequest(IN RPC2_RequestFilter *filter, IN struct timeval *timeout, OUT struct RPC2_PacketBuffer **pb, OUT struct CEntry **ce)
{
    struct SL_Entry *sl;

    say(9, RPC2_DebugLevel, "GetNewRequest()\n");

TryAnother:
    sl = rpc2_AllocSle(REQ, NULL);
    sl->Filter = *filter;	/* structure assignment */
    rpc2_ActivateSle(sl, timeout);

    LWP_WaitProcess((char *)sl);

    /* SocketListener will wake us up */

    switch(sl->ReturnCode)
	{
	case TIMEOUT:	/* timeout */
		    say(9, RPC2_DebugLevel, "Request wait timed out\n");
		    rpc2_FreeSle(&sl);
		    return(RPC2_TIMEOUT);
		    
	case ARRIVED:	/* a request that matches my filter was received */
		    say(9, RPC2_DebugLevel, "Request wait succeeded\n");
		    *pb = sl->Packet;	/* save a pointer to the buffer */
		    rpc2_FreeSle(&sl);

		    *ce = rpc2_GetConn((*pb)->Header.RemoteHandle);
		    if (*ce == NULL)
			{/* a connection was nuked by someone while we slept */
			say(9, RPC2_DebugLevel, "Conn gone, punting packet\n");
			RPC2_FreeBuffer(pb);
			goto TryAnother;
			}
		    return(RPC2_SUCCESS);

	default:  assert(FALSE);
	}
    return RPC2_FAIL;
    /*NOTREACHED*/
    }


static long MakeFake(INOUT pb, IN ce, OUT xrand, OUT authenticationtype, OUT cident)
    RPC2_PacketBuffer *pb;
    struct CEntry *ce;
    long *xrand;
    RPC2_Integer *authenticationtype;
    RPC2_CountedBS *cident;
    {
    /* Synthesize fake packet after extracting encrypted XRandom and clientident */
    long i;
    struct Init1Body *ib1;
    RPC2_NewConnectionBody *ncb;

    ib1 = (struct Init1Body *)(pb->Body);
    ncb = &ib1->FakeBody;

    if (strcmp(ib1->Version, RPC2_VERSION) != 0)
	{
	say(9, RPC2_DebugLevel, "Old Version  Mine: \"%s\"  His: \"%s\"\n",
				 RPC2_VERSION, (char *)ib1->Version);
	ce->Flags |= CE_OLDV;
	}

    *xrand  = ib1->XRandom;		/* Still encrypted */
    *authenticationtype = ntohl(ncb->AuthenticationType);
    cident->SeqLen = ntohl(ncb->ClientIdent.SeqLen);
    cident->SeqBody = (RPC2_ByteSeq) &ncb->ClientIdent.SeqBody;

    /* For RP2Gen or other stub generators:
	(1) leave FakeBody fields in network order
	(2) copy text of client ident to SeqBody
    */
    memcpy(cident->SeqBody, ib1->Text, cident->SeqLen);

    /* Obtain pointer to appropriate set of SE routines */
    ce->SEProcs = NULL;
    if (ntohl(ncb->SideEffectType))
	{
	for (i = 0; i < SE_DefCount; i++)
	    if (SE_DefSpecs[i].SideEffectType == ntohl(ncb->SideEffectType)) break;
	if (i >= SE_DefCount) return(RPC2_SEFAIL2);
	ce->SEProcs = &SE_DefSpecs[i];
	}

    pb->Header.Opcode = RPC2_NEWCONNECTION;
    return(RPC2_SUCCESS);
    }


static void SendOKInit2(IN struct CEntry *ce)
    {
    RPC2_PacketBuffer *pb;

    say(9, RPC2_DebugLevel, "SendOKInit2()\n");

    RPC2_AllocBuffer(sizeof(struct Init2Body), &pb);
    rpc2_InitPacket(pb, ce, sizeof(struct Init2Body));
    pb->Header.Opcode = RPC2_INIT2;
    if (ce->Flags & CE_OLDV) pb->Header.ReturnCode = RPC2_OLDVERSION;
    else pb->Header.ReturnCode = RPC2_SUCCESS;

    if (ce->TimeStampEcho)     /* service time is now-requesttime */
	rpc2_StampPacket(ce, pb);

    rpc2_htonp(pb);	/* convert to network order */
    rpc2_XmitPacket(rpc2_RequestSocket, pb, ce->HostInfo->Addr, 1);
    SavePacketForRetry(pb, ce);
    }

static int ServerHandShake(IN struct CEntry *ce,
			   IN RPC2_Integer authenticationtype,
			   IN RPC2_CountedBS *cident,
			   IN long xrand /* still encrypted */,
			   IN RPC2_GetKeys_func *KeyProc, IN long emask)
    {
    RPC2_EncryptionKey SharedSecret;
    RPC2_PacketBuffer *pb;
    long saveYRandom, rc;

    /* Abort if we cannot get keys or if bogus encryption type specified */
    if (!KeyProc ||
	KeyProc(&authenticationtype, cident, SharedSecret, ce->SessionKey) ||
	(ce->EncryptionType & emask) == 0 ||	/* unsupported or unknown encryption type */
	MORETHANONEBITSET(ce->EncryptionType))
	{
	RejectBind(ce, (long) sizeof(struct Init2Body), (long) RPC2_INIT2);
	return(RPC2_NOTAUTHENTICATED);
	}

    rpc2_Decrypt((char *)&xrand, (char *)&xrand, sizeof(xrand), SharedSecret, ce->EncryptionType);
    xrand = ntohl(xrand);

    /* Send Init2 packet and await Init3 */
    pb = Send2Get3(ce, SharedSecret, xrand, &saveYRandom);
    if (!pb) return(RPC2_NOTAUTHENTICATED);

    /* Validate Init3 */
    rc = Test3(pb, ce, saveYRandom, SharedSecret);
    RPC2_FreeBuffer(&pb);
    if (rc == RPC2_NOTAUTHENTICATED)
	{
	RejectBind(ce, (long) sizeof(struct Init4Body), (long) RPC2_INIT4);
	return(RPC2_NOTAUTHENTICATED);
	}

    /* Send Init4 */
    Send4AndSave(ce, xrand, SharedSecret);
    return(RPC2_SUCCESS);
    }


static void RejectBind(ce, bodysize, opcode)
    struct CEntry *ce;
    long bodysize, opcode;
    {
    RPC2_PacketBuffer *pb;

    say(9, RPC2_DebugLevel, "Rejecting  bind request\n");

    RPC2_AllocBuffer(bodysize, &pb);
    rpc2_InitPacket(pb, ce, bodysize);
    pb->Header.Opcode = opcode;
    pb->Header.ReturnCode = RPC2_NOTAUTHENTICATED;

    rpc2_htonp(pb);
    rpc2_XmitPacket(rpc2_RequestSocket, pb, ce->HostInfo->Addr, 1);
    RPC2_FreeBuffer(&pb);
    }

static RPC2_PacketBuffer *Send2Get3(IN ce, IN key, IN xrand, OUT yrand)
    struct CEntry *ce;
    RPC2_EncryptionKey key;
    long xrand;
    long *yrand;
    {
    RPC2_PacketBuffer *pb2, *pb3;
    struct Init2Body *ib2;
    struct SL_Entry *sl;

    /* Allocate Init2 packet */
    RPC2_AllocBuffer(sizeof(struct Init2Body), &pb2);
    ib2 = (struct Init2Body *)pb2->Body;
    rpc2_InitPacket(pb2, ce, sizeof(struct Init2Body));
    pb2->Header.Opcode = RPC2_INIT2;
    if (ce->Flags & CE_OLDV) pb2->Header.ReturnCode = RPC2_OLDVERSION;
    else pb2->Header.ReturnCode = RPC2_SUCCESS;

    /* Do xrand, yrand munging */
    say(9, RPC2_DebugLevel, "XRandom = %ld\n", xrand);
    ib2->XRandomPlusOne = htonl(xrand+1);
    *yrand = rpc2_NextRandom(NULL);
    ib2->YRandom = htonl(*yrand);
    say(9, RPC2_DebugLevel, "YRandom = %ld\n", *yrand);
    rpc2_Encrypt((char *)ib2, (char *)ib2, sizeof(struct Init2Body), key, ce->EncryptionType);

    if (ce->TimeStampEcho)     /* service time is now-requesttime */
	rpc2_StampPacket(ce, pb2);

    rpc2_htonp(pb2);

    /* Send Init2 packet and await Init3 packet */
    SetState(ce, S_AWAITINIT3);
    sl = rpc2_AllocSle(OTHER, ce);
    rpc2_SendReliably(ce, sl, pb2, (struct timeval *)&ce->Retry_Beta[0]);

    switch(sl->ReturnCode)
	{
	case ARRIVED:
		pb3 = sl->Packet;	/* get the Init3 Packet */
		if (pb3->Header.BodyLength != sizeof(struct Init3Body))
		    {
		    say(9, RPC2_DebugLevel, "Runt Init3 packet\n");
		    RPC2_FreeBuffer(&pb3);	/* will set to NULL */
		    }
		break;

	case NAKED:
	case TIMEOUT:
		/* free connection, buffers, and quit */
		say(9, RPC2_DebugLevel, "Failed to send INIT2\n");
		pb3 = NULL;
		break;
		
	default:    assert(FALSE);
	}

    /* Clean up and quit */
    rpc2_FreeSle(&sl);
    RPC2_FreeBuffer(&pb2);	/* release the Init2 Packet */
    return(pb3);    
    }


static long Test3(RPC2_PacketBuffer *pb, struct CEntry *ce, long yrand, RPC2_EncryptionKey ekey)
{
    struct Init3Body *ib3;

    ib3 = (struct Init3Body *)pb->Body;
    rpc2_Decrypt((char *)ib3, (char *)ib3, sizeof(struct Init3Body), ekey, ce->EncryptionType);
    ib3->YRandomPlusOne = ntohl(ib3->YRandomPlusOne);
    say(9, RPC2_DebugLevel, "YRandomPlusOne = %ld\n", ib3->YRandomPlusOne);
    if (ib3->YRandomPlusOne == yrand+1) return(RPC2_SUCCESS);
    else return(RPC2_NOTAUTHENTICATED);	
    }

static void Send4AndSave(ce, xrand, ekey)
    struct CEntry *ce;
    int xrand;
    RPC2_EncryptionKey ekey;
    {
    RPC2_PacketBuffer *pb;
    struct Init4Body *ib4;

    say(9, RPC2_DebugLevel, "Send4AndSave()\n");

    RPC2_AllocBuffer(sizeof(struct Init4Body), &pb);
    rpc2_InitPacket(pb, ce, sizeof(struct Init4Body));
    pb->Header.Opcode = RPC2_INIT4;
    pb->Header.ReturnCode = RPC2_SUCCESS;
    ib4 = (struct Init4Body *)pb->Body;
    memcpy(ib4->SessionKey, ce->SessionKey, sizeof(RPC2_EncryptionKey));
    ib4->InitialSeqNumber = htonl(ce->NextSeqNumber);
    ib4->XRandomPlusTwo = htonl(xrand + 2);
    rpc2_Encrypt((char *)ib4, (char *)ib4, sizeof(struct Init4Body), ekey, ce->EncryptionType);

    if (ce->TimeStampEcho)     /* service time is now-requesttime */
	rpc2_StampPacket(ce, pb);

    rpc2_htonp(pb);

    /* Send packet; don't bother waiting for acknowledgement */
    rpc2_XmitPacket(rpc2_RequestSocket, pb, ce->HostInfo->Addr, 1);

    SavePacketForRetry(pb, ce);
    }
