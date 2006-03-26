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
	Routines for MultiRPC
*/


#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "rpc2.private.h"
#include <rpc2/se.h>
#include "trace.h"
#include "cbuf.h"
#include <rpc2/multi.h>


extern void rpc2_IncrementSeqNumber();
extern long HandleResult();
extern void rpc2_PrintPacketHeader();
extern long SetupMulticast();

typedef struct  {
		struct CEntry	    *ceaddr;
		RPC2_PacketBuffer   *req;
		struct SL_Entry	    *sle;
		long		    retcode;
		} MultiCon;

typedef struct	{
		struct SL_Entry	    **pending;
		long		    indexlen;
		long		    *indexlist;
		} PacketCon;

static void SetupConns(int HowMany, MultiCon *mcon,
		       RPC2_Handle ConnHandleList[]);
static void SetupPackets(int HowMany, MultiCon *mcon,
			 RPC2_Handle ConnHandleList[],
			 SE_Descriptor SDescList[], RPC2_PacketBuffer *Request);
static MultiCon *InitMultiCon(int HowMany);
static void FreeMultiCon(int HowMany, MultiCon *mcon);
static long mrpc_SendPacketsReliably();
static PacketCon *InitPacketCon(int HowMany);
static void FreePacketCon(PacketCon *pcon);
long exchange(PacketCon *pcon, int cur_ind);
static void MSend_Cleanup(int HowMany, MultiCon *mcon,
			  SE_Descriptor SDescList[],
			  struct timeval *Timeout, PacketCon *pcon);
static inline long EXIT_MRPC(long code, int HowMany, RPC2_Integer *RCList, MultiCon *context);

#define GOODSEDLE(i)  (SDescList && SDescList[i].Tag != OMITSE)


long RPC2_MultiRPC(IN HowMany, IN ConnHandleList, IN RCList, IN MCast,
		   IN Request, IN SDescList, IN UnpackMulti, IN OUT ArgInfo,
		   IN BreathOfLife)
    int	HowMany;			/* no of connections involved */
    RPC2_Handle	ConnHandleList[];
    RPC2_Integer RCList[];	/* NULL or list of per-connection return codes */
    RPC2_Multicast *MCast;		/* NULL if multicast not used */
    RPC2_PacketBuffer *Request;/* Gets clobbered during call: BEWARE */
    SE_Descriptor SDescList[];
    long (*UnpackMulti)();
    ARG_INFO *ArgInfo;
    struct timeval *BreathOfLife;
{
    MultiCon *mcon;
    int	host;
    int SomeConnsOK;
    long rc = 0;


    rpc2_Enter();
    say(0, RPC2_DebugLevel, "Entering RPC2_MultiRPC\n");

    TR_MULTI();

    /* perform sanity checks */
    assert(Request->Prefix.MagicNumber == OBJ_PACKETBUFFER);

    /* get context pointer */
    mcon = InitMultiCon(HowMany);

    /*  verify and set connection state */
    SetupConns(HowMany, mcon, ConnHandleList);

    /* prepare all of the packets */
    SetupPackets(HowMany, mcon, ConnHandleList, SDescList, Request);

    /* call UnpackMulti on all bad connections;
       if there are NO good connections, exit */
    SomeConnsOK = FALSE;
    for (host = 0; host < HowMany; host++) {
	if (mcon[host].retcode > RPC2_ELIMIT) {
	    SomeConnsOK = TRUE;
	} else {
	    if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, NULL, 
			       mcon[host].retcode, host) == -1) {
		return EXIT_MRPC(rc, HowMany, RCList, mcon);
	    }
	}
    }

    if (!SomeConnsOK) /* NO usable connections */
	return EXIT_MRPC(rc, HowMany, RCList, mcon);

    /* finally safe to update the state of the good connections */
    for	(host =	0; host	< HowMany; host++) {
	if (mcon[host].retcode > RPC2_ELIMIT) {
	    SetState(mcon[host].ceaddr, C_AWAITREPLY);
	}
    }

    /* send packets and await replies */
    say(9, RPC2_DebugLevel, "Sending requests\n");
    rc = mrpc_SendPacketsReliably(HowMany, mcon, ConnHandleList,
				  ArgInfo, SDescList, UnpackMulti,BreathOfLife);

    switch((int) rc) {
    case RPC2_SUCCESS:
	break;
    case RPC2_TIMEOUT:
    case RPC2_FAIL:
	say(9, RPC2_DebugLevel,
	    "mrpc_SendPacketsReliably()--> %s\n", RPC2_ErrorMsg(rc));
	break;
    default:
	say(9, RPC2_DebugLevel, 
	    "Bad return code for mrpc_SendPacketsReliably: %ld\n", rc);
	rc = RPC2_FAIL;
    }

    host = HowMany - 1;
    return EXIT_MRPC(rc, HowMany, RCList, mcon);
}


