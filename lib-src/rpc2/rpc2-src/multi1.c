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
		int		    busy;
		struct CEntry	    **ceaddr;
		RPC2_PacketBuffer   **req;
		RPC2_PacketBuffer   **preq;
		struct SL_Entry	    **slarray;
		long		    *retcode;
		} MultiCon;

typedef struct	{
		long		    count;
		int		    busy;
		struct SL_Entry	    **slarray;
		RPC2_PacketBuffer   **Reply;
		long		    *indexlist;
		} PacketCon;

static void SetupConns();
static void SetupPackets();
#define MaxLWPs	  8    /* lwp's enabled to make simultaneous MultiRPC calls */
static MultiCon *mcon[MaxLWPs] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static MultiCon *get_multi_con();
static void cleanup();
static long mrpc_SendPacketsReliably();
static PacketCon *spcon[MaxLWPs] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static PacketCon *get_packet_con();
static void MSend_Cleanup();
static void mrpc_ProcessRC(long *in, long *out, int howmany);
static inline long  EXIT_MRPC(long code, long *retcode, long *RCList, 
			      int HowMany, struct RPC2_PacketBuffer **req, 
			      RPC2_PacketBuffer **preq, 
			      struct SL_Entry **slarray, MultiCon *context, 
			      struct MEntry *me);

#define GOODSEDLE(i)  (SDescList != NULL && SDescList[i].Tag != OMITSE)


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
    struct CEntry **ceaddr;
    RPC2_PacketBuffer **req;	/* server specific copies of request packet */
    RPC2_PacketBuffer **preq;		/* keep original buffers for reference */
    struct SL_Entry **slarray;
    MultiCon *context;
    int	host;
    long *retcode;
    struct MEntry *me = NULL;
    int SomeConnsOK;
    long rc = 0;


    rpc2_Enter();
    say(0, RPC2_DebugLevel, "Entering RPC2_MultiRPC\n");
	    
    TR_MULTI();

    /* perform sanity checks */
    assert(Request->Prefix.MagicNumber == OBJ_PACKETBUFFER);

    /* get context pointer */
    /* the multi_con management should be redone to ensure that 
       allocation never fails! */
    assert((context = get_multi_con(HowMany)) != NULL);

    /* alias the context arrays for convenience */
    slarray = context->slarray;
    req = context->req;
    preq = context->preq;
    ceaddr = context->ceaddr;
    retcode = context->retcode;

    /* initialize slarray, request array and the return code list */
    memset(slarray, 0, sizeof(char *) * (HowMany + 2));
    memset(req, 0, sizeof(char *) * HowMany);
    for (host = 0; host < HowMany; host++)
	retcode[host] = RPC2_ABANDONED;

    /* setup for multicast */
    if (MCast) {
	rc = SetupMulticast(MCast, &me, HowMany, ConnHandleList);
	if (rc != RPC2_SUCCESS) {
	    for (host = 0; host < HowMany; host++) {
		retcode[host] = rc;
	    }
	    return EXIT_MRPC(rc, retcode, RCList, HowMany, req, preq, 
			     slarray, context, me);
	}
    }

    /*  verify and set connection state */
    SetupConns(HowMany, ConnHandleList, ceaddr, MCast,
		me, SDescList, retcode);

    /* prepare all of the packets */
    SetupPackets(HowMany, ConnHandleList, ceaddr, slarray, MCast,
		 me, req, preq, retcode, SDescList, Request);

    /* call UnpackMulti on all bad connections; 
       if there are NO good connections, exit */
    SomeConnsOK = FALSE;
    for (host = 0; host < HowMany; host++) {
	if (retcode[host] > RPC2_ELIMIT) {
	    SomeConnsOK = TRUE;
	} else {
	    if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, NULL, 
			       retcode[host], host) == -1) {
		return EXIT_MRPC(rc, retcode, RCList, HowMany, req, preq, 
				 slarray, context, me);
	    }
	}
    }

    if (!SomeConnsOK) {
	/* NO usable connections */
	return EXIT_MRPC(rc, retcode, RCList, HowMany, req, preq, slarray, 
			 context, me);
    }

    /* finally safe to update the state of the good connections */
    for	(host =	0; host	< HowMany; host++) { 
	if (retcode[host] > RPC2_ELIMIT) { 
	    SetState(ceaddr[host], C_AWAITREPLY);
	}
    }

    /* send packets and await replies */
    say(9, RPC2_DebugLevel, "Sending requests\n");
    /* allocate timer entry */
    slarray[HowMany] = rpc2_AllocSle(OTHER, NULL);  
    slarray[HowMany + 1] = NULL;
    rc = mrpc_SendPacketsReliably(HowMany, ConnHandleList, MCast,
				   me, ceaddr, slarray, preq,
				   ArgInfo, SDescList, UnpackMulti,
				   BreathOfLife, retcode);
    /* free timer entry */
    rpc2_FreeSle(&(slarray[HowMany]));	
    
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
    return EXIT_MRPC(rc, retcode, RCList, HowMany, req, preq, slarray, 
		     context, me);
}


