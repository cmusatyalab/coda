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
		long		    count;
		struct CEntry	    **ceaddr;
		RPC2_PacketBuffer   **req;
		RPC2_PacketBuffer   **preq;
		struct SL_Entry	    **slarray;
		long		    *retcode;
		} MultiCon;

typedef struct	{
		long		    count;
		struct SL_Entry	    **pending;
		long		    *indexlist;
		} PacketCon;

static void SetupConns(MultiCon *mcon, RPC2_Handle ConnHandleList[],
		       RPC2_Multicast *MCast, struct MEntry *me,
		       SE_Descriptor SDescList[]);
static void SetupPackets(MultiCon *mcon, RPC2_Handle ConnHandleList[],
			 RPC2_Multicast *MCast, struct MEntry *me,
			 SE_Descriptor SDescList[], RPC2_PacketBuffer *Request);
static MultiCon *InitMultiCon(int HowMany);
static void FreeMultiCon(MultiCon *mcon);
static void cleanup(struct MEntry *me);
static long mrpc_SendPacketsReliably();
static PacketCon *InitPacketCon(int HowMany);
static void FreePacketCon(PacketCon *pcon);
long exchange(PacketCon *pcon, int cur_ind);
static void MSend_Cleanup(MultiCon *mcon, SE_Descriptor SDescList[],
			  struct timeval *Timeout, PacketCon *pcon);
static void mrpc_ProcessRC(long *in, long *out, int howmany);
static inline long EXIT_MRPC(long code, long *RCList, MultiCon *context, struct MEntry *me);

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
    struct MEntry *me = NULL;
    int SomeConnsOK;
    long rc = 0;


    rpc2_Enter();
    say(0, RPC2_DebugLevel, "Entering RPC2_MultiRPC\n");
	    
    TR_MULTI();

    /* perform sanity checks */
    assert(Request->Prefix.MagicNumber == OBJ_PACKETBUFFER);

    /* get context pointer */
    mcon = InitMultiCon(HowMany);

    /* setup for multicast */
    if (MCast) {
	rc = SetupMulticast(MCast, &me, HowMany, ConnHandleList);
	if (rc != RPC2_SUCCESS) {
	    for (host = 0; host < HowMany; host++)
		mcon->retcode[host] = rc;
	    return EXIT_MRPC(rc, RCList, mcon, me);
	}
    }

    /*  verify and set connection state */
    SetupConns(mcon, ConnHandleList, MCast, me, SDescList);

    /* prepare all of the packets */
    SetupPackets(mcon, ConnHandleList, MCast, me, SDescList, Request);

    /* call UnpackMulti on all bad connections; 
       if there are NO good connections, exit */
    SomeConnsOK = FALSE;
    for (host = 0; host < HowMany; host++) {
	if (mcon->retcode[host] > RPC2_ELIMIT) {
	    SomeConnsOK = TRUE;
	} else {
	    if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, NULL, 
			       mcon->retcode[host], host) == -1) {
		return EXIT_MRPC(rc, RCList, mcon, me);
	    }
	}
    }

    if (!SomeConnsOK) /* NO usable connections */
	return EXIT_MRPC(rc, RCList, mcon, me);

    /* finally safe to update the state of the good connections */
    for	(host =	0; host	< HowMany; host++) { 
	if (mcon->retcode[host] > RPC2_ELIMIT) { 
	    SetState(mcon->ceaddr[host], C_AWAITREPLY);
	}
    }

    /* send packets and await replies */
    say(9, RPC2_DebugLevel, "Sending requests\n");
    /* allocate timer entry */
    mcon->slarray[HowMany] = rpc2_AllocSle(OTHER, NULL);  
    mcon->slarray[HowMany + 1] = NULL;
    rc = mrpc_SendPacketsReliably(mcon, ConnHandleList, MCast, me, ArgInfo,
				  SDescList, UnpackMulti, BreathOfLife);
    /* free timer entry */
    rpc2_FreeSle(&(mcon->slarray[HowMany]));	
    
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
    return EXIT_MRPC(rc, RCList, mcon, me);
}


/* easier to manage than the former macro definition */
static inline long
EXIT_MRPC(long code, long *RCList, MultiCon *mcon, struct MEntry *me)
{
    if (RCList)
	mrpc_ProcessRC(mcon->retcode, RCList, mcon->count);

    FreeMultiCon(mcon);
    if (me)
	cleanup(me);

    rpc2_Quit(code);
}

