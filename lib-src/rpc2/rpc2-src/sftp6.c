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
	-- sftp6.c contains (most of) the Multicast extensions to SFTP
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#include <netinet/in.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include "rpc2.private.h"
#include <rpc2/se.h>
#include "sftp.h"


/* sftp5.c */
extern void B_ShiftLeft();
extern void B_ShiftRight();
extern void B_Assign();
extern void B_And();

static void SFSendBusy();
static void MC_AppendParmsToPacket();
static int MC_SendStrategy(), SDescCmp(), MC_ExtractParmsFromPacket();

/*----------------------- The procs below interface directly with RPC2 ------------------------ */

#define	HOSTSEOK(host)\
  (ceaddr[host] && (retcode[host] > RPC2_ELIMIT) && (SDescList[host].Tag != OMITSE))

#define	FAIL_MCRPC1(rCode)\
        {\
        say(9, SFTP_DebugLevel, "FAIL_MCRPC1: code = %d\n", rCode);\
	for (host = 0; host < HowMany; host++)\
	    if (HOSTSEOK(host)) retcode[host] = rCode;\
	mse->SDesc = NULL;\
	if (mdesc) free(mdesc);\
	free(ceaddr);\
	return -1;\
        }

#define	INIT_SE_DESC(desc)\
        {\
	((SE_Descriptor	*)desc)->LocalStatus = SE_SUCCESS;  /* non-execution == success */\
	((SE_Descriptor	*)desc)->RemoteStatus =	SE_SUCCESS; /* non-execution == success */\
        sftp_Progress((SE_Descriptor *)desc, 0);\
        }

#define INIT_SE_ENTRY(se, desc, req)\
	se->SDesc = desc;\
	se->ThisRPCCall = req->Header.SeqNumber;\
	se->XferState = XferNotStarted;\
	se->UseMulticast = TRUE;\
	se->RepliedSinceLastSS = FALSE;\
	se->McastersStarted = 0;\
	se->McastersFinished = 0;\
	se->HitEOF = FALSE;\
	se->SendFirst = se->SendLastContig + 1;\
	se->SendMostRecent = se->SendLastContig;\
	se->SendWorriedLimit = se->SendLastContig;\
	memset(se->SendTheseBits, 0, BITMASKWIDTH * sizeof(long));\
	se->ReadAheadCount = 0;\
	se->RecvMostRecent = se->RecvLastContig;\
	memset(se->RecvTheseBits, 0, BITMASKWIDTH * sizeof(long));

