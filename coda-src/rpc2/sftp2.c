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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rpc2/sftp2.c,v 4.2 1998/04/14 21:07:04 braam Exp $";
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


/*
	-- SFTP: a smart file transfer protocol using windowing and piggybacking
	-- sftp2.c contains SFTP listener-related routines

*/


#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "rpc2.private.h"
#include "se.h"
#include "sftp.h"

extern errno;

static void ClientPacket();
static void ServerPacket();
static void ExaminePacket();
static void SFSendNAK();
static void sftp_ProcessPackets();
static bool sftp_MorePackets();
static ScanTimerQ(), AwaitEvent(), hostcmp();

#define NAKIT(se)\
    SFSendNAK((se)->PInfo.RemoteHandle, &((se)->PInfo.RemoteHost), &((se)->PeerPortal))

#define BOGUS(pb)\
    (sftp_TraceBogus(2, __LINE__), sftp_bogus++, SFTP_FreeBuffer(&pb))

sftp_Listener()
    {/* LWP that listens for SFTP packets */
    
    TM_Init(&sftp_Chain);

    while (TRUE)
	{
	ScanTimerQ();	/* wakeup all LWPs with expired timer entries */

	if (AwaitEvent() != 1)
		continue; /* timeout or bogus wakeup */
	
	sftp_ProcessPackets();
	}
    }

SFTP_DispatchProcess()
    {
    struct timeval tv;
    bool rpc2, sftp;

    while (sftp_MorePackets(&rpc2, &sftp))
	{
	if (rpc2) rpc2_ProcessPackets();
	if (sftp) sftp_ProcessPackets();
	}

    /* keep current time from being too inaccurate */
    (void) FT_GetTimeOfDay(&tv, (struct timezone *) 0);

    /* also check for timed-out events, using current time */
    rpc2_ExpireEvents();
    ScanTimerQ();

    LWP_DispatchProcess();
    }

static void sftp_ProcessPackets()
{
    RPC2_PacketBuffer *pb;
    RPC2_HostIdent rhost;
    RPC2_PortalIdent rportal;
    int rc;

    bzero(&rhost, sizeof(rhost));	/* Else bcmp() in ExaminePacket() may give bogus results */
    bzero(&rportal, sizeof(rportal));	/* ditto */

    /* A packet has arrived: read it in  */

    SFTP_AllocBuffer(SFTP_MAXBODYSIZE, &pb);
    rc = sftp_RecvPacket(sftp_Socket, pb, &rhost, &rportal);
    if (rc < 0)
	{
	/* If errno = 0, libfail killed the packet.
	   else packet greater than RPC2_MAXPACKETSIZE */
	if (errno != 0)
	    BOGUS(pb);
	return;
	}

    /* Go look at this  packet */
    ExaminePacket(pb, &rhost, &rportal);
}

static ScanTimerQ()
    {
    register int i;
    register struct SLSlot *s;
    register struct TM_Elem *t;

    /* scan timer queue and notify timed out events */
    for (i = TM_Rescan(sftp_Chain); i > 0; i--)
	{
	assert((t = TM_GetExpired(sftp_Chain)) != NULL);
	s = (struct SLSlot *)t->BackPointer;
	s->State= S_TIMEOUT;
	REMOVETIMER(s);
	LWP_NoYieldSignal((char *)s);
	}
    }


static bool sftp_MorePackets(rpc2, sftp)
bool *rpc2, *sftp;
    {
    struct timeval tv;
    long rmask, wmask, emask;

    tv.tv_sec = tv.tv_usec = 0;	    /* do polling select */
    emask = rmask = (1 << sftp_Socket) | (1 << rpc2_RequestSocket);
    wmask = 0;
    /* We use select rather than IOMGR_Select to avoid overheads. This is acceptable
    	only because we are doing a polling select */
    if (select(8*sizeof(long), &rmask, &wmask, &emask, &tv) > 0)
	{
	*rpc2 = rmask & (1 << rpc2_RequestSocket);
	*sftp = rmask & (1 << sftp_Socket);
	return(TRUE);
	}
    else return(FALSE);
    }