/* easier to manage than the former macro definition */
static inline long
EXIT_MRPC(long code, long *retcode, long *RCList, int HowMany,
	  struct RPC2_PacketBuffer **req, RPC2_PacketBuffer **preq, 
	  struct SL_Entry **slarray, MultiCon *context, 
	  struct MEntry *me)
{
    if (RCList) {
	mrpc_ProcessRC(retcode, RCList, HowMany);
    }
    cleanup(HowMany, req, preq, slarray, context, me);
    rpc2_Quit(code);
}

/* copy arguments into the return code lists, possibly translating 
   error codes */
static void mrpc_ProcessRC(long *in, long *out, int howmany) 
{
#ifdef ERRORTR
    int host;
    for ( host = 0 ; host < howmany ; host++ )
	if ( in[host] > 0 ) {
	    out[host] = RPC2_R2SError(in[host]);
	} else {
	    out[host] = in[host];
	}
#else
    memcpy(out, in, sizeof(long) * howmany);
#endif 
}

static void SetupConns(HowMany, ConnHandleList, ceaddr, MCast, me, SDescList, retcode)
    int		    HowMany;
    RPC2_Handle	    ConnHandleList[];
    struct CEntry   **ceaddr;
    RPC2_Multicast  *MCast;
    struct MEntry   *me;
    SE_Descriptor   SDescList[];
    long	    *retcode;
    {
    struct CEntry   *thisconn;
    int		    host;
    long	    setype = -1;	    /* -1 ==> first time through loop */

    /* verify the handles; don't update the connection state of the "good" connections yet */
    host = 0;
    while (TRUE)
	{
	if (host == HowMany) break;

	if ((thisconn = ceaddr[host] = rpc2_GetConn(ConnHandleList[host])) == NULL)
	    {
	    retcode[host] = RPC2_NOCONNECTION;
	    host++;
	    continue;
	    }
	assert(thisconn->MagicNumber == OBJ_CENTRY);
 	if (!TestRole(thisconn, CLIENT))
	    {
	    retcode[host] = RPC2_FAIL;
	    host++;
	    continue;
	    }
	switch ((int) (thisconn->State  & 0x0000ffff))
	    {
	    case C_HARDERROR:
		retcode[host] = RPC2_FAIL;
		host++;
		break;
	    
	    case C_THINK:
		/* wait to update connection state */
		host++;
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
		    else
			{
			/* can't continue if ANY connections are busy */
			for (host = 0; host < HowMany; host++)
			    if (retcode[host] > RPC2_ELIMIT)
				retcode[host] = RPC2_MGRPBUSY;
			return;
			}
		    }
/*		else*/
/*		    {*/
/*		    retcode[host] = RPC2_CONNBUSY;*/
/*		    break;*/
/*		    }*/
	    }
	}

    /* insist that all connections have the same side-effect type (or none) */
    for (host = 0; host < HowMany; host++)
	if (retcode[host] > RPC2_ELIMIT)
	    {
	    long this_setype = ceaddr[host]->SEProcs ? 
		ceaddr[host]->SEProcs->SideEffectType: 0;

	    if (setype == -1)		/* first time through loop */
		setype = this_setype;
	    else
		if (this_setype != setype)
		{
		    /* reuse host variable! */
		    for (host = 0; host < HowMany; host++)
			if (retcode[host] > RPC2_ELIMIT)
			    retcode[host] = RPC2_FAIL;  /* better return code ? */
		    return;
		}
	    }

    /* We delay updating the state of the "good" connections until we know */
    /* FOR SURE that mrpc_SendPacketsReliably() will be called. */
    }