long SFTP_MultiRPC1(IN HowMany, IN ConnHandleList, IN MCast, INOUT SDescList, INOUT req, INOUT retcode)
    int			HowMany;
    RPC2_Handle		ConnHandleList[];
    RPC2_Multicast	*MCast;
    SE_Descriptor	SDescList[];
    RPC2_PacketBuffer	*req[];
    long		retcode[];
    {
    int	host;
    struct CEntry **ceaddr = (struct CEntry **)malloc(HowMany * sizeof(struct CEntry *));
    assert(ceaddr != NULL);

    say(0, SFTP_DebugLevel, "SFTP_MultiRPC1()\n");

    /* reacquire CEntry pointers */
    for (host = 0; host < HowMany; host++)
	ceaddr[host] = rpc2_GetConn(ConnHandleList[host]);

    if (!MCast)
	/* Non-multicast MRPC: simply iterate over the set of hosts calling SFTP_MakeRPC1() */
	{
	  for (host = 0; host < HowMany; host++)
	      {
	      if (HOSTSEOK(host))
		  retcode[host] = SFTP_MakeRPC1(ConnHandleList[host],&SDescList[host], &req[host]);		
	      }
	}
    else
	/* Multicast MRPC: a whole 'nother ballgame. */
	/* NOTE: MULTICAST MRPC fetch currently has a HACK in it! */
	{
	struct MEntry		*me;
	struct SFTP_Entry	*mse;		/* multicast SE entry */
	SE_Descriptor		*mdesc = NULL;	/* multicast SE descriptor */
	struct SFTP_Entry	*thisse;	/* singlecast SE entry */
	SE_Descriptor		*thisdesc;	/* singlescast SE descriptor */

	/* reacquire MEntry pointer */
	assert(MCast->Mgroup != 0);
	assert((me = rpc2_GetMgrp(&rpc2_LocalHost, &rpc2_LocalPort, MCast->Mgroup, CLIENT)) != NULL);
	assert((mse = (struct SFTP_Entry *)me->SideEffectPtr) != NULL);
	assert(mse->SDesc == NULL);
	if (mse->WhoAmI != SFCLIENT) FAIL_MCRPC1(RPC2_SEFAIL1);

	/* ALL descriptors (for valid connections) must be identical.  We FAIL if they are not. */
	/* A Multicast descriptor is created to parallel the multicast se_entry. */
	for (host = 0; host < HowMany; host++)
	    if (HOSTSEOK(host))
		{
		enum WhichWay xdir;
		thisdesc = &SDescList[host];
		xdir = thisdesc->Value.SmartFTPD.TransmissionDirection;
		assert(xdir == CLIENTTOSERVER || xdir == SERVERTOCLIENT);
		if (mdesc == NULL)	/* initialize multicast SE descriptor */
		    {
		    assert((mdesc = (SE_Descriptor *)malloc(sizeof(SE_Descriptor))) != NULL);
		    memcpy(mdesc, thisdesc, sizeof(SE_Descriptor));
		    }
		else
		    if (!SDescCmp(mdesc, thisdesc)) FAIL_MCRPC1(RPC2_SEFAIL1);
		}
	if (mdesc == NULL) FAIL_MCRPC1(RPC2_SEFAIL2);	/* all connections BAD! */

	/* Initialize the multicast data structures. */
	INIT_SE_DESC(mdesc);
	INIT_SE_ENTRY(mse, mdesc, (me->CurrentPacket));

	/* Initialize the corresponding data structures for the participating connections. */
	for (host = 0; host < HowMany; host++)
	    if (HOSTSEOK(host))
		{
		INIT_SE_DESC(&SDescList[host]);
		assert(RPC2_GetSEPointer(ConnHandleList[host], &thisse) == RPC2_SUCCESS);
		INIT_SE_ENTRY(thisse, &(SDescList[host]), (req[host]));
		mse->McastersStarted++;
		}

	switch(mdesc->Value.SmartFTPD.TransmissionDirection)
	    {
	    case CLIENTTOSERVER:
		/* Attempt to open the file to be stored.  FD is recorded in MC se_entry ONLY! */
		if (sftp_InitIO(mse) < 0)
		    {
		    for (host = 0; host < HowMany; host++)
			if (HOSTSEOK(host)) SDescList[host].LocalStatus = SE_FAILURE;
		    FAIL_MCRPC1(RPC2_SEFAIL1);
		    }

		/* piggyback file if possible; parms are guaranteed to already be there */
		if (SFTP_DoPiggy)
		    {
		    int rc = sftp_AppendFileToPacket(mse, &me->CurrentPacket);
		    switch(rc)
			{
			case -1:				/* system call failure */
			    FAIL_MCRPC1(RPC2_SEFAIL4);

			case -2:				/* file too big to fit */
			    break;

			default:				/* rc == length of file */
			    for (host = 0; host < HowMany; host++)
				if (HOSTSEOK(host))
				    {
				    /* copy packet body in case retries are singlecasted */
				    assert(sftp_AddPiggy(&req[host],(char *)me->CurrentPacket->Header.SEDataOffset, rc, SFTP_MAXPACKETSIZE) == 0);
                                    sftp_Progress(&SDescList[host], rc);
				    sftp_didpiggy++;
				    }
			    break;
			}
		    }
		break;

	    case SERVERTOCLIENT:
		/* Attempt to open the file to be fetched. */
		for (host = 0; host < HowMany; host++)
		    if (HOSTSEOK(host))
			{
			assert(RPC2_GetSEPointer(ConnHandleList[host], &thisse) == RPC2_SUCCESS);
			if (sftp_InitIO(thisse) < 0)
				{
				for (host = 0; host < HowMany; host++)
				    if (HOSTSEOK(host))
					SDescList[host].LocalStatus = SE_FAILURE;
				FAIL_MCRPC1(RPC2_SEFAIL1);
				}
			}
		break;

	    default:
		assert(FALSE);	    /* this was checked above */
	    }
	}

    free(ceaddr);
    return -1;
    }

