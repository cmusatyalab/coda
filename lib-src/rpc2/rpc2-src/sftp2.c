/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
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
	-- SFTP: a smart file transfer protocol using windowing and piggybacking
	-- sftp2.c contains SFTP listener-related routines

*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <assert.h>
#include "rpc2.private.h"
#include <rpc2/se.h>
#include <rpc2/sftp.h>

static void ClientPacket();
static void ServerPacket();
static void SFSendNAK(RPC2_PacketBuffer *pb);

#define BOGUS(pb) \
    (sftp_TraceBogus(2, __LINE__), sftp_bogus++, SFTP_FreeBuffer(&pb))

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* This function is not called by the sftp code itself */
void SFTP_DispatchProcess(void)
{
    RPC2_DispatchProcess();
}

void sftp_ExaminePacket(RPC2_PacketBuffer *pb)
{
    struct SFTP_Entry *sfp;
    struct CEntry *ce;
    int iamserver;

    /* collect statistics */
    sftp_Recvd.Total++;
    sftp_Recvd.Bytes += pb->Prefix.LengthOfPacket;

    /* SFTPVERSION must match or we have no hope at all. */
    if (ntohl(pb->Header.ProtoVersion) != SFTPVERSION) {
        BOGUS(pb);
        return;
    }

    /* Get the connection and side-effect entries, and make sure they aren't
     * in error. */
    ce = rpc2_GetConn(ntohl(pb->Header.RemoteHandle));

    /* check if the packet and the connection use the same security context. */
    if (ce && (ce->sa.decrypt || ce->sa.validate) && &ce->sa != pb->Prefix.sa) {
        say(1, SFTP_DebugLevel,
            "Incoming sftp packet with different security association\n");
        SFTP_FreeBuffer(&pb);
        return;
    }

    sfp = ce ? (struct SFTP_Entry *)ce->SideEffectPtr : NULL;

    if (!ce || !sfp || TestState(ce, CLIENT, C_HARDERROR) ||
        TestState(ce, SERVER, S_HARDERROR)
        //	|| pb->Header.LocalHandle != ce->PeerHandle
        || sfp->WhoAmI == ERROR || sfp->WhoAmI == DISKERROR) {
        /* SFSendNAK expects host-order */
        pb->Header.LocalHandle = ntohl(pb->Header.LocalHandle);
        SFSendNAK(pb); /* NAK this packet */
        BOGUS(pb);
        return;
    }

    /* Decrypt and net-to-host the packet. */
    if (ntohl(pb->Header.Flags) & RPC2_ENCRYPTED)
        sftp_Decrypt(pb, sfp);
    rpc2_ntohp(pb);

    /* Drop NAK's. */
    if (pb->Header.Opcode == SFTP_NAK) {
        sftp_Recvd.Naks++;
        say(1, SFTP_DebugLevel, "SFTP_NAK received\n");

        /* sfp->WhoAmI is set to ERROR by sftp_SetError */
        iamserver = (sfp->WhoAmI == SFSERVER);

        sftp_SetError(sfp, ERROR);
        SFTP_FreeBuffer(&pb);

        /* When we are the SFSERVER, tell the blocked thread about the
	 * failure now, instead of having it wait for a full timeout. */
        if (iamserver)
            ServerPacket(NULL, sfp);
        return;
    }

    /* SANITY CHECK: validate socket-level and connection-level host values. */
    /* It looks like we can validly get an IPv6 reply to an IPv4 request, so
     * I disabled this for now */
    if (0 && !RPC2_cmpaddrinfo(sfp->HostInfo->Addr, pb->Prefix.PeerAddr)) {
        say(1, SFTP_DebugLevel, "Received SFTP packet from unexpected host\n");
        SFSendNAK(pb); /* NAK this packet */
        BOGUS(pb);
        return;
    }

    /* SANITY CHECK: make sure this pertains to the current RPC call. */
    if (pb->Header.ThisRPCCall != sfp->ThisRPCCall) {
        say(1, SFTP_DebugLevel, "Old SFTP packet RPC %d, expecting RPC %d\n",
            pb->Header.ThisRPCCall, sfp->ThisRPCCall);
        SFTP_FreeBuffer(&pb);
        return;
    }

    /* Client records SFTP port here since we may need to use it before we
     * record other parms. */
    if (sfp->GotParms == FALSE && sfp->WhoAmI == SFCLIENT) {
        sfp->HostInfo = ce->HostInfo; /* Set up host/port linkage. */
        /* Can't set GotParms to TRUE yet; must pluck off other parms. */
    }

    /* update the last-heard-from times for this SFTP entry, and the
       connection-independent entry for this host. */
    assert(sfp->HostInfo != NULL);

    /* structure assignment */
    sfp->LastWord = sfp->HostInfo->LastWord = pb->Prefix.RecvStamp;

    /* remember packet arrival time to compensate RTT errors */
    TVTOTS(&pb->Prefix.RecvStamp, sfp->RequestTime);

    /* Go handle the packet appropriately. */
    sftp_TraceStatus(sfp, 2, __LINE__);
    if (sfp->WhoAmI == SFSERVER)
        ServerPacket(pb, sfp);
    else
        ClientPacket(pb, sfp);
}

