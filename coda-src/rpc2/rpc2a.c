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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rpc2/rpc2a.c,v 4.1 1997/01/08 21:50:27 rvb Exp $";
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
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include "lwp.h"
#include "timer.h"
#include "preempt.h"
#include "rpc2.h"
#include "rpc2.private.h"
#include "se.h"
#include "trace.h"
#include "cbuf.h"
#include "multi.h"



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
	1. State:	Modified in GetRequest, SendResponse, MakeRPC,
			    MultiRPC, Bind and SocketListener.
			Always C_THINK in client code.
			In S_AWAITREQUEST when not servicing a request, in server code.
			In S_PROCESS between a GetRequest and a SendResponse in server code.
			Other values used only during bind sequence.
			Set to S_HARDERROR or C_HARDERROR on a permanent error.

	2. NextSeqNumber:
			Initialized by connection creation code in GetRequest.
			ALWAYS  updated by SocketListener, except in the RPC2_MultiRPC case.
			Updated in RPC2_MultiRPC() if SocketListener return code is WAITING.
			On client side, in state C_THINK, this value is the next outgoing
			    request's sequence number.
			On server side, in state S_AWAITREQUEST, this value is the
			    next incoming request's sequence number.

NOTE 1
======
    The code  works with the LWP preemption package.  All user-callable
    RPC2 routines are in critical sections.  The independent LWPs such as 
    SocketListener, side-effect LWPs, etc. are totally non-preemptible, since they do not
    do a PRE_PreemptMe() call.  The only lower-level RPC routines that have to be 
    explicitly bracketed by critical sections are the randomize and encryption routines which
    are useful independent of RPC2.
*/


void SavePacketForRetry();
PRIVATE int InvokeSE(), ResolveBindParms(), ServerHandShake();
PRIVATE void SendOKInit2(), RejectBind(), Send4AndSave();
PRIVATE long cmpaddr(), GetFilter(), GetNewRequest(), MakeFake(), Test3();
PRIVATE RPC2_PacketBuffer *HeldReq(), *Send2Get3();

extern struct timeval SaveResponse;

long RPC2_SendResponse(IN ConnHandle, IN Reply)
    RPC2_Handle ConnHandle;
    register RPC2_PacketBuffer *Reply;
    {
    RPC2_PacketBuffer *preply, *pretry;
    register struct CEntry *ceaddr;
    long rc;

    rpc2_Enter();
    say(0, RPC2_DebugLevel, ("RPC2_SendResponse()\n"));
    assert(Reply->Prefix.MagicNumber == OBJ_PACKETBUFFER);

#ifdef RPC2DEBUG
    TR_SENDRESPONSE()
#endif RPC2DEBUG

	    
    /* Perform sanity checks */
    ceaddr = rpc2_GetConn(ConnHandle);
    if (ceaddr == NULL) rpc2_Quit(RPC2_NOCONNECTION);
    if (!TestState(ceaddr, SERVER, S_PROCESS)) 	rpc2_Quit(RPC2_NOTWORKER);

    preply = Reply;	/* side effect routine usually does not reallocate packet */
			/* preply will be the packet actually sent over the wire */


    
    rc = preply->Header.ReturnCode; /* InitPacket clobbers it */
    rpc2_InitPacket(preply, ceaddr, preply->Header.BodyLength);
    preply->Header.ReturnCode = rc;
    preply->Header.Opcode = RPC2_REPLY;
    preply->Header.SeqNumber = ceaddr->NextSeqNumber-1;
    			/* SocketListener has already updated NextSeqNumber */

    rc = RPC2_SUCCESS;	/* tentative, for sendresponse */
    /* Notify side effect routine, if any */
    if (ceaddr->SEProcs != NULL && ceaddr->SEProcs->SE_SendResponse != NULL)
    	{
	rc = (*ceaddr->SEProcs->SE_SendResponse)(ConnHandle, &preply);
	}

    /* set connection state */
    SetState(ceaddr, S_AWAITREQUEST);
    if (ceaddr->Mgrp != NULL) SetState(ceaddr->Mgrp, S_AWAITREQUEST);

    /* Allocate retry packet before encrypting Bodylength */ 
    RPC2_AllocBuffer(preply->Header.BodyLength, &pretry); 

    if (ceaddr->TimeStampEcho) {     /* service time is now-requesttime */
	assert(ceaddr->RequestTime);
        preply->Header.TimeStamp = ceaddr->TimeStampEcho + rpc2_MakeTimeStamp() -
	                           ceaddr->RequestTime;
    }

    /* Sanitize packet */
    rpc2_htonp(preply);
    rpc2_ApplyE(preply, ceaddr);

    /* Send reply */
    say(9, RPC2_DebugLevel, ("Sending reply\n"));
    rpc2_XmitPacket(rpc2_RequestSocket, preply, &ceaddr->PeerHost, &ceaddr->PeerPortal);

    /* Save reply for retransmission */
    bcopy(&preply->Header, &pretry->Header, preply->Prefix.LengthOfPacket);
    pretry->Prefix.LengthOfPacket = preply->Prefix.LengthOfPacket;
    SavePacketForRetry(pretry, ceaddr);
    
    if (preply != Reply) RPC2_FreeBuffer(&preply);  /* allocated by SE routine */
    rpc2_Quit(rc);
    }