#undef	HOSTSEOK
#undef	FAIL_MCRPC1
#undef	INIT_SE_DESC
#undef	INIT_SE_ENTRY


long SFTP_MultiRPC2(IN ConnHandle, INOUT SDesc, INOUT Reply)
    RPC2_Handle		ConnHandle;
    SE_Descriptor	*SDesc;
    RPC2_PacketBuffer	*Reply;
    {
    struct SFTP_Entry	*se;
    long		rc;

    say(0, SFTP_DebugLevel, "SFTP_MultiRPC2()\n");

    rc = SFTP_MakeRPC2(ConnHandle, SDesc, Reply);
    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    se->XferState = XferCompleted;

    /* if this was the last good connection in a multicast group, clean up the multicast state */
    if (se->UseMulticast)
	{
	struct CEntry	    *ce;
	struct MEntry	    *me;
	struct SFTP_Entry   *mse;
	int		    i;

	assert((ce = rpc2_GetConn(se->LocalHandle)) != NULL);
	assert((me = ce->Mgrp) != NULL);
	assert((mse = (struct SFTP_Entry *)me->SideEffectPtr) != NULL);

	/* See if everyone has finished. */
	mse->McastersFinished++;
	if (mse->McastersFinished < mse->McastersStarted) return(rc);

	/* Everyone has finished, so clean up the multicast state. */
	say(9, SFTP_DebugLevel, "SFTP_MultiRPC2: cleaning up multicast state\n");
	sftp_vfclose(mse);
	if (mse->PiggySDesc != NULL) sftp_FreePiggySDesc(mse);
	for (i = 0; i < MAXOPACKETS; i++)
	    if (mse->ThesePackets[i] != NULL) SFTP_FreeBuffer(&mse->ThesePackets[i]);
	if (mse->SDesc != NULL)
	    { free(mse->SDesc); mse->SDesc = NULL; }
	mse->SendLastContig = mse->SendMostRecent;
	memset(mse->SendTheseBits, 0, sizeof(int)*BITMASKWIDTH);
	mse->XferState = XferCompleted;
	}

    return(rc);
    }


long SFTP_CreateMgrp(IN MgroupHandle)
    RPC2_Handle MgroupHandle;
    {
    struct MEntry	*me;
    struct SFTP_Entry	*mse;			/* Multicast SFTP Entry */
    RPC2_PeerInfo	*PeerInfo;

    say(0, SFTP_DebugLevel, "SFTP_CreateMgrp()\n");
    assert((me = rpc2_GetMgrp(&rpc2_LocalHost, &rpc2_LocalPort, MgroupHandle, CLIENT)) != NULL);

    /* allocate an SFTP_Entry for the multicast group */
    mse = sftp_AllocSEntry();
    mse->WhoAmI = SFCLIENT;
    mse->LocalHandle = 0;			/* none is relevant */

    /* fill in peer info; can't use RPC2_GetPeerInfo() */
    PeerInfo = &mse->PInfo;
    PeerInfo->RemoteHost = me->IPMHost;		/* structure assignment */
    PeerInfo->RemotePort = me->IPMPort;	/* structure assignment */
    PeerInfo->RemoteSubsys.Tag = RPC2_SUBSYSBYID;
    PeerInfo->RemoteSubsys.Value.SubsysId = me->SubsysId;
    PeerInfo->RemoteHandle = me->MgroupID;
    PeerInfo->SecurityLevel = me->SecurityLevel;
    PeerInfo->EncryptionType = me->EncryptionType;
    PeerInfo->Uniquefier = 0;		/* not used */
    memcpy(PeerInfo->SessionKey, me->SessionKey, sizeof(RPC2_EncryptionKey));

    /* Hopefully we'll either get SFTP parameters, or the other side is a
     * recent version of RPC2 or he'll miss some messages */
    //mse->PeerPort = PeerInfo->RemotePort;
    //mse->PeerPort.Value.InetPortNumber = htons(ntohs(mse->PeerPort.Value.InetPortNumber) + 1);
    mse->PeerPort.Tag = 0;

    /* plug in the SFTP descriptor */
    me->SideEffectPtr = (char *)mse;

    return(RPC2_SUCCESS);
    }