static AwaitEvent()
    /* Awaits for a packet or earliest timeout and returns code from IOMGR_Select() */
    {
    int emask, rmask, wmask, rc;
    register struct timeval *tvp;
    register struct TM_Elem *t;
    
    /* wait for packet or earliest timeout */
    t = TM_GetEarliest(sftp_Chain);
    if (t == NULL) tvp = NULL;
    else tvp = &t->TimeLeft;
    
    emask = rmask = (1 << sftp_Socket);
    wmask = 0;

    /* Only place where sftp_Listener() gives up control */
    rc = IOMGR_Select(8*sizeof(long), &rmask, &wmask, &emask, tvp);
    return(rc);
    }


static void ExaminePacket(whichPacket, whichHost, whichPortal)
    RPC2_PacketBuffer *whichPacket;
    RPC2_HostIdent *whichHost;
    RPC2_PortalIdent *whichPortal;
    {
    RPC2_PacketBuffer	*pb = whichPacket;
    struct SFTP_Entry	*sfp;
    struct CEntry	*ce;
    struct HEntry 	*he;

    /* SFTPVERSION must match or we have no hope at all. */
    if (ntohl(pb->Header.ProtoVersion) != SFTPVERSION)
	{ BOGUS(pb); return; }

    /* Translate the packet to the singlecast channel if it was multicasted */
    if (ntohl(pb->Header.Flags) & RPC2_MULTICAST)
	if (!SFXlateMcastPacket(pb, whichHost, whichPortal))
	    /* Can't NAK since we (probably) don't know the real sender's handle! */
	    /* IMPLEMENT MCNAK's!!! */
	    { BOGUS(pb); return; }

    /* Get the connection and side-effect entries, and make sure they aren't in error. */
    if ((ce = rpc2_GetConn(ntohl(pb->Header.RemoteHandle))) == NULL ||
	TestState(ce, CLIENT, C_HARDERROR) ||
	TestState(ce, SERVER, S_HARDERROR) ||
	(sfp = (struct SFTP_Entry *)ce->SideEffectPtr) == NULL ||
	sfp->WhoAmI == ERROR ||
	sfp->WhoAmI == DISKERROR)
	{
	if (ntohl(pb->Header.LocalHandle) != -1)	/* don't NAK NAKs! */
	    SFSendNAK(ntohl(pb->Header.LocalHandle), whichHost, whichPortal);
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
	sftp_SetError(sfp, ERROR);
	SFTP_FreeBuffer(&pb);
	return;	
	}

    /* SANITY CHECK: validate socket-level and connection-level host values. */
    if (hostcmp(whichHost, &sfp->PInfo.RemoteHost) == FALSE)
	/* Can't compare portals, since SFTP socket is not RPC socket */
	{ SFSendNAK(pb->Header.LocalHandle, whichHost, whichPortal); BOGUS(pb); return; }
	    
    /* SANITY CHECK: make sure this pertains to the current RPC call. */
    if (pb->Header.ThisRPCCall != sfp->ThisRPCCall)
	{
	say(0, SFTP_DebugLevel, "Old SFTP packet RPC %ld, expecting RPC %ld\n", pb->Header.ThisRPCCall, sfp->ThisRPCCall);
	SFTP_FreeBuffer(&pb);
	return;
	}

    /* Client records SFTP portal here since we may need to use it before we record other parms. */
    if (sfp->GotParms == FALSE && sfp->WhoAmI == SFCLIENT)
	{
	/* SANITY CHECK: validate peer's SFTP Portal for multicast. */
	/* The requirement is that (Server.SFTP.Portal = Server.RPC.Portal + 1). */
	if (sfp->UseMulticast)
	    {
	    long s_portal = ntohs(whichPortal->Value.InetPortNumber);
	    long r_portal = ntohs(sfp->PInfo.RemotePortal.Value.InetPortNumber);
	    if (s_portal != r_portal + 1)
		{
		say(9, SFTP_DebugLevel, "Bad Peer Portal: 0x%lx\tExpecting: 0x%lx\n", s_portal, r_portal + 1);
		SFSendNAK(pb->Header.LocalHandle, whichHost, whichPortal);
		BOGUS(pb);
		return;
		}
	    }

	sfp->PeerPortal	= *whichPortal;	    /* structure assignment */

	/* Set up host/portal linkage. */
	sfp->HostInfo = rpc2_GetHost(&sfp->PInfo.RemoteHost, &sfp->PeerPortal);
	if (sfp->HostInfo == NULL) 
	    sfp->HostInfo = rpc2_AllocHost(&sfp->PInfo.RemoteHost, 
					   &sfp->PeerPortal, SMARTFTP_HE);

	/* Can't set GotParms to TRUE yet; must pluck off other parms. */
	}

    /* update the last-heard-from times for this SFTP entry, and the 
       connection-independent entry for this host/SFTP portal. */
    FT_GetTimeOfDay(&sfp->LastWord, (struct timezone *)0);
    assert(sfp->HostInfo != NULL);
    sfp->HostInfo->LastWord = sfp->LastWord;	/* structure assignment */

    /* Go handle the packet appropriately. */
    sftp_TraceStatus(sfp, 2, __LINE__);
    if (sfp->WhoAmI == SFSERVER) ServerPacket(pb, sfp);
    else ClientPacket(pb, sfp);
    }


