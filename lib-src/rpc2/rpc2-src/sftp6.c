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
#include <unistd.h>
#include "rpc2.private.h"
#include <rpc2/se.h>
#include <rpc2/sftp.h>


/* sftp5.c */
extern void B_ShiftLeft();
extern void B_ShiftRight();
extern void B_Assign();
extern void B_And();

static void MC_AppendParmsToPacket(struct SFTP_Entry *mse,
				   struct SFTP_Entry *sse,
				   RPC2_PacketBuffer **req);
static int MC_ExtractParmsFromPacket(struct SFTP_Entry *mse,
				     struct SFTP_Entry *sse,
				     RPC2_PacketBuffer *req);

/*----------------------- The procs below interface directly with RPC2 ------------------------ */

long SFTP_MultiRPC1(IN HowMany, IN ConnHandleList, INOUT SDescList, INOUT req, INOUT retcode)
    int			HowMany;
    RPC2_Handle		ConnHandleList[];
    SE_Descriptor	SDescList[];
    RPC2_PacketBuffer	*req[];
    long		retcode[];
{
    int	host;
    say(1, SFTP_DebugLevel, "SFTP_MultiRPC1()\n");

    /* simply iterate over the set of hosts calling SFTP_MakeRPC1() */
    for (host = 0; host < HowMany; host++) {
	if (retcode[host] <= RPC2_ELIMIT || SDescList[host].Tag == OMITSE)
	    continue;

	retcode[host] = SFTP_MakeRPC1(ConnHandleList[host],
				      &SDescList[host], &req[host]);
    }
    return -1;
}

long SFTP_MultiRPC2(IN ConnHandle, INOUT SDesc, INOUT Reply)
    RPC2_Handle		ConnHandle;
    SE_Descriptor	*SDesc;
    RPC2_PacketBuffer	*Reply;
    {
    struct SFTP_Entry	*se;
    long		rc;

    say(1, SFTP_DebugLevel, "SFTP_MultiRPC2()\n");

    rc = SFTP_MakeRPC2(ConnHandle, SDesc, Reply);
    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    se->XferState = XferCompleted;

    return(rc);
    }


long SFTP_CreateMgrp(IN MgroupHandle)
    RPC2_Handle MgroupHandle;
    {
    struct MEntry	*me;
    struct SFTP_Entry	*mse;			/* Multicast SFTP Entry */
    RPC2_PeerInfo	*PeerInfo;

    say(1, SFTP_DebugLevel, "SFTP_CreateMgrp()\n");
    assert((me = rpc2_GetMgrp(NULL, MgroupHandle, CLIENT)) != NULL);

    /* allocate an SFTP_Entry for the multicast group */
    mse = sftp_AllocSEntry();
    mse->WhoAmI = SFCLIENT;
    mse->LocalHandle = 0;			/* none is relevant */

    /* fill in peer info; can't use RPC2_GetPeerInfo() */
    PeerInfo = &mse->PInfo;
    memset(PeerInfo, 0, sizeof(RPC2_PeerInfo));
    PeerInfo->RemoteSubsys.Tag = RPC2_DUMMYSUBSYS;
    PeerInfo->RemoteHandle = me->MgroupID;
    PeerInfo->RemoteHost.Tag = RPC2_DUMMYHOST; /* was INADDR_ANY:2432 */
    PeerInfo->Uniquefier = 0;		/* not used */

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

    assert((me = rpc2_GetMgrp(NULL, MgroupHandle, CLIENT)) != NULL);
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
    int ret;

    say(1, SFTP_DebugLevel, "SFTP_InitMulticast()\n");
    ce = rpc2_GetConn(ConnHandle);
    assert(ce != NULL);

    me = rpc2_GetMgrp(ce->HostInfo->Addr, MgroupHandle, SERVER);
    assert(me != NULL);

    ret = RPC2_GetSEPointer(ConnHandle, &sse);
    assert(ret == RPC2_SUCCESS);

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

    /* Depending on rpc2_ipv6ready, rpc2_splitaddrinfo might return a simple
     * IPv4 address. Convert it back to the more useful RPC2_addrinfo... */
    rpc2_simplifyHost(&mse->PInfo.RemoteHost, &mse->PInfo.RemotePort);

    /* Plug in the SFTP Entry */
    me->SideEffectPtr = (char *)mse;

    return(RPC2_SUCCESS);
    }


long SFTP_DeleteMgrp(RPC2_Handle MgroupHandle, struct RPC2_addrinfo *ClientAddr,
		     long Role)
    {
    struct MEntry	*me;
    struct SFTP_Entry	*mse;			/* Multicast SFTP Entry */
    int			i;

    say(1, SFTP_DebugLevel, "SFTP_DeleteMgrp()\n");
    assert((me = rpc2_GetMgrp(ClientAddr, MgroupHandle, Role)) != NULL);

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

static void MC_AppendParmsToPacket(struct SFTP_Entry *mse,
				   struct SFTP_Entry *sse,
				   RPC2_PacketBuffer **req)
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


static int MC_ExtractParmsFromPacket(struct SFTP_Entry *mse,
				     struct SFTP_Entry *sse,
				     RPC2_PacketBuffer *req)
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