long SFTP_AddToMgrp(IN MgroupHandle, IN ConnHandle, INOUT Request)
    RPC2_Handle		MgroupHandle;
    RPC2_Handle 	ConnHandle;
    RPC2_PacketBuffer	**Request;		/* InitMulticast packet */
    {
    struct MEntry	*me;
    struct SFTP_Entry	*mse;			/* Multicast SFTP Entry */
    struct SFTP_Entry	*sse;			/* Singlecast SFTP_Entry */

    assert((me = rpc2_GetMgrp(&rpc2_LocalHost, &rpc2_LocalPort, MgroupHandle, CLIENT)) != NULL);
    assert((mse = (struct SFTP_Entry *)me->SideEffectPtr) != NULL);
    assert(RPC2_GetSEPointer(ConnHandle, &sse) == RPC2_SUCCESS);

    MC_AppendParmsToPacket(mse, sse, Request);
    return(RPC2_SUCCESS);
    }


/* This is effectively a combination SE_CreateMgrp/SE_AddToMgrp call for the server */
long SFTP_InitMulticast(IN MgroupHandle, IN ConnHandle, IN Request)
    RPC2_Handle 	MgroupHandle;
    RPC2_Handle 	ConnHandle;
    RPC2_PacketBuffer	*Request;
    {
    struct CEntry	*ce;
    struct SFTP_Entry	*sse;			/* Singlecast SFTP Entry */
    struct MEntry	*me;
    struct SFTP_Entry	*mse;			/* Multicast SFTP Entry */

    say(0, SFTP_DebugLevel, "SFTP_InitMulticast()\n");
    assert((ce = rpc2_GetConn(ConnHandle)) != NULL);
    assert((me = rpc2_GetMgrp(&ce->PeerHost, &ce->PeerPort, MgroupHandle, SERVER)) != NULL);
    assert(RPC2_GetSEPointer(ConnHandle, &sse) == RPC2_SUCCESS);

    /* Allocate and initialize the MULTICAST parameter block. */
    mse = sftp_AllocSEntry();
    mse->WhoAmI = SFSERVER;
    mse->LocalHandle = MgroupHandle;

    if (MC_ExtractParmsFromPacket(mse, sse, Request) < 0)
	{
	free(mse);
	return(RPC2_SEFAIL1);
	}

    /* Fill in peer info; get SessionKey from Mgrp, not Conn */
    RPC2_GetPeerInfo(ConnHandle, &mse->PInfo);
    memcpy(mse->PInfo.SessionKey, me->SessionKey, sizeof(RPC2_EncryptionKey));

    /* Plug in the SFTP Entry */
    me->SideEffectPtr = (char *)mse;

    return(RPC2_SUCCESS);
    }


long SFTP_DeleteMgrp(IN MgroupHandle, IN ClientHost, IN ClientPort, IN Role)
    RPC2_Handle		MgroupHandle;
    RPC2_HostIdent	*ClientHost;
    RPC2_PortIdent	*ClientPort;
    long		Role;
    {
    struct MEntry	*me;
    struct SFTP_Entry	*mse;			/* Multicast SFTP Entry */
    int			i;

    say(0, SFTP_DebugLevel, "SFTP_DeleteMgrp()\n");
    assert(ClientHost->Tag == RPC2_HOSTBYINETADDR && ClientPort->Tag == RPC2_PORTBYINETNUMBER);
    assert((me = rpc2_GetMgrp(ClientHost, ClientPort, MgroupHandle, Role)) != NULL);

    /* ...below is taken from SFTP_Unbind()... */
    if ((mse = (struct SFTP_Entry *)me->SideEffectPtr) != NULL)
	{
	sftp_vfclose(mse);
	if (mse->PiggySDesc != NULL) sftp_FreePiggySDesc(mse);
	for (i = 0; i < MAXOPACKETS; i++)
	    if (mse->ThesePackets[i] != NULL) SFTP_FreeBuffer(&mse->ThesePackets[i]);
	free(mse);
	me->SideEffectPtr = NULL;
	}

    return(RPC2_SUCCESS);
    }


/*------------------------------------------------------------------------------*/