/* easier to manage than the former macro definition */
static inline long
EXIT_MRPC(long code, int HowMany, RPC2_Integer *RCList, MultiCon *mcon)
{
    int i;

    /* copy arguments into the return code lists, possibly translating 
       error codes */
    if (RCList) {
	for (i = 0; i < HowMany; i++ ) {
	    RPC2_Integer rc;
	    rc = mcon[i].retcode;
#ifdef ERRORTR
	    rc = RPC2_R2SError(rc);
#endif 
	    RCList[i] = rc;
	}
    }

    FreeMultiCon(HowMany, mcon);

    rpc2_Quit(code);
}

static void SetupConns(int HowMany, MultiCon *mcon,
		       RPC2_Handle ConnHandleList[])
{
    struct CEntry   *thisconn;
    int		    host;
    long	    rc, setype = -1;	    /* -1 ==> first time through loop */

    /* verify the handles; don't update the connection state of the "good" connections yet */
    for (host = 0; host < HowMany; host++)
    {
	thisconn = mcon[host].ceaddr = rpc2_GetConn(ConnHandleList[host]);
	if (!thisconn) {
	    mcon[host].retcode = RPC2_NOCONNECTION;
	    continue;
	}
	assert(thisconn->MagicNumber == OBJ_CENTRY);
 	if (!TestRole(thisconn, CLIENT))
	{
	    mcon[host].retcode = RPC2_FAIL;
	    continue;
	}
	switch ((int) (thisconn->State  & 0x0000ffff))
	{
	    case C_HARDERROR:
		mcon[host].retcode = RPC2_FAIL;
		break;
	    
	    case C_THINK:
		/* wait to update connection state */
		break;

	    default:
/* This isn't the behavior the manual claims, but it's what I need. -JJK */
		{
		    if (TRUE/*EnqueueRequest*/)
			{
			say(0, RPC2_DebugLevel, "Enqueuing on connection %#x\n", ConnHandleList[host]);
			LWP_WaitProcess((char *)thisconn);
			say(0, RPC2_DebugLevel, "Dequeueing on connection %#x\n", ConnHandleList[host]);
			host = 0;	/* !!! restart loop !!! */
			break;
			}
		    else {
			/* can't continue if ANY connections are busy */
			rc = RPC2_MGRPBUSY;
			goto exit_fail;
		    }
		}
	}
    }

    /* insist that all connections have the same side-effect type (or none) */
    for (host = 0; host < HowMany; host++)
	if (mcon[host].retcode > RPC2_ELIMIT)
	{
	    long this_setype = mcon[host].ceaddr->SEProcs ? 
		mcon[host].ceaddr->SEProcs->SideEffectType: 0;

	    if (setype == -1)		/* first time through loop */
		setype = this_setype;

	    if (this_setype != setype) {
		rc = RPC2_FAIL;  /* better return code ? */
		goto exit_fail;
	    }
	}

    /* We delay updating the state of the "good" connections until we know */
    /* FOR SURE that mrpc_SendPacketsReliably() will be called. */
    return;

exit_fail:
    for (host = 0; host < HowMany; host++)
	if (mcon[host].retcode > RPC2_ELIMIT)
	    mcon[host].retcode = rc;
    return;
}


