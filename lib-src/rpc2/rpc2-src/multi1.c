/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2016 Carnegie Mellon University
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

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "cbuf.h"
#include "rpc2.private.h"
#include "trace.h"
#include <rpc2/multi.h>
#include <rpc2/se.h>

extern void rpc2_IncrementSeqNumber();
extern long HandleResult();
extern void rpc2_PrintPacketHeader();
extern long SetupMulticast();

typedef struct {
    struct CEntry *ceaddr;
    RPC2_PacketBuffer *req;
    struct SL_Entry *sle;
    long retcode;
} MultiCon;

typedef struct {
    struct SL_Entry **pending;
    long indexlen;
    long *indexlist;
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
static long exchange(PacketCon *pcon, int cur_ind);
static void MSend_Cleanup(int HowMany, MultiCon *mcon,
                          SE_Descriptor SDescList[], struct timeval *Timeout,
                          PacketCon *pcon);
static inline long EXIT_MRPC(long code, int HowMany, RPC2_Integer *RCList,
                             MultiCon *context);

#define GOODSEDLE(i) (SDescList && SDescList[i].Tag != OMITSE)

long RPC2_MultiRPC(
    IN int HowMany, /* no of connections involved */
    IN RPC2_Handle ConnHandleList[],
    IN RPC2_Integer RCList[], /* NULL or list of per-connection return codes */
    IN RPC2_Multicast *MCast, /* NULL if multicast not used */
    IN RPC2_PacketBuffer *Request, /* Gets clobbered during call: BEWARE */
    IN SE_Descriptor SDescList[], IN long (*UnpackMulti)(),
    IN OUT ARG_INFO *ArgInfo, IN struct timeval *BreathOfLife)
{
    MultiCon *mcon;
    int host;
    int SomeConnsOK;
    long rc = 0;

    rpc2_Enter();
    say(1, RPC2_DebugLevel, "Entering RPC2_MultiRPC\n");

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
    for (host = 0; host < HowMany; host++) {
        if (mcon[host].retcode > RPC2_ELIMIT) {
            SetState(mcon[host].ceaddr, C_AWAITREPLY);
        }
    }

    /* send packets and await replies */
    say(9, RPC2_DebugLevel, "Sending requests\n");
    rc = mrpc_SendPacketsReliably(HowMany, mcon, ConnHandleList, ArgInfo,
                                  SDescList, UnpackMulti, BreathOfLife);

    switch ((int)rc) {
    case RPC2_SUCCESS:
        break;
    case RPC2_TIMEOUT:
    case RPC2_FAIL:
        say(9, RPC2_DebugLevel, "mrpc_SendPacketsReliably()--> %s\n",
            RPC2_ErrorMsg(rc));
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
static inline long EXIT_MRPC(long code, int HowMany, RPC2_Integer *RCList,
                             MultiCon *mcon)
{
    int i;

    /* copy arguments into the return code lists, possibly translating
       error codes */
    if (RCList) {
        for (i = 0; i < HowMany; i++)
            RCList[i] = mcon[i].retcode;
    }

    FreeMultiCon(HowMany, mcon);

    rpc2_Quit(code);
}

static void SetupConns(int HowMany, MultiCon *mcon,
                       RPC2_Handle ConnHandleList[])
{
    struct CEntry *thisconn;
    int host;
    long rc, setype = -1; /* -1 ==> first time through loop */

    /* verify the handles; don't update the connection state of the "good"
     * connections yet */
    for (host = 0; host < HowMany; host++) {
        thisconn = mcon[host].ceaddr = rpc2_GetConn(ConnHandleList[host]);
        if (!thisconn) {
            mcon[host].retcode = RPC2_NOCONNECTION;
            continue;
        }
        assert(thisconn->MagicNumber == OBJ_CENTRY);
        if (!TestRole(thisconn, CLIENT)) {
            mcon[host].retcode = RPC2_FAIL;
            continue;
        }
        switch ((int)(thisconn->State & 0x0000ffff)) {
        case C_HARDERROR:
            mcon[host].retcode = RPC2_FAIL;
            break;

        case C_THINK:
            /* wait to update connection state */
            break;

        default:
            /* This isn't the behavior the manual claims, but it's what I need. -JJK
            */
            {
                if (TRUE /*EnqueueRequest*/) {
                    say(1, RPC2_DebugLevel, "Enqueuing on connection %#x\n",
                        ConnHandleList[host]);
                    LWP_WaitProcess((char *)thisconn);
                    say(1, RPC2_DebugLevel, "Dequeueing on connection %#x\n",
                        ConnHandleList[host]);
                    host = 0; /* !!! restart loop !!! */
                    break;
                } else {
                    /* can't continue if ANY connections are busy */
                    rc = RPC2_MGRPBUSY;
                    goto exit_fail;
                }
            }
        }
    }

    /* insist that all connections have the same side-effect type (or none) */
    for (host = 0; host < HowMany; host++)
        if (mcon[host].retcode > RPC2_ELIMIT) {
            long this_setype = mcon[host].ceaddr->SEProcs ?
                                   mcon[host].ceaddr->SEProcs->SideEffectType :
                                   0;

            if (setype == -1) /* first time through loop */
                setype = this_setype;

            if (this_setype != setype) {
                rc = RPC2_FAIL; /* better return code ? */
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
    struct CEntry *thisconn;
    RPC2_PacketBuffer *thisreq;
    int host;

    /* allocate and setup HowMany request packets */
    /* we won't send on bad connections, so don't bother to set them up */
    for (host = 0; host < HowMany; host++) {
        if (mcon[host].retcode <= RPC2_ELIMIT)
            continue;

        RPC2_AllocBuffer(Request->Header.BodyLength, &thisreq);

        /* preserve address of allocated packet */
        mcon[host].req = thisreq;
        thisconn       = mcon[host].ceaddr;

        /* initialize header fields to defaults, and copy body of request packet */
        rpc2_InitPacket(thisreq, thisconn, Request->Header.BodyLength);
        memcpy(thisreq->Body, Request->Body, Request->Header.BodyLength);

        /* complete non-default header fields */
        thisreq->Header.SeqNumber = thisconn->NextSeqNumber;
        thisreq->Header.Opcode    = Request->Header.Opcode; /* set by client */
        thisreq->Header.BindTime  = 0;
        thisreq->Header.Flags     = 0;
    }

    /* Notify side effect routine, if any. */
    if (SDescList != NULL) {
        /* We have already verified that all connections have the same side-effect
         * type (or none), so we can simply invoke the procedure corresponding
         * to the first GOOD connection. */
        thisconn = 0;
        for (host = 0; host < HowMany; host++)
            if (mcon[host].retcode > RPC2_ELIMIT) {
                thisconn = mcon[host].ceaddr;
                break;
            }
        if (thisconn && thisconn->SEProcs && thisconn->SEProcs->SE_MultiRPC1) {
            long *seretcode;
            RPC2_PacketBuffer **preqs;
            assert((seretcode = (long *)malloc(HowMany * sizeof(long))) !=
                   NULL);
            assert((preqs = (RPC2_PacketBuffer **)malloc(
                        HowMany * sizeof(RPC2_PacketBuffer *))) != NULL);
            for (host = 0; host < HowMany; host++) {
                seretcode[host] = mcon[host].retcode;
                preqs[host]     = mcon[host].req;
            }

            (*thisconn->SEProcs->SE_MultiRPC1)(HowMany, ConnHandleList,
                                               SDescList, preqs, seretcode);
            for (host = 0; host < HowMany; host++) {
                /* Has the sideeffect modified the original request? */
                if (mcon[host].req != preqs[host]) {
                    RPC2_FreeBuffer(&mcon[host].req);
                    mcon[host].req = preqs[host];
                }
                if (seretcode[host] == RPC2_SUCCESS)
                    continue;

                /* Any new errors? */
                if (mcon[host].retcode != seretcode[host]) {
                    if (seretcode[host] > RPC2_FLIMIT) {
                        SetState(mcon[host].ceaddr,
                                 C_THINK); /* reset connection state */
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
        thisreq  = mcon[host].req;

        /* create call entry */
        mcon[host].sle                    = rpc2_AllocSle(OTHER, thisconn);
        mcon[host].sle->TElem.BackPointer = (char *)mcon[host].sle;

        /* convert to network order */
        rpc2_htonp(thisreq);

        /* Encrypt appropriate portions of the packet */
        rpc2_ApplyE(thisreq, thisconn);
    }
}

/* Get a free context */
static MultiCon *InitMultiCon(int count)
{
    MultiCon *mcon;
    int i;

    mcon = (MultiCon *)calloc(count, sizeof(MultiCon));
    assert(mcon);

    for (i = 0; i < count; i++)
        mcon[i].retcode = RPC2_ABANDONED;

    return (mcon);
}

/* deallocate buffers and free allocated arrays */
void FreeMultiCon(int HowMany, MultiCon *mcon)
{
    int i;

    for (i = 0; i < HowMany; i++) {
        if (mcon[i].sle)
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
    int HowMany, MultiCon *mcon,
    RPC2_Handle ConnHandleList[], /* array of connection ids */
    ARG_INFO *ArgInfo, /* Structure of client information
                                                      (built in MakeMulti) */
    SE_Descriptor SDescList[], /* array of side effect descriptors */
    long (*UnpackMulti)(), /* pointer to unpacking routine */
    struct timeval *TimeOut) /* client specified timeout */
{
    struct SL_Entry *slp;
    RPC2_PacketBuffer *req, *preply; /* RPC2 Response buffers */
    struct CEntry *ce;
    long finalrc, secode = 0;
    long thispacket, hopeleft, i;
    int packets     = 1; /* packet counter for LWP yield */
    int busy        = 0;
    int goodpackets = 0; /* packets with good connection state */
    PacketCon *pcon;
    unsigned long timestamp;
    int rc;

#define EXIT_MRPC_SPR(rc)                                                      \
    {                                                                          \
        MSend_Cleanup(HowMany, mcon, SDescList, TimeOut, pcon);                \
        return (rc);                                                           \
    }

    say(1, RPC2_DebugLevel, "mrpc_SendPacketsReliably()\n");

    TR_MSENDRELIABLY();

    /* find a context */
    /* the packet_con management should be redone to ensure that allocation never
   * fails! */
    pcon = InitPacketCon(HowMany);
    /*
       if((pcon = InitPacketCon(HowMany)) == NULL) {
           for(i = 0; i < HowMany; i++) {
               if (mcon[i].sle == NULL) continue;
               if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, NULL,
                                 RPC2_FAIL, i) == -1)
                  return(RPC2_FAIL);
           }
       }
    */

    if (TimeOut) { /* create a time bomb */
        slp = pcon->pending[goodpackets++] = rpc2_AllocSle(OTHER, NULL);
        rpc2_ActivateSle(slp, TimeOut);
    }

    timestamp = rpc2_MakeTimeStamp();
    say(9, RPC2_DebugLevel, "Sending initial packets at time %lu\n", timestamp);

    /* Do an initial send of packets on all good connections */
    /* for estimating the effiency of the calculation */
    /* should separate this into separate LWP for efficiency */
    for (thispacket = HowMany - 1; thispacket >= 0; thispacket--) {
        /* initialize permuted index array */
        pcon->indexlist[thispacket] = thispacket;

        ce  = mcon[thispacket].ceaddr;
        slp = mcon[thispacket].sle;
        req = mcon[thispacket].req;

        if (!slp) /* something is wrong with connection - don't send packet */
        {
            if (mcon[thispacket].retcode > RPC2_ELIMIT)
                mcon[thispacket].retcode = RPC2_FAIL;
            exchange(pcon, thispacket);
            busy++;
            continue;
        }
        pcon->pending[goodpackets++] = slp; /* build array of good packets */

        /* send the packet and activate socket listener entry */
        /* offer control to Socket Listener every 32 packets to prevent buffer
         * overflow */
        if ((packets++ & 0x1f) && (packets < pcon->indexlen - 6)) {
            LWP_DispatchProcess();
            timestamp = rpc2_MakeTimeStamp();
        }

        req->Header.TimeStamp = htonl(timestamp);
        ce->reqsize           = req->Prefix.LengthOfPacket;

        slp->RetryIndex = 0;
        /* XXX we should have the size of the expected reply packet */
        rpc2_RetryInterval(ce, 0, &slp->RInterval, req->Prefix.LengthOfPacket,
                           sizeof(struct RPC2_PacketHeader), 0);
        rpc2_ActivateSle(slp, &slp->RInterval);

        rpc2_XmitPacket(req, ce->HostInfo->Addr, 0);
    }
    pcon->pending[goodpackets] = NULL;

    if (busy == HowMany)
        EXIT_MRPC_SPR(RPC2_FAIL) /* no packets were sent */

    if (busy == 0)
        finalrc = RPC2_SUCCESS; /* RPC2_FAIL if anything goes wrong */
    else
        finalrc = RPC2_FAIL;

    do {
        hopeleft = 0;
        /* wait for SocketListener to tap me on the shoulder */
        LWP_MwaitProcess(1, (const void **)pcon->pending);

        if (TimeOut && pcon->pending[0]->ReturnCode == TIMEOUT)
            /* Overall timeout expired: clean up state and quit */
            EXIT_MRPC_SPR(RPC2_TIMEOUT)

        /* the loop below looks at a decreasing list of sl entries using the
         * permuted index array for sorting */
        for (i = 0; i < pcon->indexlen; i++) {
            thispacket = pcon->indexlist[i];
            ce         = mcon[thispacket].ceaddr;
            slp        = mcon[thispacket].sle;
            req        = mcon[thispacket].req;
            preply     = NULL;

            switch (slp->ReturnCode) {
            case WAITING:
                hopeleft = 1; /* someday we will be told about this packet */
                continue; /* not yet done with this connection */

            case ARRIVED:
                /* know this hasn't yet been processd */
                say(9, RPC2_DebugLevel, "Request reliably sent on %#x\n",
                    ce->UniqueCID);

                /* At this point the final reply has been received;
                   SocketListener has already decrypted it. */
                preply                   = (RPC2_PacketBuffer *)slp->data;
                mcon[thispacket].retcode = preply->Header.ReturnCode;
                break; /* done with this connection */

            case KEPTALIVE:
            case TIMEOUT:
                if (slp->ReturnCode == KEPTALIVE || rpc2_CancelRetry(ce, slp)) {
                    /* retryindex -1 -> keepalive timeout */
                    say(9, RPC2_DebugLevel, "Keepalive for request on %#x\n",
                        ce->UniqueCID);
                    slp->RetryIndex = -1;
                } else
                    slp->RetryIndex += 1;

                /* XXX we should have the size of the expected reply
                 * packet, somewhere.. */
                rc = rpc2_RetryInterval(ce, slp->RetryIndex, &slp->RInterval,
                                        req->Prefix.LengthOfPacket,
                                        sizeof(struct RPC2_PacketHeader), 0);

                if (rc) {
                    say(9, RPC2_DebugLevel, "Request failed on %#x\n",
                        ce->UniqueCID);
                    rpc2_SetConnError(ce); /* does signal on ConnHandle */
                    finalrc                  = RPC2_FAIL;
                    mcon[thispacket].retcode = RPC2_DEAD;
                    break; /* done with this connection */
                }

                /* else retry with the next Beta value  for timeout */
                hopeleft = 1;
                rpc2_ActivateSle(slp, &slp->RInterval);

                /* timeout? need to retransmit*/
                if (slp->RetryIndex >= 0) {
                    say(9, RPC2_DebugLevel,
                        "Sending retry %d at %ld on %#x (timeout %ld.%06ld)\n",
                        slp->RetryIndex, rpc2_time(), ce->UniqueCID,
                        slp->RInterval.tv_sec, slp->RInterval.tv_usec);

                    req->Header.Flags =
                        htonl((ntohl(req->Header.Flags) | RPC2_RETRY));
                    req->Header.TimeStamp = htonl(rpc2_MakeTimeStamp());

                    rpc2_Sent.Retries += 1;
                    rpc2_XmitPacket(req, ce->HostInfo->Addr, 0);
                }
                continue; /* not yet done with this connection */

            case NAKED: /* explicitly NAK'ed this time or earlier */
                say(9, RPC2_DebugLevel, "Request NAK'ed on %#x\n",
                    ce->UniqueCID);
                rpc2_SetConnError(ce);
                finalrc                  = RPC2_FAIL;
                mcon[thispacket].retcode = RPC2_NAKED;
                break; /* done with this connection */

            default: /* abort */
                /* BUSY ReturnCode should never go into switch */
                // assert(FALSE);
                say(9, RPC2_DebugLevel, "Request aborted on %#x\n",
                    ce->UniqueCID);
                rpc2_SetConnError(ce);
                finalrc                  = RPC2_FAIL;
                mcon[thispacket].retcode = RPC2_DEAD;
                break; /* done with this connection */
            }

            /* done with this connection, move it to the end of the list */
            i = exchange(pcon, i);

            if (GOODSEDLE(thispacket) && ce->SEProcs &&
                ce->SEProcs->SE_MultiRPC2) {
                secode = (*ce->SEProcs->SE_MultiRPC2)(
                    ConnHandleList[thispacket], &SDescList[thispacket], preply);

                if (mcon[thispacket].retcode == RPC2_SUCCESS) {
                    if (secode < RPC2_FLIMIT) {
                        rpc2_SetConnError(ce);
                        finalrc                  = RPC2_FAIL;
                        mcon[thispacket].retcode = RPC2_SEFAIL2;
                    } else if (SDescList[thispacket].LocalStatus ==
                                   SE_FAILURE ||
                               SDescList[thispacket].RemoteStatus ==
                                   SE_FAILURE) {
                        finalrc                  = RPC2_FAIL;
                        mcon[thispacket].retcode = RPC2_SEFAIL1;
                    }
                }
            }

            if ((*UnpackMulti)(HowMany, ConnHandleList, ArgInfo, preply,
                               mcon[thispacket].retcode, thispacket) == -1)
                EXIT_MRPC_SPR(finalrc) /* enough responses, return */
        }
    } while (hopeleft);

    EXIT_MRPC_SPR(finalrc)
#undef EXIT_MRPC_SPR
}

static PacketCon *InitPacketCon(int count)
{
    PacketCon *pcon;

    /* allocate new context */
    pcon = (PacketCon *)malloc(sizeof(PacketCon));
    assert(pcon);

    pcon->pending =
        (struct SL_Entry **)calloc(count + 2, sizeof(struct SL_Entry *));
    assert(pcon->pending);

    pcon->indexlen  = count;
    pcon->indexlist = (long *)malloc(count * sizeof(long));
    assert(pcon->indexlist);

    return (pcon);
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
static long exchange(PacketCon *pcon, int cur_ind)
{
    long tmp;

    pcon->indexlen--;
    if (cur_ind == pcon->indexlen)
        return (cur_ind);
    tmp                             = pcon->indexlist[cur_ind];
    pcon->indexlist[cur_ind]        = pcon->indexlist[pcon->indexlen];
    pcon->indexlist[pcon->indexlen] = tmp;
    return (cur_ind - 1);
}

/* Clean up state before exiting mrpc_SendPacketsReliably */
static void MSend_Cleanup(int HowMany, MultiCon *mcon,
                          SE_Descriptor SDescList[], struct timeval *Timeout,
                          PacketCon *pcon)
{
    long thispacket, i;
    struct SL_Entry *slp;

    for (i = 0; i < pcon->indexlen; i++) {
        thispacket = pcon->indexlist[i];
        slp        = mcon[thispacket].sle;
        TM_Remove(rpc2_TimerQueue, &slp->TElem);

        /* Call side-effect routine and increment connection sequence number for
         * abandoned requests */
        if (GOODSEDLE(thispacket) && mcon[thispacket].ceaddr->SEProcs &&
            mcon[thispacket].ceaddr->SEProcs->SE_MultiRPC2)
            (*mcon[thispacket].ceaddr->SEProcs->SE_MultiRPC2)(
                mcon[thispacket].ceaddr->UniqueCID, &SDescList[thispacket],
                NULL);
        rpc2_IncrementSeqNumber(mcon[thispacket].ceaddr);
        SetState(mcon[thispacket].ceaddr, C_THINK);
        /*	LWP_NoYieldSignal((char *)mcon[thispacket].ceaddr);*/
    }

    if (Timeout) {
        slp = pcon->pending[0]; /* Tag assumed to be TIMEENTRY */
        if (slp->ReturnCode == WAITING) {
            /* delete time bomb if it has not fired  */
            TM_Remove(rpc2_TimerQueue, &slp->TElem);
        }
        rpc2_FreeSle(&slp); /* free timer entry */
    }
    FreePacketCon(pcon);
}