long RPC2_GetRequest(IN Filter,  OUT ConnHandle, OUT Request, IN BreathOfLife,
	    IN GetKeys, IN EncryptionTypeMask, IN AuthFail)
    RPC2_RequestFilter *Filter;
    RPC2_Handle *ConnHandle;
    register RPC2_PacketBuffer **Request;
    struct timeval *BreathOfLife;
    long (*GetKeys)();
    long EncryptionTypeMask;
    long (*AuthFail)();
    {
    struct CEntry *ce;
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer *pb;
    RPC2_CountedBS cident;
    long rc, saveXRandom;

    rpc2_Enter();
    say(0, RPC2_DebugLevel, ("RPC2_GetRequest()\n"));
	    
#ifdef RPC2DEBUG
    TR_GETREQUEST();
#endif RPC2DEBUG

#define DROPIT()\
	/* worthless request */\
	RPC2_FreeBuffer(Request);\
	(void) RPC2_Unbind(*ConnHandle);\
	goto ScanWorkList;\


    if (!GetFilter(Filter, &myfilter)) rpc2_Quit(RPC2_BADFILTER);

ScanWorkList:
    pb = HeldReq(&myfilter, &ce);
    if (!pb)
	{
	/* await a proper request */
	rc = GetNewRequest(&myfilter, BreathOfLife, &pb, &ce);
	if (rc != RPC2_SUCCESS) rpc2_Quit(rc);
	}

    if (!TestState(ce, SERVER, S_STARTBIND))
	{
	SetState(ce, S_PROCESS);
	if (IsMulticast(pb))
	    {
	    assert(ce->Mgrp != NULL);
	    SetState(ce->Mgrp, S_PROCESS);
	    }
	}

    /* Invariants here:
    	(1) pb points to a request packet, decrypted and nettohosted
	(2) ce is the connection associated with pb
	(3) ce's state is S_STARTBIND if this is a new bind, S_PROCESS otherwise
    */

    *Request = pb;
    *ConnHandle = ce->UniqueCID;

    if (!TestState(ce, SERVER, S_STARTBIND))
	{/* not a bind request */
	say(9, RPC2_DebugLevel, ("Request on existing connection\n"));

	rc = RPC2_SUCCESS;
	
	/* Notify side effect routine, if any */
	if (ce->SEProcs && ce->SEProcs->SE_GetRequest)
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

    rc = MakeFake(pb, ce, &saveXRandom, &cident);
    if (rc < RPC2_WLIMIT) {DROPIT();}

    /* Do rest of bind protocol */
    if (ce->SecurityLevel == RPC2_OPENKIMONO)
	{
	SendOKInit2(ce);
	}
    else
	{
	rc = ServerHandShake(ce, &cident, saveXRandom, GetKeys, EncryptionTypeMask);
	if (rc!= RPC2_SUCCESS)
	    {
	    if (AuthFail)
		{/* Client could be iterating through keys; log this */
		(*AuthFail)(&cident, ce->EncryptionType, &ce->PeerHost, &ce->PeerPortal);
		}
	    DROPIT();
	    }
	}


    /* Do final processing */
    SetState(ce, S_AWAITENABLE); /* all we need is RPC2_Enable() */

    /* Call side effect routine if present */
    if (ce->SEProcs && ce->SEProcs->SE_NewConnection)
    	{
	rc = (*ce->SEProcs->SE_NewConnection)(*ConnHandle, &cident);
	if (rc < RPC2_FLIMIT) {DROPIT();}
	}

    /* Set up host linkage -- host & portal numbers are resolved by now. */
    ce->HostInfo = rpc2_GetHost(&ce->PeerHost, &ce->PeerPortal);
    if (ce->HostInfo == NULL) 
	ce->HostInfo = rpc2_AllocHost(&ce->PeerHost, &ce->PeerPortal, RPC2_HE);
	
    /* And now we are really done */
    if (ce->Flags & CE_OLDV) rpc2_Quit(RPC2_OLDVERSION);
    else rpc2_Quit(RPC2_SUCCESS);

#undef DROPIT
    }

long RPC2_MakeRPC(IN ConnHandle, IN Request, IN SDesc, OUT Reply,
		IN BreathOfLife, IN EnqueueRequest)
    RPC2_Handle ConnHandle;
    register RPC2_PacketBuffer *Request;	/* Gets clobbered during call: BEWARE */
    SE_Descriptor *SDesc;
    RPC2_PacketBuffer **Reply;
    struct timeval *BreathOfLife;
    long EnqueueRequest;
    {
    register struct CEntry *ce;
    struct SL_Entry *sl;
    register RPC2_PacketBuffer *preply;
    RPC2_PacketBuffer *preq;	/* no register because of & */
    long rc, secode, finalrc, opcode;

    rpc2_Enter();
    say(0, RPC2_DebugLevel, ("RPC2_MakeRPC()\n"));
	    
#ifdef RPC2DEBUG
    TR_MAKERPC();
#endif RPC2DEBUG

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
	if (ce == NULL) rpc2_Quit(RPC2_NOCONNECTION);
	if (TestState(ce, CLIENT, C_HARDERROR)) rpc2_Quit(RPC2_FAIL);
	if (TestState(ce, CLIENT, C_THINK)) break;
	if (SDesc != NULL && ce->sebroken) rpc2_Quit(RPC2_SEFAIL2);

	if (!EnqueueRequest) rpc2_Quit(RPC2_CONNBUSY);
	say(0, RPC2_DebugLevel, ("Enqueuing on connection 0x%lx\n",ConnHandle));
	LWP_WaitProcess((char *)ce);
	say(0, RPC2_DebugLevel, ("Dequeueing on connection 0x%lx\n", ConnHandle));
	}
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
    if (SDesc != NULL && ce->SEProcs != NULL && ce->SEProcs->SE_MakeRPC1 != NULL)
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

    say(9, RPC2_DebugLevel, ("Sending request on  0x%lx\n", ConnHandle));
    /* create call entry */
    sl = rpc2_AllocSle(OTHER, ce);
    rc = rpc2_SendReliably(ce, sl, preq, BreathOfLife);
    
    switch((int) rc)
	{
	case RPC2_SUCCESS:	break;

	case RPC2_TIMEOUT:	/* don't destroy the connection */
				say(9, RPC2_DebugLevel,
				    ("rpc2_SendReliably()--> %s on 0x%lx\n",
				    RPC2_ErrorMsg(rc), ConnHandle));
				rpc2_FreeSle(&sl);
				/* release packet allocated by SE routine */
				if (preq != Request) RPC2_FreeBuffer(&preq);
				finalrc = RPC2_TIMEOUT;
				goto QuitMRPC;

	default:		assert(FALSE);
	}

    
    switch(sl->ReturnCode)
	{
	case ARRIVED:
		say(9, RPC2_DebugLevel, 
			("Request reliably sent on 0x%lx\n", ConnHandle));
		*Reply = preply = sl->Packet;
		rpc2_FreeSle(&sl);
		/* release packet allocated by SE routine */
		if (preq != Request) RPC2_FreeBuffer(&preq);
		rc = RPC2_SUCCESS;
		break;
	
	case TIMEOUT:
		say(9, RPC2_DebugLevel,	("Request failed on 0x%lx\n", ConnHandle));
		rpc2_FreeSle(&sl);
		rpc2_SetConnError(ce);	/* does signal on ConnHandle also */
		/* release packet allocated by SE routine */
		if (preq != Request) RPC2_FreeBuffer(&preq);
		rc = RPC2_DEAD;
		break;

	case NAKED:
		say(9, RPC2_DebugLevel,
		    ("Request NAK'ed on 0x%lx\n", ConnHandle));
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
    if (SDesc != NULL && ce->SEProcs != NULL && ce->SEProcs->SE_MakeRPC2 != NULL)
	{
	secode = (*ce->SEProcs->SE_MakeRPC2)(ConnHandle, SDesc, (rc == RPC2_SUCCESS)? preply : NULL);

	if (secode < RPC2_FLIMIT)
	    {
	    ce->sebroken = TRUE;
	    finalrc = secode;
	    goto QuitMRPC;
	    }
	}

    LWP_NoYieldSignal((char *)ce);	/* return code may be LWP_SUCCESS or LWP_ENOWAIT */

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
    
QuitMRPC:	/* finalrc has been correctly set by now */
    rpc2_Quit(finalrc);
    }
    

    
    
long RPC2_NewBinding(IN Host, IN Portal, IN Subsys, IN Bparms, OUT ConnHandle)
    RPC2_HostIdent   *Host;
    RPC2_PortalIdent *Portal;
    RPC2_SubsysIdent *Subsys;
    RPC2_BindParms *Bparms;
    RPC2_Handle *ConnHandle;
    {
    register struct CEntry *ce;	/* equal to *ConnHandle after allocation */
    RPC2_PacketBuffer *pb;	/* cannot be register: & is used often */
    register long i;
    register struct Init1Body *ib;
    struct Init2Body *ib2;
    struct Init3Body *ib3;
    struct Init4Body *ib4;    
    struct SL_Entry *sl;
    long rc, init2rc, init4rc, savexrandom, saveyrandom, bsize;

#define DROPCONN()\
	    {rpc2_SetConnError(ce); (void) RPC2_Unbind(*ConnHandle); *ConnHandle = 0;}

    rpc2_Enter();
    say(0, RPC2_DebugLevel, ("In RPC2_NewBinding()\n"));

#ifdef RPC2DEBUG
    TR_BIND();
#endif RPC2DEBUG

    switch ((int) Bparms->SecurityLevel)
	{
	case RPC2_OPENKIMONO:
		break;


	case RPC2_AUTHONLY:
	case RPC2_HEADERSONLY:
	case RPC2_SECURE:
		if ((Bparms->EncryptionType & RPC2_ENCRYPTIONTYPES) == 0) rpc2_Quit (RPC2_FAIL); 	/* unknown encryption type */
		if (MORETHANONEBITSET(Bparms->EncryptionType)) rpc2_Quit(RPC2_FAIL);	/* tell me just one */
		break;
		
	default:	rpc2_Quit(RPC2_FAIL);	/* bogus security level */
	}
	

    /* Step 0: Obtain and initialize a new connection */

    ce = rpc2_AllocConn();	/* Pointer fields set to null */
    *ConnHandle = ce->UniqueCID;
    say(9, RPC2_DebugLevel, ("Allocating connection 0x%lx\n", *ConnHandle));
    SetRole(ce, CLIENT);
    SetState(ce, C_AWAITINIT2);	
    ce->PeerUnique = rpc2_NextRandom(NULL);

    switch((int) Bparms->SecurityLevel)
        {
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
		break;	/* NextSeqNumber will be filled in in the last step of the handshake */
	}

    /* Obtain pointer to appropriate set of side effect routines */
    if (Bparms->SideEffectType != NULL)
	{
	for (i = 0; i < SE_DefCount; i++)
	    if (SE_DefSpecs[i].SideEffectType == Bparms->SideEffectType) break;
	if (i >= SE_DefCount)
	    {
	    DROPCONN();
	    rpc2_Quit (RPC2_FAIL);	/* bogus side effect */
	    }
	ce->SEProcs = &SE_DefSpecs[i];
	}
    else ce->SEProcs = NULL;

    /* Call side effect routine if present */
    if (ce->SEProcs != NULL && ce->SEProcs->SE_Bind1 != NULL)
	if ((rc = (*ce->SEProcs->SE_Bind1)(*ConnHandle, Bparms->ClientIdent)) != RPC2_SUCCESS)
	    {
	    DROPCONN();
	    rpc2_Quit(rc);
	    }

    /* Step1: Resolve bind parameters */

    if (ResolveBindParms(ce, Host, Portal, Subsys) != RPC2_SUCCESS)
	{
	DROPCONN();
	rpc2_Quit(RPC2_NOBINDING);
	}
    say(9, RPC2_DebugLevel, ("Bind parameters successfully resolved\n"));

    /* Set up host linkage -- host & portal numbers are resolved by now. */
    ce->HostInfo = rpc2_GetHost(&ce->PeerHost, &ce->PeerPortal);
    if (ce->HostInfo == NULL) 
	ce->HostInfo = rpc2_AllocHost(&ce->PeerHost, &ce->PeerPortal, RPC2_HE);
	
    /* Step2: Construct Init1 packet */

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
    ib->SenderPortal = rpc2_LocalPortal;	/* structure assignment */
    ib->SenderPortal.Tag = (PortalTag)htonl(ib->SenderPortal.Tag);	/* always in network order */
    if(rpc2_GetLocalHost(&ib->SenderHost, &ce->PeerHost) == -1)
	    ib->SenderHost = rpc2_LocalHost;	/* structure assignment */
    ib->SenderHost.Tag = (HostTag)htonl(ib->SenderHost.Tag);	/* always in network order */
    ib->FakeBody.SideEffectType = htonl(Bparms->SideEffectType);
    ib->FakeBody.SecurityLevel = htonl(Bparms->SecurityLevel);
    ib->FakeBody.EncryptionType = htonl(Bparms->EncryptionType);
    if (Bparms->ClientIdent == NULL)
	ib->FakeBody.ClientIdent.SeqLen = 0;
    else
	{
	ib->FakeBody.ClientIdent.SeqLen = htonl(Bparms->ClientIdent->SeqLen);
	ib->FakeBody.ClientIdent.SeqBody = ib->Text;    /* not really meaningful: this is pointer has to be reset on other side */
	bcopy(Bparms->ClientIdent->SeqBody, ib->Text, Bparms->ClientIdent->SeqLen);
	}
    assert(sizeof(RPC2_VERSION) < sizeof(ib->Version));
    (void) strcpy(ib->Version, RPC2_VERSION);

    rpc2_htonp(pb);	/* convert header to network order */

    if (Bparms->SecurityLevel != RPC2_OPENKIMONO)
	{
	savexrandom = rpc2_NextRandom(NULL);
	say(9, RPC2_DebugLevel, ("XRandom = %ld\n",savexrandom));
	ib->XRandom = htonl(savexrandom);

	rpc2_Encrypt((char *)&ib->XRandom, (char *)&ib->XRandom, sizeof(ib->XRandom),
		(char *)Bparms->SharedSecret, Bparms->EncryptionType);	/* in-place encryption */
	}

    /* Step3: Send INIT1 packet  and wait for reply */

    /* send packet and await positive acknowledgement (i.e., RPC2_INIT2 packet) */

    say(9, RPC2_DebugLevel, ("Sending INIT1 packet on 0x%lx\n", *ConnHandle));
    /* create call entry */
    sl = rpc2_AllocSle(OTHER, ce);
    rpc2_SendReliably(ce, sl, pb, (struct timeval *)NULL);

    switch(sl->ReturnCode)
	{
	case ARRIVED:
		say(9, RPC2_DebugLevel, ("Received INIT2 packet on 0x%lx\n", *ConnHandle));
		RPC2_FreeBuffer(&pb);	/* release the Init1 Packet */
		pb = sl->Packet;		/* and get the Init2 Packet */
		rpc2_FreeSle(&sl);
		break;
	
	case NAKED:
	case TIMEOUT:
		/* free connection, buffers, and quit */
		say(9, RPC2_DebugLevel, ("Failed to send INIT1 packet on 0x%lx\n", *ConnHandle));
		RPC2_FreeBuffer(&pb);	/* release the Init1 Packet */
		rc = sl->ReturnCode == NAKED ? RPC2_NAKED : RPC2_NOBINDING;
		rpc2_FreeSle(&sl);
		DROPCONN();
		rpc2_Quit(rc);
		
	default:	assert(FALSE);
	}

    /* At this point, pb points to the Init2 packet */

    /* Step3: Examine Init2 packet, get bind info (at least PeerHandle) and continue with handshake sequence */
    init2rc = pb->Header.ReturnCode;	/* is usually RPC2_SUCCESS or RPC2_OLDVERSION */

    if (init2rc  < RPC2_ELIMIT)
	{/* Authentication failure typically */
	DROPCONN();
	RPC2_FreeBuffer(&pb);
	rpc2_Quit(init2rc);	
	}

    /* We have a good INIT2 packet in pb */
    if (Bparms->SecurityLevel != RPC2_OPENKIMONO)
	{
	ib2 = (struct Init2Body *)pb->Body;
	rpc2_Decrypt((char *)ib2, (char *)ib2, sizeof(struct Init2Body), (char *)Bparms->SharedSecret,
		Bparms->EncryptionType);
	ib2->XRandomPlusOne = ntohl(ib2->XRandomPlusOne);
	say(9, RPC2_DebugLevel, ("XRandomPlusOne = %ld\n", ib2->XRandomPlusOne));
	if (savexrandom+1 != ib2->XRandomPlusOne)
	    {
	    DROPCONN();
	    RPC2_FreeBuffer(&pb);
	    rpc2_Quit(RPC2_NOTAUTHENTICATED);
	    }
	saveyrandom = ntohl(ib2->YRandom);
	say(9, RPC2_DebugLevel, ("YRandom = %ld\n", saveyrandom));	
	}

    ce->PeerHandle = pb->Header.LocalHandle;
    say(9, RPC2_DebugLevel, ("PeerHandle for local 0x%lx is 0x%lx\n", *ConnHandle, ce->PeerHandle));
    RPC2_FreeBuffer(&pb);	/* Release INIT2 packet */
    if (Bparms->SecurityLevel == RPC2_OPENKIMONO) goto BindOver;	/* skip remaining phases of handshake */
    

    /* Step4: Construct Init3 packet and send it */

    RPC2_AllocBuffer(sizeof(struct Init3Body), &pb);
    rpc2_InitPacket(pb, ce, sizeof(struct Init3Body));
    pb->Header.Opcode = RPC2_INIT3;

    rpc2_htonp(pb);

    ib3 = (struct Init3Body *)pb->Body;
    ib3->YRandomPlusOne = htonl(saveyrandom+1);
    rpc2_Encrypt((char *)ib3, (char *)ib3, sizeof(struct Init3Body), (char *)Bparms->SharedSecret, Bparms->EncryptionType);	/* in-place encryption */

    /* send packet and await positive acknowledgement (i.e., RPC2_INIT4 packet) */

    say(9, RPC2_DebugLevel, ("Sending INIT3 packet on 0x%lx\n", *ConnHandle));

    /* create call entry */
    sl = rpc2_AllocSle(OTHER, ce);
    rpc2_SendReliably(ce, sl, pb, (struct timeval *)NULL);

    switch(sl->ReturnCode)
	{
	case ARRIVED:
		say(9, RPC2_DebugLevel, ("Received INIT4 packet on 0x%lx\n", *ConnHandle));
		RPC2_FreeBuffer(&pb);	/* release the Init3 Packet */
		pb = sl->Packet;	/* and get the Init4 Packet */
		rpc2_FreeSle(&sl);
		break;
	
	case NAKED:
	case TIMEOUT:
		/* free connection, buffers, and quit */
		say(9, RPC2_DebugLevel, ("Failed to send INIT3 packet on 0x%lx\n", *ConnHandle));
		RPC2_FreeBuffer(&pb);	/* release the Init3 Packet */
		rpc2_FreeSle(&sl);
		DROPCONN();
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
    ce->NextSeqNumber = ntohl(ib4->InitialSeqNumber);
    bcopy(ib4->SessionKey, ce->SessionKey, sizeof(RPC2_EncryptionKey));
    RPC2_FreeBuffer(&pb);	/* Release Init4 Packet */

    /* The security handshake is now over */

BindOver:
    /* Call side effect routine if present */
    if (ce->SEProcs != NULL && ce->SEProcs->SE_Bind2 != NULL) {
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

    say(9, RPC2_DebugLevel, ("Bind complete for 0x%lx\n", *ConnHandle));
    rpc2_Quit(init2rc);	/* RPC2_SUCCESS or RPC2_OLDVERSION */
    /* quit */
    }

long RPC2_InitSideEffect(IN ConnHandle, IN SDesc)
    RPC2_Handle ConnHandle;
    SE_Descriptor *SDesc;
    {
    say(0, RPC2_DebugLevel, ("RPC2_InitSideEffect()\n"));

#ifdef RPC2DEBUG
    TR_INITSE();
#endif RPC2DEBUG

    rpc2_Enter();
    rpc2_Quit(InvokeSE(1, ConnHandle, SDesc, 0));
    }

long RPC2_CheckSideEffect(IN ConnHandle, INOUT SDesc, IN Flags)
    RPC2_Handle ConnHandle;
    SE_Descriptor *SDesc;
    long Flags;    
    {
    say(0, RPC2_DebugLevel, ("RPC2_CheckSideEffect()\n"));

#ifdef RPC2DEBUG
    TR_CHECKSE();
#endif RPC2DEBUG
    
    rpc2_Enter();
    rpc2_Quit(InvokeSE(2, ConnHandle, SDesc, Flags));
    }


PRIVATE int InvokeSE(CallType, ConnHandle, SDesc, Flags)
    long CallType;  /* 1 ==> Init, 2==> Check */
    RPC2_Handle ConnHandle;
    SE_Descriptor *SDesc;
    long Flags;
    {
    long rc;
    register struct CEntry *ce;

    ce = rpc2_GetConn(ConnHandle);
    if (ce == NULL) rpc2_Quit(RPC2_NOCONNECTION);
    if (!TestState(ce, SERVER, S_PROCESS)) return(RPC2_FAIL);
    if (ce->sebroken) return(RPC2_SEFAIL2);  

    if (SDesc == NULL || ce->SEProcs == NULL) return(RPC2_FAIL);

    if (CallType == 1)
	{
	if (ce->SEProcs->SE_InitSideEffect == NULL) return(RPC2_FAIL);
	SetState(ce, S_INSE);
	rc = (*ce->SEProcs->SE_InitSideEffect)(ConnHandle, SDesc);
	}
    else
	{
	if (ce->SEProcs->SE_CheckSideEffect == NULL) return(RPC2_FAIL);
	SetState(ce, S_INSE);
	rc = (*ce->SEProcs->SE_CheckSideEffect)(ConnHandle, SDesc, Flags);
	}
    if (rc < RPC2_FLIMIT) ce->sebroken = TRUE;
    SetState(ce, S_PROCESS);
    return(rc);
    }

long RPC2_Unbind(whichConn)
    RPC2_Handle whichConn;
    {
    register struct CEntry *ce;
    register struct MEntry *me;

    say(0, RPC2_DebugLevel, ("RPC2_Unbind()\n"));

#ifdef RPC2DEBUG
    TR_UNBIND();
#endif RPC2DEBUG

    rpc2_Enter();
    rpc2_Unbinds++;

    ce = rpc2_GetConn(whichConn);
    if (ce == NULL) rpc2_Quit(RPC2_NOCONNECTION);
    if (TestState(ce, CLIENT, ~(C_THINK|C_HARDERROR)) ||
    	TestState(ce, SERVER, ~(S_AWAITREQUEST|S_REQINQUEUE|S_PROCESS|S_HARDERROR)) ||
	(ce->MySl != NULL && ce->MySl->ReturnCode != WAITING))
	{
	rpc2_Quit(RPC2_CONNBUSY);
	}

    if (ce->SEProcs != NULL && ce->SEProcs->SE_Unbind != NULL)
	{/* Call side effect routine and ignore return code */
	(*ce->SEProcs->SE_Unbind)(whichConn);
	}

    /* Remove ourselves from our Mgrp if we have one. */
    me = ce->Mgrp;
    if (me != NULL) rpc2_RemoveFromMgrp(me, ce);

    rpc2_FreeConn(whichConn);
    rpc2_Quit(RPC2_SUCCESS);
    }


PRIVATE long cmpaddr(xhost, xportal, yhost, yportal)
    register RPC2_HostIdent *xhost, *yhost;
    register RPC2_PortalIdent *xportal, *yportal;
    /* Returns 0 if x and y are the same address, -1 otherwise.
    	NOTE: this routine is not smart enough to do name<-->number conversions.
	ONLY Internet addresses are recognized.
    */
    {
    if (xhost->Tag != yhost->Tag) return(-1);
    if (xportal->Tag != yportal->Tag) return(-1);
    if (xportal->Tag != RPC2_PORTALBYINETNUMBER || xhost->Tag != RPC2_HOSTBYINETADDR) return(-1);
    if (xhost->Value.InetAddress != yhost->Value.InetAddress) return(-1);
    if (xportal->Value.InetPortNumber != yportal->Value.InetPortNumber) return(-1);
    return(0);
    }

rpc2_time()
    {
    struct timeval tv;
    FT_GetTimeOfDay(&tv, NULL);
    return(tv.tv_sec);
    }


void SavePacketForRetry(pb, ce)
    register RPC2_PacketBuffer *pb;
    register struct CEntry *ce;
    {
    register struct SL_Entry *sl;

    pb->Header.Flags = htonl((ntohl(pb->Header.Flags) | RPC2_RETRY));
    ce->HeldPacket = pb;

    sl = rpc2_AllocSle(REPLY, ce);
    rpc2_ActivateSle(sl, &ce->SaveResponse);
    }



PRIVATE int ResolveBindParms(IN whichConn, IN whichHost, IN whichPortal, IN whichSubsys)
    register struct CEntry *whichConn;
    register RPC2_HostIdent *whichHost;
    register RPC2_PortalIdent *whichPortal;
    register RPC2_SubsysIdent *whichSubsys;
    
    /*  Assumes whichConn points to a newly created connection and fills its PeerAddr by resolving
	the input parameters.  Returns RPC2_SUCCESS on successful resolution, RPC2_FAIL on failure.
    */
    {
    struct servent *sentry;
    struct hostent *hentry;

    /* Resolve host */
    switch(whichHost->Tag)
	{
	case RPC2_HOSTBYINETADDR:	/* you passed it in in network order! */
		whichConn->PeerHost.Tag = RPC2_HOSTBYINETADDR;
		whichConn->PeerHost.Value.InetAddress = whichHost->Value.InetAddress;
		break;

	case RPC2_HOSTBYNAME:
		whichConn->PeerHost.Tag = RPC2_HOSTBYINETADDR;
		if ((hentry = gethostbyname (whichHost->Value.Name)) == NULL) {
		    say(0, RPC2_DebugLevel, ("ResolveBindParms: gethostbyname failed\n"));
		    return(RPC2_FAIL);
		}
		bcopy (hentry->h_addr, &whichConn->PeerHost.Value.InetAddress, hentry->h_length);
	    								/* Already in network byte order */
	    break;

	default:  assert(FALSE);
	}
    
    /* Resolve portal */
    switch(whichPortal->Tag)
	{
	case RPC2_PORTALBYINETNUMBER:	/* you passed it in network order */
		whichConn->PeerPortal.Tag = RPC2_PORTALBYINETNUMBER;
		whichConn->PeerPortal.Value.InetPortNumber = whichPortal->Value.InetPortNumber;
		break;

	case RPC2_PORTALBYNAME:
	    if ((sentry = getservbyname (whichPortal->Value.Name, NULL)) == NULL)
		return(RPC2_FAIL);
	    if (htonl(1) == 1)
		{
		whichConn->PeerPortal.Value.InetPortNumber = sentry->s_port;
		}
	    else
		{
		bcopy(&sentry->s_port, &whichConn->PeerPortal.Value.InetPortNumber, sizeof(short));
		/* ghastly, but true: s_port is in network order, but stored as a 2-byte byte 
			string in a 4-byte field */
		}
	    whichConn->PeerPortal.Tag = RPC2_PORTALBYINETNUMBER;
	    break;

	default:  assert(FALSE);
	}

    /* Obtain subsys id if necessary */
    switch(whichSubsys->Tag)
	{
	case RPC2_SUBSYSBYID:
		whichConn->SubsysId = whichSubsys->Value.SubsysId;
		break;

	case RPC2_SUBSYSBYNAME:
	    if ((whichConn->SubsysId =  getsubsysbyname(whichSubsys->Value.Name)) == -1)
		return(RPC2_FAIL);
	    break;

	default:  assert(FALSE);
	}
    return(RPC2_SUCCESS);
    }

PRIVATE bool GetFilter(inf, outf)
    RPC2_RequestFilter *inf, *outf;
    {
    register struct SubsysEntry *ss;
    register struct CEntry *ce;
    register long i;

    if (inf == NULL)
	{
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


PRIVATE RPC2_PacketBuffer *HeldReq(IN filter, OUT ce)
    RPC2_RequestFilter *filter;
    struct CEntry **ce;
    {
    RPC2_PacketBuffer *pb;
    register long i;

    do
    	{
	say(9, RPC2_DebugLevel, ("Scanning hold queue\n"));
	pb = rpc2_PBHoldList;
	for (i = 0; i < rpc2_PBHoldCount; i++)
	    {
	    if (rpc2_FilterMatch(filter, pb)) break;
	    else pb = (RPC2_PacketBuffer *)pb->Prefix.Next;
	    }
	if (i >= rpc2_PBHoldCount) return(NULL);

	rpc2_UnholdPacket(pb);

	*ce = rpc2_GetConn(pb->Header.RemoteHandle);
	if (*ce == NULL)
	    {/* conn nuked; throw away and rescan */
	    say(9, RPC2_DebugLevel, ("Conn missing, punting request\n"));
	    RPC2_FreeBuffer(&pb);
	    }
	}
    while (!(*ce));

    return(pb);
    }


PRIVATE long GetNewRequest(IN filter, IN timeout, OUT pb, OUT ce)
    RPC2_RequestFilter *filter;
    struct timeval *timeout;
    RPC2_PacketBuffer **pb;
    struct CEntry **ce;
    {
    struct SL_Entry *sl;

    say(9, RPC2_DebugLevel, ("GetNewRequest()\n"));

TryAnother:
    sl = rpc2_AllocSle(REQ, NULL);
    sl->Filter = *filter;	/* structure assignment */
    rpc2_ActivateSle(sl, timeout);

    LWP_WaitProcess((char *)sl);

    /* SocketListener will wake us up */

    switch(sl->ReturnCode)
	{
	case TIMEOUT:	/* timeout */
		    say(9, RPC2_DebugLevel, ("Request wait timed out\n"));
		    rpc2_FreeSle(&sl);
		    return(RPC2_TIMEOUT);
		    
	case ARRIVED:	/* a request that matches my filter was received */
		    say(9, RPC2_DebugLevel, ("Request wait succeeded\n"));
		    *pb = sl->Packet;	/* save a pointer to the buffer */
		    rpc2_FreeSle(&sl);

		    *ce = rpc2_GetConn((*pb)->Header.RemoteHandle);
		    if (*ce == NULL)
			{/* a connection was nuked by someone while we slept */
			say(9, RPC2_DebugLevel, ("Conn gone, punting packet\n"));
			RPC2_FreeBuffer(pb);
			goto TryAnother;
			}
		    return(RPC2_SUCCESS);

	default:  assert(FALSE);
	}
    /*NOTREACHED*/
    }


PRIVATE long MakeFake(INOUT pb, IN ce, OUT xrand, OUT cident)
    RPC2_PacketBuffer *pb;
    struct CEntry *ce;
    long *xrand;
    RPC2_CountedBS *cident;
    {
    /* Synthesize fake packet after extracting encrypted XRandom and clientident */
    long i;
    register struct Init1Body *ib1;
    register RPC2_NewConnectionBody *ncb;

    ib1 = (struct Init1Body *)(pb->Body);
    ncb = &ib1->FakeBody;

    if (strcmp(ib1->Version, RPC2_VERSION) != 0)
	{
	say(9, RPC2_DebugLevel, ("Old Version  Mine: \"%s\"  His: \"%s\"\n",
				 RPC2_VERSION, (char *)ib1->Version));
	ce->Flags |= CE_OLDV;
	}

    *xrand  = ib1->XRandom;		/* Still encrypted */
    cident->SeqLen = ntohl(ncb->ClientIdent.SeqLen);
    cident->SeqBody = (RPC2_ByteSeq) &ncb->ClientIdent.SeqBody;

    /* For RP2Gen or other stub generators:
	(1) leave FakeBody fields in network order
	(2) copy text of client ident to SeqBody
    */
    bcopy(ib1->Text, cident->SeqBody, cident->SeqLen);

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


PRIVATE void SendOKInit2(IN ce)
    register struct CEntry *ce;
    {
    RPC2_PacketBuffer *pb;

    say(9, RPC2_DebugLevel, ("SendOKInit2()\n"));

    RPC2_AllocBuffer(sizeof(struct Init2Body), &pb);
    rpc2_InitPacket(pb, ce, sizeof(struct Init2Body));
    pb->Header.Opcode = RPC2_INIT2;
    if (ce->Flags & CE_OLDV) pb->Header.ReturnCode = RPC2_OLDVERSION;
    else pb->Header.ReturnCode = RPC2_SUCCESS;
    if (ce->TimeStampEcho) {
	assert(ce->RequestTime);
	pb->Header.TimeStamp = ce->TimeStampEcho + rpc2_MakeTimeStamp() -
	                       ce->RequestTime;
    }
    rpc2_htonp(pb);	/* convert to network order */
    rpc2_XmitPacket(rpc2_RequestSocket, pb, &ce->PeerHost, &ce->PeerPortal);
    SavePacketForRetry(pb, ce);
    }

PRIVATE int ServerHandShake(IN ce, IN cident, IN xrand, IN KeyProc, IN emask)
    struct CEntry *ce;
    RPC2_CountedBS *cident;
    long xrand;    /* still encrypted */
    long (*KeyProc)();
    long emask;
    {
    RPC2_EncryptionKey SharedSecret;
    RPC2_PacketBuffer *pb;
    long saveYRandom, rc;

    /* Abort if we cannot get keys or if bogus encryption type specified */
    if (KeyProc == NULL
    	|| (*KeyProc)(cident, SharedSecret, ce->SessionKey) != 0
    	||  (ce->EncryptionType & emask) == 0 	/* unsupported or unknown encryption type */
	||  MORETHANONEBITSET(ce->EncryptionType))
	{
	RejectBind(ce, (long) sizeof(struct Init2Body), (long) RPC2_INIT2);
	return(RPC2_NOTAUTHENTICATED);
	}

    /* Send Init2 packet and await Init3 */
    pb = Send2Get3(ce, SharedSecret, xrand, &saveYRandom);
    if (pb == NULL) return(RPC2_NOTAUTHENTICATED);

    /* Validate Init3 */
    rc = Test3(pb, ce, saveYRandom, SharedSecret);
    RPC2_FreeBuffer(&pb);
    if (rc == RPC2_NOTAUTHENTICATED)
	{
	RejectBind(ce, (long) sizeof(struct Init4Body), (long) RPC2_INIT4);
	return(RPC2_NOTAUTHENTICATED);
	}

    /* Send Init4 */
    Send4AndSave(ce, SharedSecret);
    return(RPC2_SUCCESS);
    }


PRIVATE void RejectBind(ce, bodysize, opcode)
    struct CEntry *ce;
    long bodysize, opcode;
    {
    RPC2_PacketBuffer *pb;

    say(9, RPC2_DebugLevel, ("Rejecting  bind request\n"));

    RPC2_AllocBuffer(bodysize, &pb);
    rpc2_InitPacket(pb, ce, bodysize);
    pb->Header.Opcode = opcode;
    pb->Header.ReturnCode = RPC2_NOTAUTHENTICATED;

    rpc2_htonp(pb);
    rpc2_XmitPacket(rpc2_RequestSocket, pb, &ce->PeerHost, &ce->PeerPortal);
    RPC2_FreeBuffer(&pb);
    }

PRIVATE RPC2_PacketBuffer *Send2Get3(IN ce, IN key, IN xrand, OUT yrand)
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
    rpc2_Decrypt((char *)&xrand, (char *)&xrand, sizeof(xrand), key, ce->EncryptionType);
    xrand = ntohl(xrand);
    say(9, RPC2_DebugLevel, ("XRandom = %ld\n", xrand));
    ib2->XRandomPlusOne = htonl(xrand+1);
    *yrand = rpc2_NextRandom(NULL);
    ib2->YRandom = htonl(*yrand);
    say(9, RPC2_DebugLevel, ("YRandom = %ld\n", *yrand));
    rpc2_Encrypt((char *)ib2, (char *)ib2, sizeof(struct Init2Body), key, ce->EncryptionType);
    if (ce->TimeStampEcho) {     /* service time is now-requesttime */
	assert(ce->RequestTime);
        pb2->Header.TimeStamp = ce->TimeStampEcho + rpc2_MakeTimeStamp() -
	                        ce->RequestTime;
    }
    rpc2_htonp(pb2);

    /* Send Init2 packet and await Init3 packet */
    SetState(ce, S_AWAITINIT3);
    sl = rpc2_AllocSle(OTHER, ce);
    rpc2_SendReliably(ce, sl, pb2, (struct timeval *)NULL);

    switch(sl->ReturnCode)
	{
	case ARRIVED:
		pb3 = sl->Packet;	/* get the Init3 Packet */
		if (pb3->Header.BodyLength != sizeof(struct Init3Body))
		    {
		    say(9, RPC2_DebugLevel, ("Runt Init3 packet\n"));
		    RPC2_FreeBuffer(&pb3);	/* will set to NULL */
		    }
		break;

	case NAKED:
	case TIMEOUT:
		/* free connection, buffers, and quit */
		say(9, RPC2_DebugLevel, ("Failed to send INIT2\n"));
		pb3 = NULL;
		break;
		
	default:    assert(FALSE);
	}

    /* Clean up and quit */
    rpc2_FreeSle(&sl);
    RPC2_FreeBuffer(&pb2);	/* release the Init2 Packet */
    return(pb3);    
    }


PRIVATE long Test3(pb, ce, yrand, ekey)
    RPC2_PacketBuffer *pb;
    struct CEntry *ce;
    long yrand;
    RPC2_EncryptionKey ekey;
    {
    struct Init3Body *ib3;

    ib3 = (struct Init3Body *)pb->Body;
    rpc2_Decrypt((char *)ib3, (char *)ib3, sizeof(struct Init3Body), ekey, ce->EncryptionType);
    ib3->YRandomPlusOne = ntohl(ib3->YRandomPlusOne);
    say(9, RPC2_DebugLevel, ("YRandomPlusOne = %ld\n", ib3->YRandomPlusOne));
    if (ib3->YRandomPlusOne == yrand+1) return(RPC2_SUCCESS);
    else return(RPC2_NOTAUTHENTICATED);	
    }

PRIVATE void Send4AndSave(ce, ekey)
    struct CEntry *ce;
    RPC2_EncryptionKey ekey;
    {
    RPC2_PacketBuffer *pb;
    struct Init4Body *ib4;

    say(9, RPC2_DebugLevel, ("Send4AndSave()\n"));

    RPC2_AllocBuffer(sizeof(struct Init4Body), &pb);
    rpc2_InitPacket(pb, ce, sizeof(struct Init4Body));
    pb->Header.Opcode = RPC2_INIT4;
    pb->Header.ReturnCode = RPC2_SUCCESS;
    ib4 = (struct Init4Body *)pb->Body;
    bcopy(ce->SessionKey, ib4->SessionKey, sizeof(RPC2_EncryptionKey));
    ib4->InitialSeqNumber = htonl(ce->NextSeqNumber);
    rpc2_Encrypt((char *)ib4, (char *)ib4, sizeof(struct Init4Body), ekey, ce->EncryptionType);
    if (ce->TimeStampEcho) {     /* service time is now-requesttime */
	assert(ce->RequestTime);
        pb->Header.TimeStamp = ce->TimeStampEcho + rpc2_MakeTimeStamp() -
	                       ce->RequestTime;
    }
    rpc2_htonp(pb);

    /* Send packet; don't bother waiting for acknowledgement */
    rpc2_XmitPacket(rpc2_RequestSocket, pb, &ce->PeerHost, &ce->PeerPortal);

    SavePacketForRetry(pb, ce);
    }