static void SetupPackets(int HowMany, MultiCon *mcon,
			 RPC2_Handle ConnHandleList[],
			 SE_Descriptor SDescList[], RPC2_PacketBuffer *Request)
{
    struct CEntry	*thisconn;
    RPC2_PacketBuffer	*thisreq;
    int			host;

    /* allocate and setup HowMany request packets */
    /* we won't send on bad connections, so don't bother to set them up */
    for (host = 0; host < HowMany; host++) {
	if (mcon[host].retcode <= RPC2_ELIMIT)
	    continue;

	RPC2_AllocBuffer(Request->Header.BodyLength, &thisreq);

	/* preserve address of allocated packet */
	mcon[host].req = thisreq;
	thisconn = mcon[host].ceaddr;

	/* initialize header fields to defaults, and copy body of request packet */
	rpc2_InitPacket(thisreq, thisconn, Request->Header.BodyLength);
	memcpy(thisreq->Body, Request->Body, Request->Header.BodyLength);

	/* complete non-default header fields */
	thisreq->Header.SeqNumber = thisconn->NextSeqNumber;
	thisreq->Header.Opcode = Request->Header.Opcode;	/* set by client */
	thisreq->Header.BindTime = thisconn->RTT >> RPC2_RTT_SHIFT;
	if (thisconn->RTT && thisreq->Header.BindTime == 0)
	    thisreq->Header.BindTime = 1;  /* ugh. zero is overloaded. */

	thisreq->Header.Flags = 0;

	if (thisconn->SecurityLevel == RPC2_HEADERSONLY ||
	    thisconn->SecurityLevel == RPC2_SECURE)
	    thisreq->Header.Flags |= RPC2_ENCRYPTED;
    }

    /* Notify side effect routine, if any. */
    if (SDescList != NULL) {
	/* We have already verified that all connections have the same side-effect type (or none), */
	/* so we can simply invoke the procedure corresponding to the first GOOD connection. */
	thisconn = 0;
	for (host = 0; host < HowMany; host++)
	    if (mcon[host].retcode > RPC2_ELIMIT)
	    {
		thisconn = mcon[host].ceaddr;
		break;
	    }
	if (thisconn && thisconn->SEProcs && thisconn->SEProcs->SE_MultiRPC1)
	{
	    long *seretcode;
	    RPC2_PacketBuffer **preqs;
	    assert((seretcode = (long *)malloc(HowMany * sizeof(long))) != NULL);
	    assert((preqs = (RPC2_PacketBuffer **)malloc(HowMany * sizeof(RPC2_PacketBuffer *))) != NULL);
	    for (host = 0; host < HowMany; host++) {
		seretcode[host] = mcon[host].retcode;
		preqs[host] = mcon[host].req;
	    }

	    (*thisconn->SEProcs->SE_MultiRPC1)(HowMany, ConnHandleList, SDescList, preqs, seretcode);
	    for (host = 0; host < HowMany; host++) {
		/* Has the sideeffect modified the original request? */
		if (mcon[host].req != preqs[host]) {
		    RPC2_FreeBuffer(&mcon[host].req);
		    mcon[host].req = preqs[host];
		}
		if (seretcode[host] == RPC2_SUCCESS)
		    continue;

		/* Any new errors? */
		if (mcon[host].retcode != seretcode[host])
		{
		    if (seretcode[host] > RPC2_FLIMIT) {
			SetState(mcon[host].ceaddr, C_THINK); /* reset connection state */
			mcon[host].retcode = RPC2_SEFAIL1;
		    } else {
			rpc2_SetConnError(mcon[host].ceaddr);
			mcon[host].retcode = RPC2_SEFAIL2;
		    }
		}
	    }
	    free(preqs);
	    free(seretcode);
	}
    }

    /* complete setup of the individual packets */
    /* we won't send on bad connections, so don't bother to set them up */
    for (host = 0; host < HowMany; host++) {
	if (mcon[host].retcode <= RPC2_ELIMIT)
	    continue;

	thisconn = mcon[host].ceaddr;
	thisreq = mcon[host].req;

	/* create call entry */
	mcon[host].sle = rpc2_AllocSle(OTHER, thisconn);
	mcon[host].sle->TElem.BackPointer = (char *)mcon[host].sle;

	/* convert to network order */
	rpc2_htonp(thisreq);

	/* Encrypt appropriate portions of the packet */
	switch ((int) thisconn->SecurityLevel)
	{
	case RPC2_OPENKIMONO:
	case RPC2_AUTHONLY:
	    break;

	case RPC2_HEADERSONLY:
	    rpc2_Encrypt((char *)&thisreq->Header.BodyLength,
			 (char *)&thisreq->Header.BodyLength,
			 sizeof(struct RPC2_PacketHeader) - 4*sizeof(RPC2_Integer),
			 thisconn->SessionKey, thisconn->EncryptionType);
	    break;

	case RPC2_SECURE:
	    rpc2_Encrypt((char *)&thisreq->Header.BodyLength,
			 (char *)&thisreq->Header.BodyLength,
			 thisreq->Prefix.LengthOfPacket - 4*sizeof(RPC2_Integer),
			 thisconn->SessionKey, thisconn->EncryptionType);
	    break;
	}
    }
}

	
/* Get a free context */
static MultiCon *InitMultiCon(int count)
{
    MultiCon *mcon;
    int i;

    mcon = (MultiCon *) calloc(count, sizeof(MultiCon));
    assert(mcon);

    for (i = 0; i < count; i++)
	mcon[i].retcode = RPC2_ABANDONED;

    return(mcon);
}