int SFXlateMcastPacket(RPC2_PacketBuffer *pb)
    {
    struct MEntry  	*me;
    struct CEntry  	*ce;
    struct SFTP_Entry	*mse;			/* Multicast SFTP Entry */
    struct SFTP_Entry	*sse;			/* Singlecast SFTP Entry */
    long    h_RemoteHandle = ntohl(pb->Header.RemoteHandle),
	    h_LocalHandle = ntohl(pb->Header.LocalHandle),
	    h_Flags = ntohl(pb->Header.Flags),
	    h_Opcode,						/* decrypt first */
	    h_SeqNumber,					/* decrypt first */
	    h_ThisRPCCall;					/* decrypt first */
    RPC2_PortIdent		XlatePort;

    XlatePort = pb->Prefix.PeerPort;		/* structure assignment */

    say(9, SFTP_DebugLevel, "SFXlateMcastPacket()\n");
    say(9, SFTP_DebugLevel, "Host = %s\tPort = 0x%x\tMgrp = 0x%lx\n",
	inet_ntoa(pb->Prefix.PeerHost.Value.InetAddress),
	(unsigned) XlatePort.Value.InetPortNumber - 1, h_RemoteHandle);

    /* Find and validate the relevant data structures */
    assert(h_RemoteHandle != 0 && h_LocalHandle == 0);
    assert(XlatePort.Tag == RPC2_PORTBYINETNUMBER);
    XlatePort.Value.InetPortNumber = htons(ntohs(XlatePort.Value.InetPortNumber) - 1);
    me = rpc2_GetMgrp(&pb->Prefix.PeerHost, &XlatePort, h_RemoteHandle, SERVER);
    if (me == NULL) {
	say(9, SFTP_DebugLevel, "me == NULL\n");
	return(FALSE);
    }
    ce = me->conn;
    assert(ce != NULL && TestRole(ce, SERVER) && ce->Mgrp == me);
    if ((sse = (struct SFTP_Entry *)ce->SideEffectPtr) == NULL) {
	say(9, SFTP_DebugLevel, "sse == NULL\n"); return(FALSE);}
    if (sse->WhoAmI != SFSERVER) {
	say(9, SFTP_DebugLevel, "sse->WhoAmI != SFSERVER\n"); return(FALSE);}
    if ((mse = (struct SFTP_Entry *)me->SideEffectPtr) == NULL) {
	say(9, SFTP_DebugLevel, "mse == NULL\n"); return(FALSE);}
    if (mse->WhoAmI != SFSERVER) {
	say(9, SFTP_DebugLevel, "mse->WhoAmI != SFSERVER\n"); return(FALSE);}

    say(9, SFTP_DebugLevel, "Host = %s\tPort = 0x%x\tMgrp = 0x%lx\n",
	    inet_ntoa(pb->Prefix.PeerHost.Value.InetAddress),
	    XlatePort.Value.InetPortNumber, h_RemoteHandle );

    /* Decrypt the packet with the MULTICAST session key. Clear the encrypted bit so that we don't
	decrypt again with the connection session key. */
    if (h_Flags & RPC2_ENCRYPTED)
	{ sftp_Decrypt(pb, mse); pb->Header.Flags = htonl(h_Flags &= ~RPC2_ENCRYPTED); }

    /* Translate the Remote and Local Handles, the SeqNumber, and ThisRPCCall to the singlecast
       channel.  The only (currently) valid opcode for multicasting is SFTP_DATA.  The algorithm for
       SeqNumber and ThisRPCCall translation (for DATA packets) is to apply the difference in the
       incoming multicast SeqNumber (ThisRPCCall) and the multicast RecvLastContig (NextSeqNumber)
       marker to the singlecast RecvLastContig (NextSeqNumber) marker.*/
    pb->Header.RemoteHandle = htonl(ce->UniqueCID);
    pb->Header.LocalHandle = htonl(ce->PeerHandle);
    h_Opcode = ntohl(pb->Header.Opcode);
    assert(h_Opcode == SFTP_DATA);
    h_SeqNumber = ntohl(pb->Header.SeqNumber);
    pb->Header.SeqNumber = htonl(sse->RecvLastContig + (h_SeqNumber - mse->RecvLastContig));
    h_ThisRPCCall = ntohl(pb->Header.ThisRPCCall);
    pb->Header.ThisRPCCall = htonl(ce->NextSeqNumber + (h_ThisRPCCall - me->NextSeqNumber));
    say(9, SFTP_DebugLevel, "pb->SN = %lu\tsse->RLC = %ld\tmse->RLC = %ld\n",
	    (unsigned long)ntohl(pb->Header.SeqNumber), sse->RecvLastContig,
	    mse->RecvLastContig);

    /* Leave the Multicast flag set so that multicast state can be updated later */
    return(TRUE);
    }