/* Find a sleeping LWP to deal with this packet */
static void ServerPacket(RPC2_PacketBuffer *whichPacket,
                         struct SFTP_Entry *sEntry)
{
    struct SL_Entry *sl;

    /* WARNING: we are assuming state TIMEOUT is essentially the same as state
     * WAITING from the point of of view of the test below; this will be true
     * if no LWP yields control while its SLSlot state is TIMEOUT.  There
     * could be serious hard-to-find bugs if this assumption is violated. */

    if (sEntry->XferState != XferInProgress) {
        fprintf(stderr, "No active SFTP transfer, dropping incoming packet\n");
        BOGUS(whichPacket);
        return;
    }

    /* queue packet */
    rpc2_MoveEntry(&rpc2_PBList, &sEntry->RecvQueue, whichPacket, &rpc2_PBCount,
                   &sEntry->RecvQueueLen);

    sl = sEntry->Sleeper;
    if (!sl || (sl->ReturnCode != WAITING && sl->ReturnCode != TIMEOUT)) {
        /* no one is actively waiting for this packet, it should get picked
         * up from the queue when the SideEffect thread is done processing */
        return;
    }
    sEntry->Sleeper = NULL; /* no longer anyone waiting for a packet */
    rpc2_DeactivateSle(sl, ARRIVED);
    LWP_SignalProcess((char *)sl);
}

static void ClientPacket(RPC2_PacketBuffer *whichPacket,
                         struct SFTP_Entry *sEntry)
{
    /* Deal with this packet on Listener's thread of control */

    switch ((int)whichPacket->Header.Opcode) {
    case SFTP_ACK:
        /* Makes sense only if we are on source side */
        if (IsSource(sEntry)) {
            if (sftp_AckArrived(whichPacket, sEntry) < 0) {
                SFSendNAK(whichPacket); /* NAK this packet */
                sftp_SetError(sEntry, ERROR);
            }
            SFTP_FreeBuffer(&whichPacket);
        } else {
            BOGUS(whichPacket);
        }
        break;

    case SFTP_DATA:
        /* Makes sense only if we are on sink side */
        if (IsSink(sEntry)) {
            if (sftp_DataArrived(whichPacket, sEntry) < 0) {
                SFSendNAK(whichPacket); /* NAK this packet */
                if (sEntry->WhoAmI != DISKERROR) {
                    sftp_SetError(sEntry, ERROR);
                    SFTP_FreeBuffer(&whichPacket);
                }
            }
        } else {
            BOGUS(whichPacket);
        }
        break;

    case SFTP_START:
        /* Makes sense only on client between file transfers */
        if (IsSource(sEntry)) {
            if (sftp_StartArrived(whichPacket, sEntry) < 0) {
                SFSendNAK(whichPacket); /* NAK this packet */
                sftp_SetError(sEntry, ERROR);
            }
            SFTP_FreeBuffer(&whichPacket);
        } else {
            BOGUS(whichPacket);
        }
        break;

    case SFTP_NAK: /* should have been dealt with in sftp_ExaminePacket()*/
    default:
        BOGUS(whichPacket);
        break;
    }
}

static void SFSendNAK(RPC2_PacketBuffer *pb)
{
    struct SFTP_Entry fake_se;

    RPC2_PacketBuffer *nakpb;
    RPC2_Handle remoteHandle = pb->Header.LocalHandle;

    /* don't NAK NAK's */
    if (remoteHandle == -1)
        return;

    sftp_Sent.Naks++;
    say(1, SFTP_DebugLevel, "SFSendNAK\n");
    SFTP_AllocBuffer(0, &nakpb);
    nakpb->Prefix.LengthOfPacket = sizeof(struct RPC2_PacketHeader);
    nakpb->Header.ProtoVersion   = SFTPVERSION;
    nakpb->Header.RemoteHandle   = remoteHandle;
    nakpb->Header.LocalHandle    = -1; /* "from Listener" */
    nakpb->Header.BodyLength     = 0;
    nakpb->Header.Opcode         = SFTP_NAK;
    /* All other fields are irrelevant in a NAK packet */
    rpc2_htonp(nakpb);

    /* add a reference to the addrinfo in a fake_se */
    fake_se.HostInfo = rpc2_GetHost(pb->Prefix.PeerAddr);

    sftp_XmitPacket(&fake_se, nakpb, 1); /* ignore return code */

    rpc2_FreeHost(&fake_se.HostInfo);
    SFTP_FreeBuffer(&nakpb);
}