/* deallocate buffers and free allocated arrays */
void FreeMultiCon(int HowMany, MultiCon *mcon)
{
    int i;

    for (i = 0; i < HowMany; i++) {
	if(mcon[i].sle)
	    rpc2_FreeSle(&mcon[i].sle);

	if (mcon[i].req)
          RPC2_FreeBuffer(&mcon[i].req);

	if (mcon[i].ceaddr)
	    LWP_NoYieldSignal((char *)mcon[i].ceaddr);
    }
    free(mcon);
}

	/* MultiRPC version */
static long mrpc_SendPacketsReliably(
    int HowMany,
    MultiCon *mcon,
    RPC2_Handle ConnHandleList[],	/* array of connection ids */
    ARG_INFO *ArgInfo,			/* Structure of client information
					   (built in MakeMulti) */
    SE_Descriptor SDescList[],		/* array of side effect descriptors */
    long (*UnpackMulti)(),		/* pointer to unpacking routine */
    struct timeval *TimeOut)		/* client specified timeout */
{
    struct SL_Entry *slp;
    RPC2_PacketBuffer *preply;  /* RPC2 Response buffers */
    struct CEntry *c_entry;
    long finalrc, secode = 0;
    long thispacket, hopeleft, i;
    struct timeval *tout;
    int packets = 1; 			/* packet counter for LWP yield */
    int busy = 0;
    int goodpackets = 0;		/* packets with good connection state */
    PacketCon *pcon;
    unsigned long timestamp;

#define	EXIT_MRPC_SPR(rc)\
	{\
	MSend_Cleanup(HowMany, mcon, SDescList, TimeOut, pcon);\
	return(rc);\
	}

    say(0, RPC2_DebugLevel, "mrpc_SendPacketsReliably()\n");

    TR_MSENDRELIABLY();

    /* find a context */
    /* the packet_con management should be redone to ensure that allocation never fails! */
    pcon = InitPacketCon(HowMany);
/*
    if((pcon = InitPacketCon(HowMany)) == NULL)
	for(i = 0; i < HowMany; i++)
	    {
	    if (mcon[i].sle == NULL) continue;
	    if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, NULL, RPC2_FAIL, i) == -1) 
		return(RPC2_FAIL);
	    }
*/

    if (TimeOut) {	    /* create a time bomb */
	/* allocate timer entry */
	slp = pcon->pending[HowMany] = rpc2_AllocSle(OTHER, NULL);  
	rpc2_ActivateSle(slp, TimeOut);
    }