/* copy arguments into the return code lists, possibly translating 
   error codes */
static void mrpc_ProcessRC(long *in, long *out, int howmany) 
{
#ifdef ERRORTR
    int i;
    for (i = 0; i < howmany; i++ )
	out[i] = RPC2_R2SError(in[i]);
#else
    memcpy(out, in, sizeof(long) * howmany);
#endif 
}

static void SetupConns(MultiCon *mcon, RPC2_Handle ConnHandleList[],
		       RPC2_Multicast *MCast, struct MEntry *me,
		       SE_Descriptor SDescList[])
{
    struct CEntry   *thisconn;
    int		    host;
    long	    rc, setype = -1;	    /* -1 ==> first time through loop */

    /* verify the handles; don't update the connection state of the "good" connections yet */
    for (host = 0; host < mcon->count; host++)
    {
	thisconn = mcon->ceaddr[host] = rpc2_GetConn(ConnHandleList[host]);
	if (!thisconn) {
	    mcon->retcode[host] = RPC2_NOCONNECTION;
	    continue;
	}
	assert(thisconn->MagicNumber == OBJ_CENTRY);
 	if (!TestRole(thisconn, CLIENT))
	{
	    mcon->retcode[host] = RPC2_FAIL;
	    continue;
	}
	switch ((int) (thisconn->State  & 0x0000ffff))
	{
	    case C_HARDERROR:
		mcon->retcode[host] = RPC2_FAIL;
		break;
	    
	    case C_THINK:
		/* wait to update connection state */
		break;

	    default:
/* This isn't the behavior the manual claims, but it's what I need. -JJK */
/*		if (MCast)*/
		{
		    if (TRUE/*EnqueueRequest*/)
			{
			say(0, RPC2_DebugLevel, "Enqueuing on connection 0x%lx\n", ConnHandleList[host]);
			LWP_WaitProcess((char *)thisconn);
			say(0, RPC2_DebugLevel, "Dequeueing on connection 0x%lx\n", ConnHandleList[host]);
			host = 0;	/* !!! restart loop !!! */
			break;
			}
		    else {
			/* can't continue if ANY connections are busy */
			rc = RPC2_MGRPBUSY;
			goto exit_fail;
		    }
		}
/*		else*/
/*		    {*/
/*		    mcon->retcode[host] = RPC2_CONNBUSY;*/
/*		    break;*/
/*		    }*/
	}
    }

    /* insist that all connections have the same side-effect type (or none) */
    for (host = 0; host < mcon->count; host++)
	if (mcon->retcode[host] > RPC2_ELIMIT)
	{
	    long this_setype = mcon->ceaddr[host]->SEProcs ? 
		mcon->ceaddr[host]->SEProcs->SideEffectType: 0;

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
    for (host = 0; host < mcon->count; host++)
	if (mcon->retcode[host] > RPC2_ELIMIT)
	    mcon->retcode[host] = rc;
    return;
}


static void SetupPackets(MultiCon *mcon, RPC2_Handle ConnHandleList[],
			 RPC2_Multicast *MCast, struct MEntry *me,
			 SE_Descriptor SDescList[], RPC2_PacketBuffer *Request)
{
    struct CEntry	*thisconn;
    RPC2_PacketBuffer	*thisreq;
    int			host;

    /* allocate and setup the multicast packet first */
    if (MCast) 
    {
	assert(me->CurrentPacket == NULL);
	RPC2_AllocBuffer(Request->Header.BodyLength, &me->CurrentPacket);
	thisreq = me->CurrentPacket;

	/* initialize header fields to defaults, and copy body of request packet */
	rpc2_InitPacket(thisreq, NULL, Request->Header.BodyLength);
        memcpy(thisreq->Body, Request->Body, Request->Header.BodyLength);

	/* complete non-default header fields */
	thisreq->Header.RemoteHandle = MCast->Mgroup;
	thisreq->Header.LocalHandle = 0;		    /* checked at server */
	thisreq->Header.SeqNumber = me->NextSeqNumber;
	thisreq->Header.Opcode = Request->Header.Opcode;    /* set by client */
	thisreq->Header.SubsysId = me->SubsysId;
	if (me->SecurityLevel == RPC2_HEADERSONLY ||
	    me->SecurityLevel == RPC2_SECURE)
	    thisreq->Header.Flags = (RPC2_MULTICAST | RPC2_ENCRYPTED);
	else
	    thisreq->Header.Flags = (RPC2_MULTICAST);
    }

    /* allocate and setup mcon->count request packets */
    /* we won't send on bad connections, so don't bother to set them up */
    for (host = 0; host < mcon->count; host++)
	if (mcon->retcode[host] > RPC2_ELIMIT)
	    {
	    RPC2_AllocBuffer(Request->Header.BodyLength, &mcon->req[host]);

	    /* preserve address of allocated packet */
	    /* preq[host] will be the packet actually sent over the wire */
	    thisreq = mcon->preq[host] = mcon->req[host];
	    thisconn = mcon->ceaddr[host];

	    /* initialize header fields to defaults, and copy body of request packet */
	    rpc2_InitPacket(thisreq, thisconn, Request->Header.BodyLength);
	    memcpy(thisreq->Body, Request->Body, Request->Header.BodyLength);

	    /* complete non-default header fields */
	    thisreq->Header.SeqNumber = thisconn->NextSeqNumber;
	    thisreq->Header.Opcode = Request->Header.Opcode;	/* set by client */
	    thisreq->Header.BindTime = thisconn->RTT >> RPC2_RTT_SHIFT;
	    if (thisconn->RTT && thisreq->Header.BindTime == 0)
		    thisreq->Header.BindTime = 1;  /* ugh. zero is overloaded. */
	    /* For multicast, set MULTICAST flag even though these packets will only
	       ever be sent out as retries on the point-to-point channels.  This
	       is to help servers maintain correct multicast sequence numbers. */
	    if (thisconn->SecurityLevel == RPC2_HEADERSONLY ||
		thisconn->SecurityLevel == RPC2_SECURE)
		thisreq->Header.Flags = ((MCast ? RPC2_MULTICAST : 0) | RPC2_ENCRYPTED);
	    else
		thisreq->Header.Flags =  (MCast ? RPC2_MULTICAST : 0);
	    }

    /* Notify side effect routine, if any. */
    if (SDescList != NULL)
	{
	/* We have already verified that all connections have the same side-effect type (or none), */
	/* so we can simply invoke the procedure corresponding to the first GOOD connection. */
	thisconn = 0;
	for (host = 0; host < mcon->count; host++)
	    if (mcon->retcode[host] > RPC2_ELIMIT)
		{
		thisconn = mcon->ceaddr[host];
		break;
		}
	if (thisconn && thisconn->SEProcs && thisconn->SEProcs->SE_MultiRPC1)
	    {
	    RPC2_PacketBuffer   *savedmcpkt = (MCast ? me->CurrentPacket : NULL);
	    long		    *savedretcode;
	    assert((savedretcode = (long *)malloc(mcon->count * sizeof(long))) != NULL);
	    memcpy(savedretcode, mcon->retcode, mcon->count * sizeof(long));

	    /* N.B.  me->state is not set yet, so the se routine should NOT look at it. */
	    (*thisconn->SEProcs->SE_MultiRPC1)(mcon->count, ConnHandleList, MCast, SDescList, mcon->preq, mcon->retcode);
	    for (host = 0; host < mcon->count; host++)
		if (mcon->retcode[host] != savedretcode[host])
		    {
		    if (mcon->retcode[host] == RPC2_SUCCESS)
			mcon->retcode[host] = savedretcode[host];	/* restore to pre-SE code */
		    else if (mcon->retcode[host] > RPC2_FLIMIT)
			{
			SetState(mcon->ceaddr[host], C_THINK);	/* reset connection state */
			mcon->retcode[host] = RPC2_SEFAIL1;
			}
		    else
			{
			rpc2_SetConnError(mcon->ceaddr[host]);
			mcon->retcode[host] = RPC2_SEFAIL2;
			}
		    }
	    free(savedretcode);
	    if (savedmcpkt != NULL && savedmcpkt != me->CurrentPacket)
		RPC2_FreeBuffer(&savedmcpkt);   /* multicast packet reallocated; free old one */
	    }
	}

    /* complete setup of the multicast packet */
    if (MCast)
	{
	thisreq = me->CurrentPacket;

	/* NO socket listener entry for the multicast packet */

	/* convert to network order */
	rpc2_htonp(thisreq);

	/* Encrypt with MULTICAST key as appropriate. */
        switch ((int) me->SecurityLevel)
	    {
	    case RPC2_OPENKIMONO:
	    case RPC2_AUTHONLY:
		break;
		
	    case RPC2_HEADERSONLY:
		rpc2_Encrypt((char *)&thisreq->Header.BodyLength,
				(char *)&thisreq->Header.BodyLength,
				sizeof(struct RPC2_PacketHeader) - 4*sizeof(RPC2_Integer),
				me->SessionKey, me->EncryptionType);
		break;
		
	    case RPC2_SECURE:
		rpc2_Encrypt((char *)&thisreq->Header.BodyLength,
				(char *)&thisreq->Header.BodyLength,
				thisreq->Prefix.LengthOfPacket - 4*sizeof(RPC2_Integer),
				me->SessionKey, me->EncryptionType);
		break;

	    default:	assert(FALSE);
	    }
	}

    /* complete setup of the individual packets */
    /* we won't send on bad connections, so don't bother to set them up */
    for (host = 0; host < mcon->count; host++)
	if (mcon->retcode[host] > RPC2_ELIMIT)
	    {
	    thisconn = mcon->ceaddr[host];
	    thisreq = mcon->preq[host];

	    /* create call entry */
	    mcon->slarray[host] = rpc2_AllocSle(OTHER, thisconn);
	    mcon->slarray[host]->TElem.BackPointer =
		(char *)mcon->slarray[host];

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

    mcon = (MultiCon *) malloc(sizeof(MultiCon));
    assert(mcon);

    mcon->count = count;
    mcon->slarray = (struct SL_Entry **)calloc(count+2, sizeof(struct SL_Entry *));
    assert(mcon->slarray);

    mcon->req = (RPC2_PacketBuffer **)calloc(count, sizeof(RPC2_PacketBuffer *));
    assert(mcon->req);

    mcon->preq = (RPC2_PacketBuffer **)calloc(count, sizeof(RPC2_PacketBuffer *));
    assert(mcon->preq);

    mcon->ceaddr = (struct CEntry **)calloc(count, sizeof(struct CEntry *));
    assert(mcon->ceaddr);

    mcon->retcode = (long *) malloc(count * sizeof(long));
    assert(mcon->retcode);
    for (i = 0; i < count; i++)
	mcon->retcode[i] = RPC2_ABANDONED;

    return(mcon);
}

/* deallocate buffers and free allocated arrays */
void FreeMultiCon(MultiCon *mcon)
{
    int i;

    for (i = 0; i < mcon->count; i++) {
	if(mcon->slarray[i])
	    rpc2_FreeSle(&mcon->slarray[i]);
	if (mcon->req[i]) {
	  if (mcon->req[i] != mcon->preq[i])
	    RPC2_FreeBuffer(&mcon->preq[i]);/* packet allocated by SE routine */
          RPC2_FreeBuffer(&mcon->req[i]);
	}
	if (mcon->ceaddr[i])
	    LWP_NoYieldSignal((char *)mcon->ceaddr[i]);
    }

    free(mcon->retcode);
    free(mcon->ceaddr);
    free(mcon->preq);
    free(mcon->req);
    free(mcon->slarray);
    free(mcon);
}

static void cleanup(struct MEntry *me)
{
    SetState(me, C_THINK);
    LWP_NoYieldSignal((char *)me);
    if (me->CurrentPacket)
    {
	RPC2_FreeBuffer(&me->CurrentPacket);	/* release request packet */
	me->CurrentPacket = NULL;
    }
}


	/* MultiRPC version */
static long mrpc_SendPacketsReliably(
    MultiCon *mcon,
    RPC2_Handle ConnHandleList[],	/* array of connection ids */
    RPC2_Multicast *MCast,		/* NULL or pointer to multicast
					   information */
    struct MEntry *me,			/* used if MCast is non-NULL */
    ARG_INFO *ArgInfo,			/* Structure of client information
					   (built in MakeMulti) */
    SE_Descriptor SDescList[],		/* array of side effect descriptors */
    long (*UnpackMulti)(),		/* pointer to unpacking routine */
    struct timeval *TimeOut)		/* client specified timeout */
{
    struct SL_Entry *slp;
    RPC2_PacketBuffer *preply, **Reply;  /* RPC2 Response buffers */
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
	MSend_Cleanup(mcon, SDescList, TimeOut, pcon);\
	return(rc);\
	}

    say(0, RPC2_DebugLevel, "mrpc_SendPacketsReliably()\n");

    TR_MSENDRELIABLY();

    /* find a context */
    /* the packet_con management should be redone to ensure that allocation never fails! */
    pcon = InitPacketCon(mcon->count);
/*
    if((pcon = InitPacketCon(mcon->count)) == NULL)
	for(i = 0; i < mcon->count; i++)
	    {
	    if (mcon->slarray[i] == NULL) continue;
	    if ((*UnpackMulti)(mcon->count, ConnHandleList, ArgInfo, NULL, RPC2_FAIL, i) == -1) 
		return(RPC2_FAIL);
	    }
*/

    if (TimeOut) {	    /* create a time bomb */
	slp = mcon->slarray[mcon->count];    /* Tag assumed to be TIMEENTRY */
	rpc2_ActivateSle(slp, TimeOut);
    }

    timestamp = rpc2_MakeTimeStamp();
    say(9, RPC2_DebugLevel, "Sending initial packets at time %ld\n", timestamp);

    /* Do an initial send of packets on all good connections */
    /* for estimating the effiency of the calculation */
    /* should separate this into separate LWP for efficiency */
    for (thispacket = mcon->count - 1; thispacket >= 0; thispacket--)
    {
	/* initialize permuted index array */
	pcon->indexlist[thispacket] = thispacket;
	
	slp = mcon->slarray[thispacket];

	if (!slp) /* something is wrong with connection - don't send packet */
	{
	    if (mcon->retcode[thispacket] > RPC2_ELIMIT)
		mcon->retcode[thispacket] = RPC2_FAIL;
	    exchange(pcon, thispacket);
	    busy++;
	    continue;
	}
	pcon->pending[goodpackets++] = slp;    /* build array of good packets */

	/* send the packet and activate socket listener entry */
	/* if we are multicasting, don't send the packet, but activate the sle's */
	if (!MCast)
	    {
	    /* offer control to Socket Listener every 32 packets to prevent buffer overflow */
	    if ((packets++ & 0x1f) && (packets < pcon->count - 6)) {
	       LWP_DispatchProcess();
	       timestamp = rpc2_MakeTimeStamp();
            }

	    mcon->preq[thispacket]->Header.TimeStamp = htonl(timestamp);
	    rpc2_XmitPacket(rpc2_RequestSocket, mcon->preq[thispacket],
			    &mcon->ceaddr[thispacket]->PeerHost,
			    &mcon->ceaddr[thispacket]->PeerPort);
	    }

        if (rpc2_Bandwidth) 
	    rpc2_ResetLowerLimit(mcon->ceaddr[thispacket],
				 mcon->preq[thispacket]);

	slp->RetryIndex = 1;
	rpc2_ActivateSle(slp, &mcon->ceaddr[thispacket]->Retry_Beta[1]);
    }

    /* add Timer entry */
    pcon->pending[goodpackets++] = mcon->slarray[mcon->count];
    pcon->pending[goodpackets] = NULL;

    if (busy == mcon->count)
	EXIT_MRPC_SPR(RPC2_FAIL)		    /* no packets were sent */

    /* send multicast packet here. */
    if (MCast)
	{
	say(9, RPC2_DebugLevel, "Sending multicast packet at time %d\n", rpc2_time());
	me->CurrentPacket->Header.TimeStamp = htonl(rpc2_MakeTimeStamp());
	rpc2_XmitPacket(rpc2_RequestSocket, me->CurrentPacket, &me->IPMHost, &me->IPMPort);
	me->NextSeqNumber += 2;	/* blindly increment the multicast sequence number??? */
	}

    if (busy == 0) finalrc = RPC2_SUCCESS;	    /* RPC2_FAIL if anything goes wrong */
    else finalrc = RPC2_FAIL;

    do {
	hopeleft = 0;
	/* wait for SocketListener to tap me on the shoulder */
	LWP_MwaitProcess(1, (char **)pcon->pending);

	if (TimeOut && mcon->slarray[mcon->count]->ReturnCode == TIMEOUT)
	    /* Overall timeout expired: clean up state and quit */
	    EXIT_MRPC_SPR(RPC2_TIMEOUT)

	/* the loop below looks at a decreasing list of sl entries using the permuted index array for sorting */
	for(i = 0;  i < pcon->count ; i++)
	{
	    thispacket = pcon->indexlist[i];
	    c_entry = mcon->ceaddr[thispacket];
	    slp = mcon->slarray[thispacket];
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
		    mcon->retcode[thispacket] = preply->Header.ReturnCode;

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
				if ((*UnpackMulti)(mcon->count, ConnHandleList, ArgInfo, preply, RPC2_SEFAIL2, thispacket) == -1)
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
		        if ((*UnpackMulti)(mcon->count, ConnHandleList, ArgInfo, preply, RPC2_SEFAIL1, thispacket) == -1)
			    /* enough responses, return */
			    EXIT_MRPC_SPR(finalrc)
			else
			    /* continue waiting for responses */
			    break;
			}
		    /* RPC2_SUCCESS if ARRIVED and SE's are OK */
		    if ((*UnpackMulti)(mcon->count, ConnHandleList, ArgInfo, preply, RPC2_SUCCESS, thispacket) == -1)
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
    		    mcon->retcode[thispacket] = RPC2_NAKED;
		    /* call side-effect routine, and ignore result */
		    if (GOODSEDLE(thispacket) &&
			 c_entry->SEProcs != NULL && c_entry->SEProcs->SE_MultiRPC2 != NULL)
			(*c_entry->SEProcs->SE_MultiRPC2)(ConnHandleList[thispacket], &(SDescList[thispacket]), NULL);
		    if ((*UnpackMulti)(mcon->count, ConnHandleList, ArgInfo, NULL, RPC2_NAKED, thispacket) == -1)
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
			mcon->retcode[thispacket] = RPC2_DEAD;

			/* call side-effect routine, and ignore result */
			if (GOODSEDLE(thispacket) && c_entry->SEProcs != NULL && c_entry->SEProcs->SE_MultiRPC2 != NULL)
			    (*c_entry->SEProcs->SE_MultiRPC2)(ConnHandleList[thispacket], &(SDescList[thispacket]), NULL);

			if ((*UnpackMulti)(mcon->count, ConnHandleList, ArgInfo, NULL, RPC2_DEAD, thispacket) == -1)
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
		    say(9, RPC2_DebugLevel, "Sending retry %ld at %d on 0x%lx (timeout %ld.%06ld)\n", slp->RetryIndex, rpc2_time(), c_entry->UniqueCID, tout->tv_sec, tout->tv_usec);
		    mcon->preq[thispacket]->Header.Flags = htonl((ntohl(mcon->preq[thispacket]->Header.Flags) | RPC2_RETRY));
		    mcon->preq[thispacket]->Header.TimeStamp = htonl(rpc2_MakeTimeStamp());
		    rpc2_Sent.Retries += 1;	/* RPC retries are currently NOT multicasted! -JJK */
		    rpc2_XmitPacket(rpc2_RequestSocket, mcon->preq[thispacket], &c_entry->PeerHost, &c_entry->PeerPort);
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
    int i;

    /* allocate new context */
    pcon = (PacketCon *) malloc(sizeof(PacketCon));
    assert(pcon);

    pcon->count = count;
    pcon->pending = (struct SL_Entry **)calloc(count+2, sizeof(struct SL_Entry *));
    assert(pcon->pending);

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

    pcon->count--;
    if (cur_ind == pcon->count) return(cur_ind);
    tmp = pcon->indexlist[cur_ind];
    pcon->indexlist[cur_ind] = pcon->indexlist[pcon->count];
    pcon->indexlist[pcon->count] = tmp;
    return(cur_ind - 1);
}


/* Clean up state before exiting mrpc_SendPacketsReliably */
static void MSend_Cleanup(MultiCon *mcon, SE_Descriptor SDescList[],
			  struct timeval *Timeout, PacketCon *pcon)
{
    long thispacket, i;
    struct SL_Entry *slp;

    for (i = 0; i < pcon->count; i++)
    {
	thispacket = pcon->indexlist[i];
	slp = mcon->slarray[thispacket];
	TM_Remove(rpc2_TimerQueue, &slp->TElem);

	/* Call side-effect routine and increment connection sequence number for abandoned requests */
	if (GOODSEDLE(i) && mcon->ceaddr[i]->SEProcs && mcon->ceaddr[i]->SEProcs->SE_MultiRPC2)
	    (*mcon->ceaddr[i]->SEProcs->SE_MultiRPC2)(mcon->ceaddr[i]->UniqueCID, &SDescList[i], NULL);
	rpc2_IncrementSeqNumber(mcon->ceaddr[thispacket]);
	SetState(mcon->ceaddr[thispacket], C_THINK);
/*	LWP_NoYieldSignal((char *)mcon->ceaddr[thispacket]);*/
    }

    if (Timeout && mcon->slarray[mcon->count]->ReturnCode == WAITING)
    {
	/* delete  time bomb if it has not fired  */
	slp = mcon->slarray[mcon->count];    /* Tag assumed to be TIMEENTRY */
	TM_Remove(rpc2_TimerQueue, &slp->TElem);
    }
    FreePacketCon(pcon);
}