int MC_CheckAckorNak(struct SFTP_Entry *whichEntry)
{
    struct CEntry	*ce;
    struct MEntry	*me;
    struct SFTP_Entry	*mse;

    say(9, SFTP_DebugLevel, "MC_CheckAckorNak()\n");
    assert((ce = rpc2_GetConn(whichEntry->LocalHandle)) != NULL);
    assert((me = ce->Mgrp) != NULL);
    assert((mse = (struct SFTP_Entry *)me->SideEffectPtr) != NULL);

    whichEntry->RepliedSinceLastSS = TRUE;

    return(MC_SendStrategy(me, mse));
}


int MC_CheckStart(struct SFTP_Entry *whichEntry)
{
    struct CEntry	*ce, *thisce;
    struct MEntry	*me;
    struct SFTP_Entry	*mse, *thisse;
    int			host;

    say(9, SFTP_DebugLevel, "MC_CheckStart()\n");
    assert((ce = rpc2_GetConn(whichEntry->LocalHandle)) != NULL);
    assert((me = ce->Mgrp) != NULL);
    assert((mse = (struct SFTP_Entry *)me->SideEffectPtr) != NULL);

    whichEntry->RepliedSinceLastSS = TRUE;

    if (mse->XferState == XferNotStarted)
	{
	/* For multicast STOREs we must wait until we have received STARTs on ALL GOOD
	    conns before calling MC_SendStrategy.  If we don't yet have ALL replies, we send out an
	    SFTP_BUSY to keep the START sender from timing us out while we wait for his colleagues. */
	for (host = 0; host < me->howmanylisteners; host++)
	    {
	    assert((thisce = me->listeners[host]) != NULL);
	    assert((thisse = (struct SFTP_Entry *)thisce->SideEffectPtr) != NULL);
	    if (TestState(thisce, CLIENT, ~C_HARDERROR) &&
		thisse->WhoAmI == SFCLIENT &&
		thisse->XferState == XferNotStarted)
		{ SFSendBusy(whichEntry); return(0); }  /* still waiting on someone */
	    }

	/* SFTP_START has now been received on all GOOD connections. */
	mse->XferState = XferInProgress;
	}

    return(MC_SendStrategy(me, mse));
}