    timestamp = rpc2_MakeTimeStamp();
    say(9, RPC2_DebugLevel, "Sending initial packets at time %lu\n", timestamp);

    /* Do an initial send of packets on all good connections */
    /* for estimating the effiency of the calculation */
    /* should separate this into separate LWP for efficiency */
    for (thispacket = HowMany - 1; thispacket >= 0; thispacket--)
    {
	/* initialize permuted index array */
	pcon->indexlist[thispacket] = thispacket;
	
	slp = mcon[thispacket].sle;

	if (!slp) /* something is wrong with connection - don't send packet */
	{
	    if (mcon[thispacket].retcode > RPC2_ELIMIT)
		mcon[thispacket].retcode = RPC2_FAIL;
	    exchange(pcon, thispacket);
	    busy++;
	    continue;
	}
	pcon->pending[goodpackets++] = slp;    /* build array of good packets */

	/* send the packet and activate socket listener entry */
	/* offer control to Socket Listener every 32 packets to prevent buffer overflow */
	if ((packets++ & 0x1f) && (packets < pcon->indexlen - 6)) {
	    LWP_DispatchProcess();
	    timestamp = rpc2_MakeTimeStamp();
	}

	mcon[thispacket].req->Header.TimeStamp = htonl(timestamp);
	rpc2_XmitPacket(mcon[thispacket].req,
			mcon[thispacket].ceaddr->HostInfo->Addr,
			mcon[thispacket].ceaddr->sa, 0);

        if (rpc2_Bandwidth) 
	    rpc2_ResetLowerLimit(mcon[thispacket].ceaddr, mcon[thispacket].req);

	slp->RetryIndex = 1;
	rpc2_ActivateSle(slp, &mcon[thispacket].ceaddr->Retry_Beta[1]);
    }

    /* don't forget to account for the Timer entry */
    if (TimeOut)
	goodpackets++;

    if (busy == HowMany)
	EXIT_MRPC_SPR(RPC2_FAIL)		    /* no packets were sent */

    if (busy == 0) finalrc = RPC2_SUCCESS;	    /* RPC2_FAIL if anything goes wrong */
    else finalrc = RPC2_FAIL;

