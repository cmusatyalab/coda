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
#include "sftp.h"

static void ClientPacket();
static void ServerPacket();
static void SFSendNAK(RPC2_PacketBuffer *pb);

#define BOGUS(pb)\
    (sftp_TraceBogus(2, __LINE__), sftp_bogus++, SFTP_FreeBuffer(&pb))

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

/* This function is not called by the sftp code itself */
void SFTP_DispatchProcess(void)
{
    struct timeval tv;
    int fd;

    while ((fd = rpc2_MorePackets()) != -1)
	rpc2_ProcessPackets(fd);

    /* keep current time from being too inaccurate */
    (void) FT_GetTimeOfDay(&tv, (struct timezone *) 0);

    /* also check for timed-out events, using current time */
    rpc2_ExpireEvents();
    LWP_DispatchProcess();
}

void sftp_ExaminePacket(RPC2_PacketBuffer *pb)
{
    struct SFTP_Entry	*sfp;
    struct CEntry	*ce;
    int			 iamserver;

    /* collect statistics */
    if (ntohl(pb->Header.Flags) & RPC2_MULTICAST) {
	sftp_MRecvd.Total++;
	sftp_MRecvd.Bytes += pb->Prefix.LengthOfPacket;
    } else {
	sftp_Recvd.Total++;
	sftp_Recvd.Bytes += pb->Prefix.LengthOfPacket;
    }

    /* SFTPVERSION must match or we have no hope at all. */
    if (ntohl(pb->Header.ProtoVersion) != SFTPVERSION)
	{ BOGUS(pb); return; }

    /* Translate the packet to the singlecast channel if it was multicasted */
    if (ntohl(pb->Header.Flags) & RPC2_MULTICAST)
	if (!SFXlateMcastPacket(pb))
	    /* Can't NAK since we (probably) don't know the real sender's
	     * handle! */
	    /* IMPLEMENT MCNAK's!!! */
	    { BOGUS(pb); return; }

    /* Get the connection and side-effect entries, and make sure they aren't in error. */
    if ((ce = rpc2_GetConn(ntohl(pb->Header.RemoteHandle))) == NULL ||
	TestState(ce, CLIENT, C_HARDERROR) ||
	TestState(ce, SERVER, S_HARDERROR) ||
//	pb->Header.LocalHandle != ce->PeerHandle ||
	(sfp = (struct SFTP_Entry *)ce->SideEffectPtr) == NULL ||
	sfp->WhoAmI == ERROR ||
	sfp->WhoAmI == DISKERROR)
    {
	/* SFSendNAK expects host-order */
	pb->Header.LocalHandle = ntohl(pb->Header.LocalHandle);
	SFSendNAK(pb); /* NAK this packet */
	BOGUS(pb);
	return;
    }

    /* Decrypt and net-to-host the packet. */
    if (ntohl(pb->Header.Flags) & RPC2_ENCRYPTED) sftp_Decrypt(pb, sfp);
    rpc2_ntohp(pb);

    /* Drop NAK's. */
    if (pb->Header.Opcode == SFTP_NAK)
    {
	sftp_Recvd.Naks++;
	say(0, SFTP_DebugLevel, "SFTP_NAK received\n");

	/* sfp->WhoAmI is set to ERROR by sftp_SetError */
	iamserver = (sfp->WhoAmI == SFSERVER);

	sftp_SetError(sfp, ERROR);
	SFTP_FreeBuffer(&pb);

	/* When we are the SFSERVER, tell the blocked thread about the
	 * failure now, instead of having it wait for a full timeout. */
	if (iamserver) ServerPacket(NULL, sfp);
	return;	
    }

    /* SANITY CHECK: validate socket-level and connection-level host values. */
    /* It looks like we can validly get an IPv6 reply to an IPv4 request, so
     * I disabled this for now */
    if (0 && !RPC2_cmpaddrinfo(sfp->HostInfo->Addr, pb->Prefix.PeerAddr))
    {
	say(0, SFTP_DebugLevel, "Received SFTP packet from unexpected host\n");
	SFSendNAK(pb); /* NAK this packet */
	BOGUS(pb);
	return;
    }
	    
    /* SANITY CHECK: make sure this pertains to the current RPC call. */
    if (pb->Header.ThisRPCCall != sfp->ThisRPCCall)
    {
	say(0, SFTP_DebugLevel, "Old SFTP packet RPC %d, expecting RPC %ld\n", pb->Header.ThisRPCCall, sfp->ThisRPCCall);
	SFTP_FreeBuffer(&pb);
	return;
    }

    /* Client records SFTP port here since we may need to use it before we record other parms. */
    if (sfp->GotParms == FALSE && sfp->WhoAmI == SFCLIENT)
    {
	sfp->HostInfo = ce->HostInfo;		/* Set up host/port linkage. */
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
    if (sfp->WhoAmI == SFSERVER) ServerPacket(pb, sfp);
    else			 ClientPacket(pb, sfp);
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

    sl = sEntry->Sleeper;
    if (!sl || (sl->ReturnCode != WAITING && sl->ReturnCode != TIMEOUT))
    {/* no one expects this packet; toss it out; NAK'ing may have race hazards */
	if (whichPacket) {
	    fprintf(stderr, "No waiters, dropped incoming sftp packet\n");
	    BOGUS(whichPacket);
	}
	return;
    }
    sEntry->Sleeper = NULL;	/* no longer anyone waiting for a packet */
    rpc2_DeactivateSle(sl, ARRIVED);
    sl->Packet = whichPacket;
    LWP_SignalProcess((char *)sl);
}


static void ClientPacket(RPC2_PacketBuffer *whichPacket,
			 struct SFTP_Entry *sEntry)
{
    unsigned long bytes;
    int retry;
    /* Deal with this packet on Listener's thread of control */

    switch ((int) whichPacket->Header.Opcode)
    {
    case SFTP_NAK:
	assert(FALSE);  /* should have been dealt with in sftp_ExaminePacket()*/
	break;	    

    case SFTP_ACK:
	/* Makes sense only if we are on source side */
	if (IsSource(sEntry)) {
	    /* we need to get some indication of a retry interval, so that
	     * AckArrived->SendStrategy->CheckWorried() can actually do
	     * the right thing */

	    /* estimated size of an sftp data transfer */
	    bytes = ((sEntry->PacketSize + sizeof(struct RPC2_PacketHeader)) *
		     sEntry->SendAhead);

	    retry = 1;
	    rpc2_RetryInterval(sEntry->LocalHandle, sizeof(struct RPC2_PacketHeader),
			       bytes, &retry, sEntry->RetryCount, &sEntry->RInterval);

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
    if (remoteHandle == -1) return;

    sftp_Sent.Naks++;
    say(0, SFTP_DebugLevel, "SFSendNAK\n");
    SFTP_AllocBuffer(0, &nakpb);
    nakpb->Prefix.LengthOfPacket = sizeof(struct RPC2_PacketHeader);
    nakpb->Header.ProtoVersion = SFTPVERSION;
    nakpb->Header.RemoteHandle = remoteHandle; 
    nakpb->Header.LocalHandle = -1;	/* "from Listener" */
    nakpb->Header.BodyLength = 0;
    nakpb->Header.Opcode = SFTP_NAK;
    /* All other fields are irrelevant in a NAK packet */
    rpc2_htonp(nakpb);

    /* add a reference to the addrinfo in a fake_se */
    fake_se.HostInfo = rpc2_GetHost(pb->Prefix.PeerAddr);

    sftp_XmitPacket(&fake_se, nakpb, 1);    /* ignore return code */

    rpc2_FreeHost(&fake_se.HostInfo);
    SFTP_FreeBuffer(&nakpb);
}