static int MC_SendStrategy(me, mse)
    struct MEntry	*me;
    struct SFTP_Entry	*mse;
    {
    struct CEntry	*thisce;
    struct SFTP_Entry	*thisse;
    int			host;
    int			word;
    int			i;
    long		mgrpSendCount = mse->SendLastContig - mse->SendFirst + 1;
    long		minSendCount = -1;
    long		maxSendCount = -1;
    long		thisSendCount;
    long		diffSendCount;
    unsigned long	CompositeSTB[BITMASKWIDTH], TempSTB[BITMASKWIDTH];
    long		ms;
    struct timeval	tt;
    long		AllReplied = TRUE;

    say(9, SFTP_DebugLevel, "MC_SendStrategy()\n");

    /* we update the multicast values of SE transmission state here as follows:
	(MC)SendLastContig: min(normalized SendLastContig) for all GOOD conns;
	(MC)SendTheseBits: LOGICAL AND (normalized SendTheseBits) for all GOOD conns;

	Invariants:
    */

    /* set CompositeSTB = '111...111' */
    for (word = 0; word < BITMASKWIDTH; word++)
	CompositeSTB[word] = 0xFFFFFFFF;		/* 32-bit long's assumed! */

    /* normalize the per-connection values and form the Mgrp composites */
    for (host = 0; host < me->howmanylisteners; host++)
    {
	assert((thisce = me->listeners[host]) != NULL);
	assert((thisse = (struct SFTP_Entry *)thisce->SideEffectPtr) != NULL);
	if (TestState(thisce, CLIENT, ~C_HARDERROR) && thisse->WhoAmI == SFCLIENT)
	{
	    thisSendCount = (thisse->SendLastContig - thisse->SendFirst + 1);
	    diffSendCount = (thisSendCount - mgrpSendCount);
	    assert(diffSendCount >= 0 && diffSendCount <= MAXOPACKETS);

	    /* (minSendCount - mgrpSendCount) is the amount we can increase mse->SendLastContig */
	    if (minSendCount ==	-1)	    /* first good connection */
		{
		minSendCount = thisSendCount;
		maxSendCount = thisSendCount;
		}
	    else
		{
		if (thisSendCount < minSendCount) minSendCount = thisSendCount;
		if (thisSendCount > maxSendCount) maxSendCount = thisSendCount;
		}

	    /* form a composite of the per-connection SendTheseBits masks */
	    B_Assign(TempSTB, thisse->SendTheseBits);
	    if (diffSendCount > 0) B_ShiftRight(TempSTB, diffSendCount);
	    B_And(CompositeSTB, TempSTB);

	    /* update the XferState for each connection */
	    if (mse->HitEOF && mse->ReadAheadCount == 0 &&
		thisse->SendMostRecent == thisse->SendLastContig) {
		thisse->XferState = XferCompleted;
	    } else {
		thisse->XferState = XferInProgress;
		/* note whether or not all (InProgress) connections have replied since the last SS */
		if (!thisse->RepliedSinceLastSS) AllReplied = FALSE;
	    }
	}
    }
    if (minSendCount ==	-1) return(-1);	    /* all connections are BAD! */

    /* finish the normalization and safety check it */
    diffSendCount = minSendCount - mgrpSendCount;
    for (i = 1; i <= diffSendCount; i++) assert(TESTBIT(CompositeSTB, i));
    if (diffSendCount > 0) B_ShiftLeft(CompositeSTB, diffSendCount);
    assert(!TESTBIT(CompositeSTB, 1));

    /* update the Mgrp parameters (finally) */
    say(9, SFTP_DebugLevel, "mse->SLC = %ld\n", mse->SendLastContig );
    mse->SendLastContig += diffSendCount;
    B_Assign(mse->SendTheseBits, CompositeSTB);
    say(9, SFTP_DebugLevel, "mse->SLC = %ld\n", mse->SendLastContig );

    /* we can now free those packets that we incremented SendLastContig over */
    for (i = 0; i < diffSendCount; i++)
	SFTP_FreeBuffer(&mse->ThesePackets[PBUFF((mse->SendLastContig - i))]);

    /* only call sftp_SendStrategy if we have something to do */
    if (mse->HitEOF && mse->ReadAheadCount == 0 && 
	 mse->SendMostRecent == mse->SendLastContig)
	return(0);

    /* only send more if all (good) connections have replied since last
     * invocation of sftp_SendStrategy or if timeout has been exceeded */
    FT_GetTimeOfDay(&tt, 0);
    ms = 1000*(tt.tv_sec - mse->LastSS.tv_sec) + (tt.tv_usec - mse->LastSS.tv_usec)/1000;
    if ((AllReplied && (maxSendCount - minSendCount < mse->SendAhead)) || ms > 8000)
        {
	if(ms > 8000)
		sftp_MSent.Timeouts++;
	/* reset per-connection reply-received flags */
	for (host = 0; host < me->howmanylisteners; host++)
	    {
	    assert((thisce = me->listeners[host]) != NULL);
	    assert((thisse = (struct SFTP_Entry *)thisce->SideEffectPtr) != NULL);
	    if (TestState(thisce, CLIENT, ~C_HARDERROR) && thisse->WhoAmI == SFCLIENT)
		thisse->RepliedSinceLastSS = FALSE;
	    }

	/* now we can actually send packets */
	if (sftp_SendStrategy(mse) < 0) return(-1);

	/* There must be at least ONE outstanding unacked packet at this point */
	if (mse->SendMostRecent == mse->SendLastContig) return(-1);
        }

    return(0);
    }