    do {
	hopeleft = 0;
	/* wait for SocketListener to tap me on the shoulder */
	LWP_MwaitProcess(1, (char **)pcon->pending);

	if (TimeOut && pcon->pending[HowMany]->ReturnCode == TIMEOUT)
	    /* Overall timeout expired: clean up state and quit */
	    EXIT_MRPC_SPR(RPC2_TIMEOUT)

	/* the loop below looks at a decreasing list of sl entries using the permuted index array for sorting */
	for(i = 0; i < pcon->indexlen; i++)
	{
	    thispacket = pcon->indexlist[i];
	    c_entry = mcon[thispacket].ceaddr;
	    slp = mcon[thispacket].sle;
	    switch(slp->ReturnCode)
		{
		case WAITING:   
		    hopeleft = 1;	/* someday we will be told about this packet */
		    break;		/* switch */

		case ARRIVED:
		    /* know this hasn't yet been processd */
		    say(9, RPC2_DebugLevel, "Request reliably sent on 0x%p\n", c_entry);
		    /* remove current connection from future examination */
		    i = exchange(pcon, i);

		    /* At this point the final reply has been received;
		       SocketListener has already decrypted it. */
		    preply = slp->Packet;
		    mcon[thispacket].retcode = preply->Header.ReturnCode;

		    /* Do preliminary side effect processing: */
		    /* Notify side effect routine, if any */
		    if (GOODSEDLE(thispacket) && c_entry->SEProcs != NULL &&
		        c_entry->SEProcs->SE_MultiRPC2 != NULL)
		        if ((secode = (*c_entry->SEProcs->SE_MultiRPC2)(ConnHandleList[thispacket],
									&(SDescList[thispacket]), preply)) != RPC2_SUCCESS)
			    if (secode < RPC2_FLIMIT)
				{
				rpc2_SetConnError(c_entry);
				finalrc = RPC2_FAIL;
				if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, preply, RPC2_SEFAIL2, thispacket) == -1)
				    /* enough responses, return */
				    EXIT_MRPC_SPR(finalrc)
				else
				    /* continue waiting for responses */
				    break;
				}
		    /* return code may be LWP_SUCCESS or LWP_ENOWAIT */
/*		    LWP_NoYieldSignal((char *)c_entry);*/
		    /* Continue side effect processing: */
		    if (GOODSEDLE(thispacket) && (secode != RPC2_SUCCESS || 
		        SDescList[thispacket].LocalStatus == SE_FAILURE ||
		        SDescList[thispacket].RemoteStatus == SE_FAILURE))
		        {
		        finalrc = RPC2_FAIL;
		        if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, preply, RPC2_SEFAIL1, thispacket) == -1)
			    /* enough responses, return */
			    EXIT_MRPC_SPR(finalrc)
			else
			    /* continue waiting for responses */
			    break;
			}
		    /* RPC2_SUCCESS if ARRIVED and SE's are OK */
		    if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, preply, RPC2_SUCCESS, thispacket) == -1)
		        /* enough responses, return */
		        EXIT_MRPC_SPR(finalrc)
		    else
		        /* continue waiting for responses */
		        break;

		case KEPTALIVE:
		    hopeleft = 1;
		    slp->RetryIndex = 0;
		    rpc2_ActivateSle(slp, &c_entry->Retry_Beta[0]);
		    break;	/* switch */
		    
		case NAKED:	/* explicitly NAK'ed this time or earlier */
		    say(9, RPC2_DebugLevel, "Request NAK'ed on 0x%p\n", c_entry);
		    rpc2_SetConnError(c_entry);	    /* does signal on ConnHandle also */
		    i = exchange(pcon, i);
		    finalrc = RPC2_FAIL;
    		    mcon[thispacket].retcode = RPC2_NAKED;
		    /* call side-effect routine, and ignore result */
		    if (GOODSEDLE(thispacket) &&
			 c_entry->SEProcs != NULL && c_entry->SEProcs->SE_MultiRPC2 != NULL)
			(*c_entry->SEProcs->SE_MultiRPC2)(ConnHandleList[thispacket], &(SDescList[thispacket]), NULL);
		    if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, NULL, RPC2_NAKED, thispacket) == -1)
		        /* enough responses, return */
		        EXIT_MRPC_SPR(finalrc)
		    else
		        /* continue waiting for responses */
		        break;

		case TIMEOUT:
		    if ((hopeleft = rpc2_CancelRetry(c_entry, slp)))
			    break;    /* switch, we heard from side effect */
		    /* 
		     * check if all retries exhausted, if not check if later
		     * retries shortened to 0 because of high LOWERLIMIT.
		     */
		    if ((slp->RetryIndex > c_entry->Retry_N) || 
			((c_entry->Retry_Beta[slp->RetryIndex+1].tv_sec <= 0) &&
			 (c_entry->Retry_Beta[slp->RetryIndex+1].tv_usec <= 0)))
			{
			say(9, RPC2_DebugLevel, "Request failed on 0x%p\n", c_entry);
			rpc2_SetConnError(c_entry); /* does signal on ConnHandle also */
			/* remote site is now declared dead; mark all inactive connections to there likewise */
			i = exchange(pcon, i);
			finalrc = RPC2_FAIL;
			mcon[thispacket].retcode = RPC2_DEAD;

			/* call side-effect routine, and ignore result */
			if (GOODSEDLE(thispacket) && c_entry->SEProcs != NULL && c_entry->SEProcs->SE_MultiRPC2 != NULL)
			    (*c_entry->SEProcs->SE_MultiRPC2)(ConnHandleList[thispacket], &(SDescList[thispacket]), NULL);

			if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, NULL, RPC2_DEAD, thispacket) == -1)
			    /* enough responses, return */
			    EXIT_MRPC_SPR(finalrc)
			else
			    /* continue waiting for responses */
			    break;
			}
		    /* else retry with the next Beta value  for timeout */
		    hopeleft = 1;
		    slp->RetryIndex += 1;
		    tout = &c_entry->Retry_Beta[slp->RetryIndex];
		    rpc2_ActivateSle(slp, tout);
		    say(9, RPC2_DebugLevel, "Sending retry %ld at %ld on %#x (timeout %ld.%06ld)\n", slp->RetryIndex, rpc2_time(), c_entry->UniqueCID, tout->tv_sec, tout->tv_usec);
		    mcon[thispacket].req->Header.Flags = htonl((ntohl(mcon[thispacket].req->Header.Flags) | RPC2_RETRY));
		    mcon[thispacket].req->Header.TimeStamp = htonl(rpc2_MakeTimeStamp());
		    rpc2_Sent.Retries += 1;	/* RPC retries are currently NOT multicasted! -JJK */
		    rpc2_XmitPacket(mcon[thispacket].req,
				    c_entry->HostInfo->Addr, c_entry->sa, 0);
		    break;	/* switch */
		    
		default:    /* abort */
		    /* BUSY ReturnCode should never go into switch */
		    assert(FALSE);
		}
	}
    } while (hopeleft);

    EXIT_MRPC_SPR(finalrc)