static void SetupPackets(HowMany, ConnHandleList, ceaddr, slarray, MCast,
		    me, req, preq, retcode, SDescList, Request)
    int			HowMany;
    RPC2_Handle		ConnHandleList[];
    struct CEntry	*ceaddr[];
    struct SL_Entry	*slarray[];
    RPC2_Multicast	*MCast;
    struct MEntry	*me;
    RPC2_PacketBuffer	*req[];
    RPC2_PacketBuffer	*preq[];
    long		retcode[];
    SE_Descriptor	SDescList[];
    RPC2_PacketBuffer	*Request;
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

    /* allocate and setup HowMany request packets */
    /* we won't send on bad connections, so don't bother to set them up */
    for (host = 0; host < HowMany; host++)
	if (retcode[host] > RPC2_ELIMIT)
	    {
	    RPC2_AllocBuffer(Request->Header.BodyLength, &req[host]);

	    /* preserve address of allocated packet */
	    /* preq[host] will be the packet actually sent over the wire */
	    thisreq = preq[host] = req[host];
	    thisconn = ceaddr[host];

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
	for (host = 0; host < HowMany; host++)
	    if (retcode[host] > RPC2_ELIMIT)
		{
		thisconn = ceaddr[host];
		break;
		}
	assert(thisconn != 0);
	if (thisconn->SEProcs != NULL && thisconn->SEProcs->SE_MultiRPC1 != NULL)
	    {
	    RPC2_PacketBuffer   *savedmcpkt = (MCast ? me->CurrentPacket : NULL);
	    long		    *savedretcode;
	    assert((savedretcode = (long *)malloc(HowMany * sizeof(long))) != NULL);
	    memcpy(savedretcode, retcode, HowMany * sizeof(long));

	    /* N.B.  me->state is not set yet, so the se routine should NOT look at it. */
	    (*thisconn->SEProcs->SE_MultiRPC1)(HowMany, ConnHandleList, MCast, SDescList, preq, retcode);
	    for (host = 0; host < HowMany; host++)
		if (retcode[host] != savedretcode[host])
		    {
		    if (retcode[host] == RPC2_SUCCESS)
			retcode[host] = savedretcode[host];	/* restore to pre-SE code */
		    else if (retcode[host] > RPC2_FLIMIT)
			{
			SetState(ceaddr[host], C_THINK);	/* reset connection state */
			retcode[host] = RPC2_SEFAIL1;
			}
		    else
			{
			rpc2_SetConnError(ceaddr[host]);
			retcode[host] = RPC2_SEFAIL2;
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
    for (host = 0; host < HowMany; host++)
	if (retcode[host] > RPC2_ELIMIT)
	    {
	    thisconn = ceaddr[host];
	    thisreq = preq[host];

	    /* create call entry */
	    slarray[host] = rpc2_AllocSle(OTHER, thisconn);
	    slarray[host]->TElem.BackPointer = (char *)slarray[host];

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

	
/* Get a free context from the pool or allocate an unused one */
/* locking is not a problem since LWPs are nonpreemptive */
static MultiCon *get_multi_con(count)
long count;
{
    long buffsize;
    MultiCon *thiscon;
    int i;

    buffsize = count * sizeof(char *);
    for(i = 0; i < MaxLWPs; i++)
	{
	thiscon = mcon[i];
	if (thiscon == NULL)		/* allocate new context */
	    {
	    assert((mcon[i] = thiscon = (MultiCon *) malloc(sizeof(MultiCon))) != NULL);
	    thiscon->busy = 1;
	    thiscon->count = count;
	    assert((thiscon->slarray = (struct SL_Entry **) malloc(buffsize + (2 * sizeof(char *)))) != NULL);
	    assert((thiscon->req = (RPC2_PacketBuffer **) malloc(buffsize)) != NULL);
	    assert((thiscon->preq = (RPC2_PacketBuffer **) malloc(buffsize)) != NULL);
	    assert((thiscon->ceaddr = (struct CEntry **) malloc(buffsize)) != NULL);
	    assert((thiscon->retcode = (long *) malloc(count * sizeof(long))) != NULL);
	    return(thiscon);
	    }
	if (thiscon->busy) continue;	/* in use; examine next context */
	if (thiscon->count >= count)	/* use this context */
	    {
	    thiscon->busy = 1;
	    return(thiscon);
	    }
	else				/* allocate more space */
	    {
	    thiscon->busy = 1;
	    thiscon->count = count;
	    assert((thiscon->slarray = (struct SL_Entry **) realloc(thiscon->slarray, buffsize + (2 * sizeof(char *)))) != NULL);
	    assert((thiscon->req = (RPC2_PacketBuffer **) realloc(thiscon->req, buffsize)) != NULL);
	    assert((thiscon->preq = (RPC2_PacketBuffer **) realloc(thiscon->preq, buffsize)) != NULL);
	    assert((thiscon->ceaddr = (struct CEntry **) realloc(thiscon->ceaddr, buffsize)) != NULL);
	    assert((thiscon->retcode = (long *) realloc(thiscon->retcode, count * sizeof(long))) != NULL);
	    return(thiscon);
	    }
	}
	return(NULL);
}


/* deallocate buffers and free allocated arrays */
static void cleanup(curhost, req, preq, slarray, context, me)
long curhost;
RPC2_PacketBuffer *req[], *preq[];
struct SL_Entry *slarray[];
MultiCon *context;
struct MEntry *me;
{
    int i;

    for (i = 0; i < curhost; i++) {
	if(slarray[i] != NULL)
	    rpc2_FreeSle(&slarray[i]);
	if (req[i] != NULL) {
	  if (req[i] != preq[i])
	    RPC2_FreeBuffer(&preq[i]);    /* release packet allocated by SE routine */
          RPC2_FreeBuffer(&req[i]);
	}
	if (context->ceaddr[i] != NULL)
	    LWP_NoYieldSignal((char *)context->ceaddr[i]);
    }

    context->busy = 0;

    if (me)
	{
	SetState(me, C_THINK);
	LWP_NoYieldSignal((char *)me);
	if (me->CurrentPacket)
	    {
	    RPC2_FreeBuffer(&me->CurrentPacket);	/* release multicast request packet */
	    me->CurrentPacket = NULL;
	    }
	}
}


	/* MultiRPC version */
static long mrpc_SendPacketsReliably(
    int HowMany,			/* how many servers to send to */
    RPC2_Handle ConnHandleList[],	/* array of connection ids */
    RPC2_Multicast *MCast,		/* NULL or pointer to multicast
					   information */
    struct MEntry *me,			/* used if MCast is non-NULL */
    struct CEntry *ConnArray[],		/* Array of HowMany CEntry structures */
    struct SL_Entry *SLArray[],		/* Array of (HowMany+2) SL entry
					   pointers: last is set to NULL, */
					/* last but one of type TIMER  */
    RPC2_PacketBuffer *PacketArray[],	/* Array of HowMany packet buffer
					   pointers */
    ARG_INFO *ArgInfo,			/* Structure of client information
					   (built in MakeMulti) */
    SE_Descriptor SDescList[],		/* array of side effect descriptors */
    long (*UnpackMulti)(),		/* pointer to unpacking routine */
    struct timeval *TimeOut,		/* client specified timeout */
    long *RCList)			/* array to put return codes */
    {
    struct SL_Entry *slp, **slarray;
    RPC2_PacketBuffer *preply, **Reply;  /* RPC2 Response buffers */
    struct CEntry *c_entry;
    long finalrc, secode = 0;
    long thispacket, hopeleft;
    long *indexlist, i;			/* array for permuted indices */
    struct timeval *tout;
    int packets = 1; 			/* packet counter for LWP yield */
    int busy = 0;
    int goodpackets = 0;		/* packets with good connection state */
    long finalind = HowMany - 1;
    PacketCon *context;
    long exchange();
    unsigned long timestamp;

#define	EXIT_MRPC_SPR(rc)\
	{\
	MSend_Cleanup(SLArray, ConnArray, SDescList, indexlist, finalind, HowMany, TimeOut, context);\
	return(rc);\
	}

    say(0, RPC2_DebugLevel, "mrpc_SendPacketsReliably()\n");

    TR_MSENDRELIABLY();

    /* find a context */
    /* the packet_con management should be redone to ensure that allocation never fails! */
    assert((context = get_packet_con(HowMany)) != NULL);
/*
    if((context = get_packet_con(HowMany)) == NULL)
	for(i = 0; i < HowMany; i++)
	    {
	    if (SLArray[i] == NULL) continue;
	    if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, NULL, RPC2_FAIL, i) == -1) 
		return(RPC2_FAIL);
	    }
*/

    if (TimeOut != NULL)			    /* create a time bomb */
	{
	slp = SLArray[HowMany];			    /* Tag assumed to be TIMEENTRY */
	rpc2_ActivateSle(slp, TimeOut);
	}

    /* alias context arrays for convenience */
    slarray = context->slarray;
    Reply = context->Reply;
    indexlist = context->indexlist;

    /* initialize reply pointers and return codes */
    memset(Reply, 0, sizeof(char *) * HowMany);

    timestamp = rpc2_MakeTimeStamp();
    say(9, RPC2_DebugLevel, "Sending initial packets at time %ld\n", timestamp);

    /* Do an initial send of packets on all good connections */
    /* for estimating the effiency of the calculation */
    /* should separate this into separate LWP for efficiency */
    for (thispacket = HowMany - 1; thispacket >= 0; thispacket--)
	{
	indexlist[thispacket] = thispacket;	    /* initialize permuted index array */
	
	slp = SLArray[thispacket];

	if (slp == NULL)	    /* something is wrong with connection - don't send packet */
	    {
	    if (RCList && RCList[thispacket] > RPC2_ELIMIT) RCList[thispacket] = RPC2_FAIL;
	    exchange(indexlist, thispacket, finalind--);	
	    busy++;
	    continue;
	    }
	slarray[goodpackets++] = slp;		    /* build array of good packets */

	/* send the packet and activate socket listener entry */
	/* if we are multicasting, don't send the packet, but activate the sle's */
	if (!MCast)
	    {
	    /* offer control to Socket Listener every 32 packets to prevent buffer overflow */
	    if ((packets++ & 0x1f) && (packets < finalind - 5)) {
	       LWP_DispatchProcess();
	       timestamp = rpc2_MakeTimeStamp();
            }

	    PacketArray[thispacket]->Header.TimeStamp = htonl(timestamp);
	    rpc2_XmitPacket(rpc2_RequestSocket, PacketArray[thispacket],
		    &(ConnArray[thispacket]->PeerHost), &(ConnArray[thispacket]->PeerPort));
	    }

        if (rpc2_Bandwidth) 
	    rpc2_ResetLowerLimit(ConnArray[thispacket], PacketArray[thispacket]);

	slp->RetryIndex = 1;
	rpc2_ActivateSle(slp, &ConnArray[thispacket]->Retry_Beta[1]);
	}

    slarray[goodpackets++] = SLArray[HowMany];	    /* add Timer entry */
    slarray[goodpackets] = NULL;

    if (busy == HowMany)
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

    do
	{
	hopeleft = 0;
	/* wait for SocketListener to tap me on the shoulder */
	LWP_MwaitProcess(1, (char **)slarray);

	if (TimeOut != NULL && SLArray[HowMany]->ReturnCode == TIMEOUT)
	    /* Overall timeout expired: clean up state and quit */
	    EXIT_MRPC_SPR(RPC2_TIMEOUT)

	/* the loop below looks at a decreasing list of sl entries using the permuted index array for sorting */
	for(i = 0;  i <= finalind ; i++)
	    {
	    thispacket = indexlist[i];
	    c_entry = ConnArray[thispacket];
	    slp = SLArray[thispacket];
	    switch(slp->ReturnCode)
		{
		case WAITING:   
		    hopeleft = 1;	/* someday we will be told about this packet */
		    break;		/* switch */

		case ARRIVED:
		    /* know this hasn't yet been processd */
		    say(9, RPC2_DebugLevel, "Request reliably sent on 0x%p\n", c_entry);
		    /* remove current connection from future examination */
		    i = exchange(indexlist, i, finalind--);

		    /* At this point the final reply has been received;
		       SocketListener has already decrypted it. */
		    Reply[thispacket] = preply = slp->Packet;
		    if (RCList) RCList[thispacket] = preply->Header.ReturnCode;

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
		    i = exchange(indexlist, i, finalind--);
		    finalrc = RPC2_FAIL;
    		    if (RCList) RCList[thispacket] = RPC2_NAKED;
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
			i = exchange(indexlist, i, finalind--);
			finalrc = RPC2_FAIL;
			if (RCList) RCList[thispacket] = RPC2_DEAD;

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
		    say(9, RPC2_DebugLevel, "Sending retry %ld at %d on 0x%lx (timeout %ld.%06ld)\n", slp->RetryIndex, rpc2_time(), c_entry->UniqueCID, tout->tv_sec, tout->tv_usec);
		    PacketArray[thispacket]->Header.Flags = htonl((ntohl(PacketArray[thispacket]->Header.Flags) | RPC2_RETRY));
		    PacketArray[thispacket]->Header.TimeStamp = htonl(rpc2_MakeTimeStamp());
		    rpc2_Sent.Retries += 1;	/* RPC retries are currently NOT multicasted! -JJK */
		    rpc2_XmitPacket(rpc2_RequestSocket, PacketArray[thispacket], &c_entry->PeerHost, &c_entry->PeerPort);
		    break;	/* switch */
		    
		default:    /* abort */
		    /* BUSY ReturnCode should never go into switch */
		    assert(FALSE);
		}
	    }
	}
    while (hopeleft);

    EXIT_MRPC_SPR(finalrc)
#undef	EXIT_MRPC_SPR
    }


static PacketCon *get_packet_con(count)
long count;
{
    long buffsize;
    PacketCon *pcon;
    int i;

    buffsize = count * sizeof(char *);
    for(i = 0; i < MaxLWPs; i++)
	{
	pcon = spcon[i];
	if (pcon == NULL)		    /* allocate new context */
	    {
	    assert((spcon[i] = pcon = (PacketCon *) malloc(sizeof(PacketCon))) != NULL);
	    pcon->busy = 1;
	    pcon->count = count;
	    assert((pcon->slarray = (struct SL_Entry **) malloc(buffsize + (2 * sizeof(char *)))) != NULL);
	    assert((pcon->Reply = (RPC2_PacketBuffer **) malloc(buffsize)) != NULL);
	    assert((pcon->indexlist = (long *) malloc(buffsize)) != NULL);
	    return(pcon);
	    }
	if (pcon->busy) continue;
	if (pcon->count >= count)
	    {
	    pcon->busy = 1;
	    return(pcon);
	    }
	else
	    {
	    pcon->busy = 1;
	    pcon->count = count;
	    assert((pcon->slarray = (struct SL_Entry **) realloc(pcon->slarray, buffsize + (2 * sizeof(char *)))) != NULL);
	    assert((pcon->Reply = (RPC2_PacketBuffer **) realloc(pcon->Reply, buffsize)) != NULL);
	    assert((pcon->indexlist = (long *) realloc(pcon->indexlist, buffsize)) != NULL);
	    return(pcon);
	    }
	}
	return(NULL);
}

	
/* exchange two elements of the socket listener element array */
/* returns value for loop counter: decrements it iff elements */
/* are physically exchanged */
long exchange(indexlist, cur_ind, final_ind)
long indexlist[];
int cur_ind, final_ind;
{
    long tmp;

    if (cur_ind == final_ind) return(cur_ind);
    tmp = indexlist[cur_ind];
    indexlist[cur_ind] = indexlist[final_ind];
    indexlist[final_ind] = tmp;
    return(cur_ind - 1);
}


/* Clean up state before exiting mrpc_SendPacketsReliably */
static void MSend_Cleanup(SLArray, ConnArray, SDescList, indexlist, finalind,
	HowMany,Timeout, context)
    struct SL_Entry *SLArray[];
    struct CEntry *ConnArray[];
    SE_Descriptor SDescList[];
    long indexlist[], finalind;
    int HowMany;
    struct timeval *Timeout;
    PacketCon *context;
    {
    long thispacket, i;
    struct SL_Entry *slp;

    for (i = 0; i <= finalind; i++)
	{
	thispacket = indexlist[i];
	slp = SLArray[thispacket];
	TM_Remove(rpc2_TimerQueue, &slp->TElem);

	/* Call side-effect routine and increment connection sequence number for abandoned requests */
	if (GOODSEDLE(i) &&
	    ConnArray[i]->SEProcs != NULL && ConnArray[i]->SEProcs->SE_MultiRPC2 != NULL)
	    (*ConnArray[i]->SEProcs->SE_MultiRPC2)(ConnArray[i]->UniqueCID, &SDescList[i], NULL);
	rpc2_IncrementSeqNumber(ConnArray[thispacket]);
	SetState(ConnArray[thispacket], C_THINK);
/*	LWP_NoYieldSignal((char *)ConnArray[thispacket]);*/
	}

    if (Timeout != NULL && SLArray[HowMany]->ReturnCode == WAITING)
	{
	/* delete  time bomb if it has not fired  */
	slp = SLArray[HowMany];    /* Tag assumed to be TIMEENTRY */
	TM_Remove(rpc2_TimerQueue, &slp->TElem);
	}
    context->busy = 0;
    }