static int SDescCmp(desc1, desc2)
    SE_Descriptor *desc1, *desc2;
    {
    struct SFTP_Descriptor *ftpd1 = &desc1->Value.SmartFTPD;
    struct SFTP_Descriptor *ftpd2 = &desc2->Value.SmartFTPD;

    if (desc1->Tag != desc2->Tag ||
	ftpd1->TransmissionDirection != ftpd2->TransmissionDirection ||
	ftpd1->hashmark != ftpd2->hashmark ||
	ftpd1->SeekOffset != ftpd2->SeekOffset ||
	ftpd1->ByteQuota != ftpd2->ByteQuota ||
	 ftpd1->Tag != ftpd2->Tag)
    { say(9, SFTP_DebugLevel, "SDescCmp: FAILED\n"); return(FALSE); }

    if (ftpd1->Tag == FILEBYNAME)
	{
	if (ftpd1->FileInfo.ByName.ProtectionBits != ftpd2->FileInfo.ByName.ProtectionBits ||
	    strncmp(ftpd1->FileInfo.ByName.LocalFileName, ftpd2->FileInfo.ByName.LocalFileName, sizeof(ftpd1->FileInfo.ByName.LocalFileName)) != 0)
    { say(9, SFTP_DebugLevel, "SDescCmp: FAILED\n"); return(FALSE); }
	}
    else	/* ftpd1->Tag == FILEBYINODE */
	{
	if (ftpd1->FileInfo.ByInode.Device != ftpd2->FileInfo.ByInode.Device ||
	    ftpd1->FileInfo.ByInode.Inode != ftpd2->FileInfo.ByInode.Inode)
    { say(9, SFTP_DebugLevel, "SDescCmp: FAILED\n"); return(FALSE); }
	}

    return(TRUE);
    }


static void SFSendBusy(struct SFTP_Entry *whichEntry)
{
    RPC2_PacketBuffer *busypb;

    sftp_Sent.Busies++;
    say(9, SFTP_DebugLevel, "SFSendBusy()\n");

    SFTP_AllocBuffer(0, &busypb);
    sftp_InitPacket(busypb, whichEntry, 0);
    busypb->Header.Opcode = SFTP_BUSY;
    rpc2_htonp(busypb);

    sftp_XmitPacket(whichEntry, busypb);
    SFTP_FreeBuffer(&busypb);
}


static void MC_AppendParmsToPacket(mse, sse, req)
    struct SFTP_Entry *mse;
    struct SFTP_Entry *sse;
    RPC2_PacketBuffer **req;
{
    struct SFTP_MCParms mcp;

    /*
     * We piggyback multicast connection parameters on the packet and force the
     * server to update (or initialize) its SINGLECAST parameter block with
     * them.  This serves two purposes:
     * 1- all servers which we multicast to have the same parameters (essential
     *    only for some parms, such as packet size)
     * 2- ensures that all (good) connections have parameters so that we can
     *    multicast to them without first having to do a singlecast
     */

    sftp_AppendParmsToPacket(mse, req);
    sse->SentParms = TRUE;	/* installed in server's SINGLECAST parm block */

    /* We also piggyback state information that is necessary to initialize the MULTICAST parameter
	block; currently this is only PeerSendLastContig (PeerCtrlSeqNumber may be added later). */
    mcp.PeerSendLastContig = htonl(mse->SendLastContig);
    assert(sftp_AddPiggy(req, (char *)&mcp, sizeof(struct SFTP_MCParms),
			 RPC2_MAXPACKETSIZE) == 0);
    }


static int MC_ExtractParmsFromPacket(mse, sse, req)
    struct SFTP_Entry *mse;
    struct SFTP_Entry *sse;
    RPC2_PacketBuffer *req;
    {
    struct SFTP_MCParms mcp;

    /* Extract information from the InitMulticast packet.  There are two things we are interested in:
	    1/ the piggybacked parameters which we install in our SINGLECAST parameter block (even if 
	       we already have a valid set of parms),
	    2/ other piggybacked MULTICAST state information which we install in our MULTICAST
	       parameter block.
	The only item currently in the latter category is PeerSendLastContig which we record as our
	RecvLastContig (later we may add a PeerCtrSeqNumber). */
    if (req->Header.BodyLength - req->Header.SEDataOffset < sizeof(struct SFTP_MCParms))
	return(-1);
    memcpy(&mcp, &req->Body[req->Header.BodyLength-sizeof(struct SFTP_MCParms)],
	   sizeof(struct SFTP_MCParms));
    mse->RecvLastContig = ntohl(mcp.PeerSendLastContig);
    req->Header.BodyLength -= sizeof(struct SFTP_MCParms);
    
    /* Now it's safe to extract the SINGLECAST parameter block. */
    return(sftp_ExtractParmsFromPacket(sse, req));	/* sse->GotParms set TRUE here */
    }