#undef	EXIT_MRPC_SPR
}


static PacketCon *InitPacketCon(int count)
{
    PacketCon *pcon;

    /* allocate new context */
    pcon = (PacketCon *) malloc(sizeof(PacketCon));
    assert(pcon);

    pcon->pending = (struct SL_Entry **)calloc(count+2, sizeof(struct SL_Entry *));
    assert(pcon->pending);

    pcon->indexlen = count;
    pcon->indexlist = (long *)malloc(count * sizeof(long));
    assert(pcon->indexlist);

    return(pcon);
}

void FreePacketCon(PacketCon *pcon)
{
    free(pcon->indexlist);
    free(pcon->pending);
    free(pcon);
}

	
/* exchange two elements of the socket listener element array */
/* returns value for loop counter: decrements it iff elements */
/* are physically exchanged */
long exchange(PacketCon *pcon, int cur_ind)
{
    long tmp;

    pcon->indexlen--;
    if (cur_ind == pcon->indexlen) return(cur_ind);
    tmp = pcon->indexlist[cur_ind];
    pcon->indexlist[cur_ind] = pcon->indexlist[pcon->indexlen];
    pcon->indexlist[pcon->indexlen] = tmp;
    return(cur_ind - 1);
}


/* Clean up state before exiting mrpc_SendPacketsReliably */
static void MSend_Cleanup(int HowMany, MultiCon *mcon,
			  SE_Descriptor SDescList[],
			  struct timeval *Timeout, PacketCon *pcon)
{
    long thispacket, i;
    struct SL_Entry *slp;

    for (i = 0; i < pcon->indexlen; i++)
    {
	thispacket = pcon->indexlist[i];
	slp = mcon[thispacket].sle;
	TM_Remove(rpc2_TimerQueue, &slp->TElem);

	/* Call side-effect routine and increment connection sequence number for abandoned requests */
	if (GOODSEDLE(thispacket) && mcon[thispacket].ceaddr->SEProcs && mcon[thispacket].ceaddr->SEProcs->SE_MultiRPC2)
	    (*mcon[thispacket].ceaddr->SEProcs->SE_MultiRPC2)(mcon[thispacket].ceaddr->UniqueCID, &SDescList[thispacket], NULL);
	rpc2_IncrementSeqNumber(mcon[thispacket].ceaddr);
	SetState(mcon[thispacket].ceaddr, C_THINK);
/*	LWP_NoYieldSignal((char *)mcon[thispacket].ceaddr);*/
    }

    if (Timeout) {
	slp = pcon->pending[HowMany]; /* Tag assumed to be TIMEENTRY */
	if (slp->ReturnCode == WAITING)
	{
	    /* delete  time bomb if it has not fired  */
	    TM_Remove(rpc2_TimerQueue, &slp->TElem);
	}
	rpc2_FreeSle(&slp); /* free timer entry */
    }
    FreePacketCon(pcon);
}