static void ServerPacket(whichPacket, whichEntry)
    RPC2_PacketBuffer *whichPacket;
    register struct SFTP_Entry *whichEntry;
    /* Find a sleeping LWP to deal with this packet */
    {
    register struct SLSlot *sls;

    /* WARNING: we are assuming state TIMEOUT is essentially the same as state WAITING from the point of
	of view of the test below; this will be true if no LWP yields control while its SLSlot state is
	TIMEOUT.  There could be serious hard-to-find bugs if this assumption is violated.
    */

    sls = whichEntry->Sleeper;
    if (sls == NULL || (sls->State != S_WAITING && sls->State != S_TIMEOUT))
	{/* no one expects this packet; toss it out; NAK'ing may have race hazards */
	BOGUS(whichPacket);
	return;
	}
    whichEntry->Sleeper = NULL;	/* no longer anyone waiting for a packet */
    sls->State = S_ARRIVED;
    sls->Packet = whichPacket;
    REMOVETIMER(sls);
    LWP_NoYieldSignal((char *)sls);
    }


static void ClientPacket(whichPacket, whichEntry)
    RPC2_PacketBuffer *whichPacket;
    register struct SFTP_Entry *whichEntry;
    {
    /* Deal with this packet on Listener's thread of control */

    switch ((int) whichPacket->Header.Opcode)
	{
	case SFTP_NAK:
	    assert(FALSE);  /* should have been dealt with in ExaminePacket() */
	    break;	    
	    
	case SFTP_ACK:
	    /* Makes sense only if we are on source side */
	    if (IsSource(whichEntry))
		{
		if (sftp_AckArrived(whichPacket, whichEntry) < 0)
		    {
		    NAKIT(whichEntry);
		    sftp_SetError(whichEntry, ERROR);
		    }
		}
	    else
		{
		BOGUS(whichPacket);
		}
	    break;
	
	case SFTP_DATA:
	    /* Makes sense only if we are on sink side */
	    if (IsSink(whichEntry))
		{
		if (sftp_DataArrived(whichPacket, whichEntry) < 0)
		    {
		    if (whichEntry->WhoAmI != DISKERROR) sftp_SetError(whichEntry, ERROR);
		    NAKIT(whichEntry);
		    }
		}
	    else
		{
		BOGUS(whichPacket);
		}
	    break;
	
	case SFTP_START:
	    /* Makes sense only on client between file transfers */
	    if (IsSource(whichEntry))
		{
		if (sftp_StartArrived(whichPacket, whichEntry) < 0)
		    {
		    NAKIT(whichEntry);
		    sftp_SetError(whichEntry, ERROR);
		    }
		}
	    else
		{
		BOGUS(whichPacket);
		}
	    break;
	
	default:
	    BOGUS(whichPacket);
	    break;
	}
    }


static void SFSendNAK(remoteHandle, whichHost, whichPortal)
    RPC2_Handle remoteHandle;
    RPC2_HostIdent *whichHost;
    RPC2_PortalIdent *whichPortal;
    {
    RPC2_PacketBuffer *nakpb;

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
    sftp_XmitPacket(sftp_Socket, nakpb, whichHost, whichPortal);    /* ignore return code */
    SFTP_FreeBuffer(&nakpb);
    }


static hostcmp(host1, host2)
    register RPC2_HostIdent *host1;
    register RPC2_HostIdent *host2;
    /* Returns TRUE iff host1 and host2 are the same. FALSE otherwise */
    {
    if (host1->Tag != host2->Tag) return(FALSE);
    
    switch(host1->Tag)
	{
	case RPC2_HOSTBYINETADDR:
	    if (host1->Value.InetAddress == host2->Value.InetAddress) return(TRUE);
	    else return(FALSE);

	default: assert(FALSE);
	}
    /*NOTREACHED*/
    }
