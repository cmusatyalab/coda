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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/rpc2/sftp1.c,v 1.1 1996/11/22 19:07:35 braam Exp $";
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
    SFTP: a smart file transfer protocol using windowing and piggybacking
    sftp1.c contains the SFTP interface to RPC2 

*/


#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include "lwp.h"
#include "timer.h"
#include "rpc2.h"
#include "rpc2.private.h"
#include "se.h"
#include "sftp.h"


/*----------------------- Local procedure specs  ----------------------*/
PRIVATE long GetFile();
PRIVATE long PutFile();
PRIVATE RPC2_PacketBuffer *AwaitPacket();
PRIVATE void AddTimerEntry();
PRIVATE long MakeBigEnough();

/*---------------------------  Local macros ---------------------------*/
#define FAIL(whichS, rCode)\
	    {\
	    if (whichS->openfd >= 0) CLOSE(whichS);\
	    return(rCode);\
	    }

#define QUIT(se, RC1, RC2)\
    se->SDesc->LocalStatus = RC1;\
    if (se->openfd >= 0) CLOSE(se);\
    return(RC2);


#define BOGUS(pb)\
    (sftp_TraceBogus(1, __LINE__), sftp_bogus++, SFTP_FreeBuffer(&pb))



/*------------- Procedures directly invoked by RPC2 ---------------*/

long SFTP_Init()
    {
    char *sname;
    
    say(0, SFTP_DebugLevel, ("SFTP_Init()\n"));

    /* Create socket for SFTP packets */
    if (rpc2_CreateIPSocket(&sftp_Socket, &sftp_Host, &sftp_Portal) != RPC2_SUCCESS)
	return(RPC2_FAIL);

    /* Create SFTP listener process */
    sname = "sftp_Listener";
    LWP_CreateProcess((PFIC)sftp_Listener, 8192, LWP_NORMAL_PRIORITY, sname, sname, &sftp_ListenerPID);
    sftp_InitTrace();
    return (RPC2_SUCCESS);
    }

void SFTP_SetDefaults(initPtr)
    register SFTP_Initializer *initPtr;
    /* Should be called before SFTP_Activate() */
    {
    initPtr->PacketSize = SFTP_DEFPACKETSIZE;
    initPtr->WindowSize = SFTP_DEFWINDOWSIZE;
    initPtr->RetryCount = 6;
    initPtr->RetryInterval = 2000;	/* milliseconds */
    initPtr->SendAhead = SFTP_DEFSENDAHEAD;   /* make sure it is less than 16, for readv() */
    initPtr->AckPoint = SFTP_DEFSENDAHEAD;    /* same as SendAhead */
    initPtr->EnforceQuota = 0;
    initPtr->DoPiggy = TRUE;
    initPtr->DupThreshold = 16;
    initPtr->MaxPackets = -1;
    initPtr->Portal.Tag = RPC2_PORTALBYINETNUMBER;
    initPtr->Portal.Value.InetPortNumber = htons(0);
    }


void SFTP_Activate(initPtr)
    register SFTP_Initializer *initPtr;
    /* Should be called before RPC2_Init() */
    {
    register struct SE_Definition *sed;
    register long size;

    if (initPtr != NULL)
	{
	SFTP_PacketSize = initPtr->PacketSize;
	SFTP_WindowSize= initPtr->WindowSize;
	SFTP_RetryCount = initPtr->RetryCount;
	SFTP_RetryInterval = initPtr->RetryInterval;	/* milliseconds */
	SFTP_EnforceQuota = initPtr->EnforceQuota;
	SFTP_SendAhead = initPtr->SendAhead;
	SFTP_AckPoint = initPtr->AckPoint;
	SFTP_DoPiggy = initPtr->DoPiggy;
	SFTP_DupThreshold = initPtr->DupThreshold;
	SFTP_MaxPackets = initPtr->MaxPackets;
	sftp_Portal = initPtr->Portal;			/* structure assignment */
	}
    assert(SFTP_SendAhead <= 16);	/* 'cause of readv() bogosity */

    /* Enlarge table by one */
    SE_DefCount++;
    size = sizeof(struct SE_Definition)*SE_DefCount;
    if (SE_DefSpecs == NULL)
	/* The realloc() on the romp dumps core if SE_DefSpecs is NULL */
	assert((SE_DefSpecs = (struct SE_Definition *)malloc(size)) != NULL)
    else
	assert((SE_DefSpecs = (struct SE_Definition *)realloc(SE_DefSpecs, size)) != NULL)

    /* Add this side effect's info to last entry in table */
    sed = &SE_DefSpecs[SE_DefCount-1];
    sed->SideEffectType = SMARTFTP;
    sed->SE_Init = SFTP_Init;
    sed->SE_Bind1 = SFTP_Bind1;
    sed->SE_Bind2 = SFTP_Bind2;
    sed->SE_Unbind = SFTP_Unbind;
    sed->SE_NewConnection = SFTP_NewConn;
    sed->SE_MakeRPC1 = SFTP_MakeRPC1;
    sed->SE_MakeRPC2 = SFTP_MakeRPC2;
    sed->SE_MultiRPC1 = SFTP_MultiRPC1;
    sed->SE_MultiRPC2 = SFTP_MultiRPC2;
    sed->SE_CreateMgrp = SFTP_CreateMgrp;
    sed->SE_AddToMgrp = SFTP_AddToMgrp;
    sed->SE_InitMulticast = SFTP_InitMulticast;
    sed->SE_DeleteMgrp = SFTP_DeleteMgrp;
    sed->SE_GetRequest = SFTP_GetRequest;
    sed->SE_InitSideEffect = SFTP_InitSE;
    sed->SE_CheckSideEffect = SFTP_CheckSE;
    sed->SE_SendResponse = SFTP_SendResponse;
    sed->SE_PrintSEDescriptor = SFTP_PrintSED;
    sed->SE_GetSideEffectTime = SFTP_GetTime;
    sed->SE_GetHostInfo = SFTP_GetHostInfo;
    }


long SFTP_Bind1(IN ConnHandle, IN ClientIdent)
    RPC2_Handle ConnHandle;
    RPC2_CountedBS *ClientIdent;
    {
    register struct SFTP_Entry *se;

    say(0, SFTP_DebugLevel, ("SFTP_Bind()\n"));

    se = sftp_AllocSEntry();	/* malloc and initialize SFTP_Entry */
    se->WhoAmI = SFCLIENT;
    se->LocalHandle = ConnHandle;
    RPC2_SetSEPointer(ConnHandle, se);
    return(RPC2_SUCCESS);
    }


long SFTP_Bind2(IN ConnHandle, IN BindTime)
    RPC2_Handle ConnHandle;
    RPC2_Unsigned BindTime;
    {
    struct SFTP_Entry *se;

    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    RPC2_GetPeerInfo(ConnHandle, &se->PInfo);
    if (BindTime) sftp_InitRTT(BindTime, se);
    se->HostInfo = rpc2_GetHostByType(&se->PInfo.RemoteHost, SMARTFTP_HE);
                   /* guess at host info until peer portal is known */
    
    return(RPC2_SUCCESS);
    }


long SFTP_Unbind(IN ConnHandle)
    RPC2_Handle ConnHandle;
    {
    struct SFTP_Entry *se;

    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    if (se) sftp_FreeSEntry(se);
    RPC2_SetSEPointer(ConnHandle, NULL);
    return(RPC2_SUCCESS);
    }
    

long SFTP_NewConn(IN ConnHandle, IN ClientIdent)
    RPC2_Handle ConnHandle;
    RPC2_CountedBS *ClientIdent;
    {
    register struct SFTP_Entry *se;

    say(0, SFTP_DebugLevel, ("SFTP_NewConn()\n"));

    se = sftp_AllocSEntry();	/* malloc and initialize */
    se->WhoAmI = SFSERVER;
    se->LocalHandle = ConnHandle;
    RPC2_GetPeerInfo(ConnHandle, &se->PInfo);
    se->HostInfo = rpc2_GetHostByType(&se->PInfo.RemoteHost, SMARTFTP_HE);
                   /* take a guess at host info until peer portal is known */
    RPC2_SetSEPointer(ConnHandle, se);

    return(RPC2_SUCCESS);    
    }


long SFTP_MakeRPC1(IN ConnHandle, INOUT SDesc, INOUT RequestPtr)
    RPC2_Handle ConnHandle;
    SE_Descriptor *SDesc;
    RPC2_PacketBuffer **RequestPtr;
    {
    struct SFTP_Entry *se;
    int rc;

    say(0, SFTP_DebugLevel, ("SFTP_MakeRPC1 ()\n"));

    SDesc->LocalStatus = SE_SUCCESS;	/* non-execution == success */
    SDesc->RemoteStatus = SE_SUCCESS;	/* non-execution == success */
    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    if (se->WhoAmI != SFCLIENT) FAIL(se, RPC2_SEFAIL2);
    se->ThisRPCCall = (*RequestPtr)->Header.SeqNumber;	/* remember new call has begun */
    se->SDesc = SDesc;
    SDesc->Value.SmartFTPD.BytesTransferred = 0;

    se->XferState = XferNotStarted;
    se->UseMulticast = FALSE;
    se->HitEOF = FALSE;
    if (SDesc->Value.SmartFTPD.TransmissionDirection == CLIENTTOSERVER)
	{
	se->SendMostRecent = se->SendLastContig;
	se->SendWorriedLimit = se->SendLastContig;
	se->SendAckLimit = se->SendLastContig;
	bzero(se->SendTheseBits, BITMASKWIDTH*sizeof(long));
	se->ReadAheadCount = 0;
	rc = sftp_InitIO(se);
	}
    else
	{
	se->RecvMostRecent = se->RecvLastContig;
	bzero(se->RecvTheseBits, BITMASKWIDTH*sizeof(long));
	rc = sftp_InitIO(se);
	}
    if (rc < 0)
	{ SDesc->LocalStatus = SE_FAILURE; FAIL(se, RPC2_SEFAIL1); }

    /* Piggyback SFTP parms if this is the very first call */
    if (se->SentParms == FALSE)
	{
	rc = sftp_AppendParmsToPacket(se, RequestPtr);
	if (rc < 0) FAIL(se, RPC2_SEFAIL4);	
	}
    else
	/* Try piggybacking file on store */
	if (SDesc->Value.SmartFTPD.TransmissionDirection == CLIENTTOSERVER && SFTP_DoPiggy)
	    {
	    rc = sftp_AppendFileToPacket(se, RequestPtr);
	    switch (rc)
		{
		case -1:	FAIL(se, RPC2_SEFAIL4);
		
		case -2:	break;	/* file too big to fit */
		
		default:	SDesc->Value.SmartFTPD.BytesTransferred = rc;
				sftp_didpiggy++;
				break;
		}
	    }

    return(RPC2_SUCCESS);    
    }


long SFTP_MakeRPC2(IN ConnHandle, INOUT SDesc, INOUT Reply)
    RPC2_Handle ConnHandle;
    SE_Descriptor *SDesc;
    RPC2_PacketBuffer *Reply;
    {
    struct SFTP_Entry *se;
    register int i, nbytes;

    say(0, SFTP_DebugLevel, ("SFTP_MakeRPC2()\n"));
    
    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);

    SDesc->LocalStatus = SDesc->RemoteStatus = SE_SUCCESS; /* tentatively */

    /* Pluck off  file if piggybacked  */
    if (Reply != NULL && (Reply->Header.SEFlags & SFTP_PIGGY)
	    && SDesc->Value.SmartFTPD.TransmissionDirection == SERVERTOCLIENT)
	{
	nbytes = sftp_ExtractFileFromPacket(se, Reply);
	if (nbytes >= 0)
	    {
	    sftp_didpiggy++;
	    SDesc->Value.SmartFTPD.BytesTransferred = nbytes;
	    }
	else
	    {
	    SDesc->LocalStatus = SE_FAILURE;
	    sftp_SetError(se, DISKERROR);	/* what else could it be? */
	    }
	}

    /* Clean up local state */
    for (i = 0; i < MAXOPACKETS; i++)
	if (se->ThesePackets[i] != NULL) SFTP_FreeBuffer(&se->ThesePackets[i]);
    if (se->openfd >= 0) CLOSE(se);
    se->SDesc = NULL;
    se->SendLastContig = se->SendMostRecent;
    se->RecvLastContig = se->RecvMostRecent;
    bzero(se->SendTheseBits,sizeof(int)*BITMASKWIDTH);
    bzero(se->RecvTheseBits,sizeof(int)*BITMASKWIDTH);

    /* Determine return code */
    if (Reply == NULL) return (RPC2_SUCCESS);	/* so base RPC2 code will carry on */
    if (se->WhoAmI == DISKERROR)
	{
	SDesc->LocalStatus = SE_FAILURE;
	return(RPC2_SEFAIL3);
	}
    if (se->XferState == XferInProgress && !(Reply->Header.SEFlags & SFTP_ALLOVER))
	{
	sftp_SetError(se, ERROR);
	SDesc->RemoteStatus = SE_FAILURE;
    	return(RPC2_SEFAIL2);
	}

    return(RPC2_SUCCESS);
    }


long SFTP_GetRequest(IN ConnHandle, INOUT Request)
    RPC2_Handle ConnHandle;
    register RPC2_PacketBuffer *Request;
    {
    struct SFTP_Entry *se;
    long len;

    say(0, SFTP_DebugLevel, ("SFTP_GetRequest()\n"));

    assert (RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS &&  se != NULL);
    if (se->WhoAmI != SFSERVER) FAIL(se, RPC2_SEFAIL2);
    se->ThisRPCCall = Request->Header.SeqNumber;   /* acquire client's RPC call number */
    if ((se->RTT == 0) && (Request->Header.BindTime))
	sftp_InitRTT(Request->Header.BindTime, se); /* initialize RTT */

    se->PiggySDesc = NULL; /* default is no piggybacked file */
    if ((Request->Header.SEFlags & SFTP_PIGGY))
	{
	if (se->GotParms == FALSE)
	    {
	    if (sftp_ExtractParmsFromPacket(se, Request) < 0)
		FAIL(se, RPC2_SEFAIL2);
	    }
	else
	    {
	    /* Save file for future InitSE and CheckSE */
	    len = Request->Header.BodyLength - Request->Header.SEDataOffset;
	    sftp_AllocPiggySDesc(se, len, CLIENTTOSERVER);
	    se->SDesc = se->PiggySDesc;
	    assert(sftp_ExtractFileFromPacket(se, Request) >= 0);
	    sftp_didpiggy++;
	    }
	}
    
    return(RPC2_SUCCESS);
    }


long SFTP_InitSE(IN ConnHandle, INOUT SDesc)
    RPC2_Handle ConnHandle;
    SE_Descriptor *SDesc;
    {
    struct SFTP_Entry *se;
    int rc;

    say(0, SFTP_DebugLevel, ("SFTP_InitSE ()\n"));
	
    SDesc->LocalStatus = SE_NOTSTARTED;
    SDesc->RemoteStatus = SE_NOTSTARTED;
    assert (RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS &&  se != NULL);
    if (se->WhoAmI != SFSERVER) FAIL(se, RPC2_SEFAIL2);
    if (se->GotParms == FALSE) FAIL(se, RPC2_SEFAIL2);
    se->SDesc = SDesc;

    rc = sftp_InitIO(se);
    if (rc < 0) 
	{ SDesc->LocalStatus = SE_FAILURE; FAIL(se, RPC2_SEFAIL1); }
    else
	return(RPC2_SUCCESS);
    }


long SFTP_CheckSE(IN ConnHandle, INOUT SDesc, IN Flags)
    RPC2_Handle ConnHandle;
    SE_Descriptor *SDesc;
    long Flags;    
    {
    long rc, flen;
    struct SFTP_Entry *se;
    register struct SFTP_Descriptor *sftpd;
    struct FileInfoByAddr *p;
	

    say(0, SFTP_DebugLevel, ("SFTP_CheckSE()\n"));

    if (Flags == 0) return(RPC2_SUCCESS);
    assert (RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS &&  se != NULL);
    if (se->WhoAmI != SFSERVER) FAIL(se, RPC2_SEFAIL2);
    se->SDesc = SDesc;
    
    if (SDesc->LocalStatus != SE_NOTSTARTED || SDesc->RemoteStatus != SE_NOTSTARTED)
	return(RPC2_SUCCESS);	/* SDesc error conditions are self-describing */
    SDesc->LocalStatus = SDesc->RemoteStatus = SE_INPROGRESS;
    SDesc->Value.SmartFTPD.BytesTransferred = 0;

    sftpd = &SDesc->Value.SmartFTPD;
    if (sftpd->hashmark != 0)
	switch(sftpd->Tag)
	    {
	    case FILEBYNAME:
		say(0, SFTP_DebugLevel, ("%s: ", sftpd->FileInfo.ByName.LocalFileName));
	    	break;

	    case FILEBYINODE:
		say(0, SFTP_DebugLevel, ("%ld.%ld: ", sftpd->FileInfo.ByInode.Device, sftpd->FileInfo.ByInode.Inode));
	    	break;

	    case FILEBYFD:
		say(0, SFTP_DebugLevel, ("%ld: ", sftpd->FileInfo.ByFD.fd));
	    	break;

	    case FILEINVM:
		say(0, SFTP_DebugLevel, ("0x%lx[%ld, %ld]: ", sftpd->FileInfo.ByAddr.vmfile.SeqBody, sftpd->FileInfo.ByAddr.vmfile.MaxSeqLen,  sftpd->FileInfo.ByAddr.vmfile.SeqLen));
	    	break;
	    }

    switch(sftpd->TransmissionDirection)
	{
	case CLIENTTOSERVER:
	    if (se->PiggySDesc)
		{/* use squirrelled-away data */
	    
		p = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;
		rc = sftp_vfwritefile(se->SDesc, se->openfd, p->vmfile.SeqBody, p->vmfile.SeqLen);
		if (rc < 0)
		    {
		    sftp_SetError(se, DISKERROR);
		    se->SDesc->LocalStatus = SE_FAILURE;
		    }
		else
		    {
		    rc = RPC2_SUCCESS;
		    se->SDesc->LocalStatus = SE_SUCCESS;
		    se->SDesc->Value.SmartFTPD.BytesTransferred =  p->vmfile.SeqLen;
		    }
		sftp_FreePiggySDesc(se); /* get rid of saved file data */
		}
	    else
		{/* full-fledged file transfer */
		rc = GetFile(se);
		}
	    break;

	case SERVERTOCLIENT:
	    flen = sftp_vfsize(se->SDesc, se->openfd);
	    if (SFTP_DoPiggy == FALSE
		|| ((Flags & SE_AWAITREMOTESTATUS) != 0)
		|| (flen >= SFTP_MAXBODYSIZE))
		{/* can't defer transfer to SendResponse */
		rc = PutFile(se);
		}
	    else
		{
		/*  Squirrel away contents of file.
		We have to save the data right away, because the
		the server may delete or modify the file after
		CheckSideEffect but before  SendResponse. */

		sftp_AllocPiggySDesc(se, flen, SERVERTOCLIENT);
		p = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;	    
		rc = sftp_vfreadfile(se->SDesc, se->openfd, p->vmfile.SeqBody);
		if (rc < 0)
		    {
		    sftp_SetError(se, DISKERROR);
		    se->SDesc->LocalStatus = SE_FAILURE;
		    }
		else
		    {
		    rc = RPC2_SUCCESS;
		    se->SDesc->LocalStatus = SE_SUCCESS;
		    se->SDesc->Value.SmartFTPD.BytesTransferred =  p->vmfile.SeqLen;
		    }
		}
	    break;

	default: FAIL(se, RPC2_SEFAIL1);
	}

    CLOSE(se);
    return(rc);
    }


long SFTP_SendResponse(IN ConnHandle, IN Reply)
    RPC2_Handle ConnHandle;
    RPC2_PacketBuffer **Reply;
    {
    struct SFTP_Entry *se;
    long rc;

    say(0, SFTP_DebugLevel, ("SFTP_SendResponse()\n"));

    assert (RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS &&  se != NULL);
    
    rc = RPC2_SUCCESS;
    if (se->PiggySDesc)
	{/* Deal with file saved for piggybacking */

	if (se->PiggySDesc->Value.SmartFTPD.TransmissionDirection == SERVERTOCLIENT)
	    {/* File is indeed for the client;  the only other way
	        PiggySDesc can be non-null is when a file is stored
                piggybacked, but the server does not do an 
		InitSideEffect or CheckSideEffect */

	    se->SDesc = se->PiggySDesc;	/* so that PutFile is happy */
	    switch(sftp_AppendFileToPacket(se, Reply))
		{
		case -1:
		    rc = RPC2_SEFAIL4;
		    break;

		case -2:			/* File was too big */
		    rc = PutFile(se);
		    break;

		default:			/* bytes were appended */
		    rc = RPC2_SUCCESS;
		    sftp_didpiggy++;
		    break;
		}

	    }
	sftp_FreePiggySDesc(se);   /* always get rid of the file */
	}

    /* clean up state */
    if (se->openfd >= 0) CLOSE(se);

    if (se->WhoAmI == ERROR)
	{
	(*Reply)->Header.SEFlags  &= ~SFTP_ALLOVER;	/* confirm SE failure */
	return(RPC2_SUCCESS);	/* but allow this SendResponse to get out */
	}
    else
	{
	/* indicate all outstanding packets received */
	(*Reply)->Header.SEFlags |= SFTP_ALLOVER;
	}
    
    return(rc);    
    }


long SFTP_GetTime(IN ConnHandle, INOUT Time) 
    RPC2_Handle ConnHandle;
    struct timeval *Time;
    {
    struct SFTP_Entry *se;
    long rc;

    say(0, SFTP_DebugLevel, ("SFTP_GetTime()\n"));

    se = NULL;
    /* 
     * We might legitimately fail here because there is no connection, 
     * there is a connection but it is not completely set up.
     */
    if ((rc = RPC2_GetSEPointer(ConnHandle, &se)) != RPC2_SUCCESS)
	    return(rc);

    if (se == NULL) return(RPC2_NOCONNECTION);

    *Time = se->LastWord;
    return(RPC2_SUCCESS);
    }


long SFTP_GetHostInfo(IN ConnHandle, INOUT HPtr) 
    RPC2_Handle ConnHandle;
    struct HEntry **HPtr;
    {
    struct SFTP_Entry *se;
    long rc;

    say(0, SFTP_DebugLevel, ("SFTP_GetHostInfo()\n"));

    se = NULL;
    if ((rc = RPC2_GetSEPointer(ConnHandle, &se)) != RPC2_SUCCESS)
	    return(rc);

    if (se == NULL) return(RPC2_NOCONNECTION);

    /* 
     * There may still be no host info.  If not, see if some of
     * the right type has appeared since the bind.
     */
    if (se->HostInfo == NULL) 
	se->HostInfo = rpc2_GetHostByType(&se->PInfo.RemoteHost, SMARTFTP_HE);
	
    *HPtr = se->HostInfo;
    return(RPC2_SUCCESS);
    }




/*-------------------- Data transmission routines -----------------------*/
PRIVATE long GetFile(sEntry)
    register struct SFTP_Entry *sEntry;
    /* Local file is already opened */
    {
    register long i;
    register long startmode = TRUE;
    RPC2_PacketBuffer *pb;
    
    sEntry->XferState = XferInProgress;
    sEntry->SDesc->Value.SmartFTPD.BytesTransferred = 0;
    sEntry->SDesc->Value.SmartFTPD.QuotaExceeded = 0;
    sEntry->HitEOF = FALSE;
    sEntry->RecvMostRecent = sEntry->RecvLastContig;
    sEntry->RecvFirst = sEntry->RecvLastContig + 1;
    bzero(sEntry->RecvTheseBits, sizeof(int)*BITMASKWIDTH);

    if (sftp_SendStart(sEntry) < 0)
    	{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}	/* Initiate */

    while (sEntry->XferState == XferInProgress)
	{
	for (i = 0; i < sEntry->RetryCount; i++)
	    {
	    pb = (RPC2_PacketBuffer *)AwaitPacket(&sEntry->RInterval, sEntry);

	    /* Make sure nothing bad happened while we were waiting */
	    if (sEntry->WhoAmI == ERROR)
		{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}
	    if (sEntry->WhoAmI == DISKERROR)
		{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL3);}

	    /* Did we receive a packet? */
	    if (pb == NULL)
		{
		/* 
		 * don't back off unless we have a rtt estimate. This is 
		 * this is more for dealing with old clients/servers than
		 * anything else.
		 */
		if (sEntry->RTT) sftp_Backoff(sEntry);

		if (startmode)
		    {
		    if (sftp_SendStart(sEntry) < 0)
			{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}
		    }
		else
		    {
		    if (sftp_SendTrigger(sEntry) < 0)
			{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}
		    }
		continue;
		}

	    /* If so process it */
	    switch((int) pb->Header.Opcode)	/* punt CtrlSeqNumber consistency for now */
		{
		case SFTP_NAK:
			assert(FALSE); /* my SEntry state should already be ERRORR */
		case SFTP_DATA:
			goto GotData;

		case SFTP_BUSY:
		        sftp_Recvd.Busies++;
			if (startmode)
			    { sftp_busy++; i = 0; SFTP_FreeBuffer(&pb); }
			else
			    BOGUS(pb);
			continue;

		default:
			BOGUS(pb);
			break;
		}
	    }
	sftp_SetError(sEntry, ERROR);
	QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);

GotData:
	startmode = FALSE;
	if (sftp_DataArrived(pb, sEntry) < 0)
	    {
	    if (sEntry->WhoAmI == DISKERROR)
		{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL3);}
	    else
		{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}
	    }
	}

    QUIT(sEntry, SE_SUCCESS, RPC2_SUCCESS);
    }


PRIVATE long PutFile(sEntry)
    register struct SFTP_Entry *sEntry;
    /* Local file is already opened */
    {
    RPC2_PacketBuffer *pb;
    register long i;

    sEntry->SDesc->Value.SmartFTPD.BytesTransferred = 0;
    sEntry->SDesc->Value.SmartFTPD.QuotaExceeded = 0;
    sEntry->HitEOF = FALSE;
    sEntry->XferState = XferInProgress;
    sEntry->SendMostRecent = sEntry->SendLastContig;	/* Really redundant */
    sEntry->SendWorriedLimit = sEntry->SendLastContig;  /* Really redundant */
    sEntry->SendAckLimit = sEntry->SendLastContig;      /* Really redundant */
    sEntry->TimeEcho = 0;
    bzero(sEntry->SendTheseBits, sizeof(int)*BITMASKWIDTH);

    if (sftp_SendStrategy(sEntry) < 0)
	{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}

    while (sEntry->XferState == XferInProgress)
	{
	for (i = 0; i < sEntry->RetryCount; i++)
	    {
	    pb = (RPC2_PacketBuffer *)AwaitPacket(&sEntry->RInterval, sEntry);

	    /* Make sure nothing bad happened while we were waiting */
	    if (sEntry->WhoAmI == ERROR)
		{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}
	    if (sEntry->WhoAmI == DISKERROR)
		{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL3);}

	    /* Things still look ok */
	    if (pb != NULL)
		goto GotAck;

	    sEntry->TimeEcho = 0;
	    if (sftp_SendStrategy(sEntry) < 0)
		{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}

	    /* 
	     * don't back off unless we have a rtt estimate. This is 
	     * this is more for dealing with old clients/servers than
	     * anything else.
	     */
	    if (sEntry->RTT) sftp_Backoff(sEntry);
	    }
	sftp_SetError(sEntry, ERROR);
	QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
GotAck:
	if (i == 0) /* got an ack on the first try. allow RTT updates now */
	    sEntry->Retransmitting = FALSE;

	switch ((int) pb->Header.Opcode)
	    {
	    case SFTP_NAK:
		assert(FALSE);  /* should have already set sEntry state */

	    case SFTP_ACK:
		if (sftp_AckArrived(pb, sEntry) < 0)
		    {QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}
		break;
	    
	    default:
		SFTP_FreeBuffer(&pb);
		break;	    
	    }
	}

    QUIT(sEntry, SE_SUCCESS, RPC2_SUCCESS);
    }


PRIVATE RPC2_PacketBuffer *AwaitPacket(tOut, sEntry)
    register struct timeval *tOut;
    register struct SFTP_Entry *sEntry;
    
    /* Awaits for a packet on sEntry or  timeout tOut
	Returns pointer to arrived packet or NULL
    */
    {
    struct SLSlot *sls;

    if (LWP_GetRock(SMARTFTP, (char **)&sls) != LWP_SUCCESS)
	{
	sls = (struct SLSlot *) malloc(sizeof(struct SLSlot));
	bzero(sls, sizeof(struct SLSlot));
	sls->Magic = SSLMAGIC;
	LWP_CurrentProcess(&sls->Owner);
	assert(LWP_NewRock(SMARTFTP, (char *)sls) == LWP_SUCCESS);
	}

    sls->TChain = sftp_Chain;
    sls->Te.TotalTime = *tOut;	/* structure assignment */
    sls->Te.BackPointer = (char *)sls;
    sls->State = S_WAITING;
    sEntry->Sleeper = sls;
    AddTimerEntry(&sls->Te);
    LWP_WaitProcess((char *)sls);
    switch(sls->State)
	{
	case S_TIMEOUT:	sls->State = S_INACTIVE; return(NULL);
    
	case S_ARRIVED:	sls->State = S_INACTIVE; return(sls->Packet);
	
	default: assert(FALSE);
	}
    /*NOTREACHED*/
    }
    

PRIVATE void AddTimerEntry(whichElem)
    register struct TM_Elem *whichElem;
    {
    register struct TM_Elem *t;
    register long mytime;

    mytime = whichElem->TotalTime.tv_sec*1000000 + whichElem->TotalTime.tv_usec;
    t = TM_GetEarliest(sftp_Chain);
    if (t == NULL || (t->TimeLeft.tv_sec*1000000 + t->TimeLeft.tv_usec) > mytime)
	IOMGR_Cancel(sftp_ListenerPID);
    TM_Insert(sftp_Chain, whichElem);
    }


/*---------------------- Piggybacking routines -------------------------*/

sftp_AddPiggy(whichP, dPtr, dSize, maxSize)
    RPC2_PacketBuffer **whichP;	/* packet to be enlarged */
    char *dPtr;		/* data to be piggybacked */
    long dSize;		/* length of data at dPtr */
    long maxSize;	/* how large whichP can grow to */
    /* If specified data can be piggy backed within a packet no larger than maxSize,
	adds the data and sets SEFlags and SEDataOffset.
	Enlarges packet if needed.
	Returns 0 if data has been piggybacked, -1 if maxSize would be exceeded
    */
    {
    say(9, SFTP_DebugLevel, ("sftp_AddPiggy: %ld\n", dSize));
    
    if (MakeBigEnough(whichP, dSize, maxSize) < 0) return (-1);

    /* We allow multiple piggybacked items, so only initialize SEDataOffset once. */
    if (((*whichP)->Header.SEFlags & SFTP_PIGGY) == 0)
	{
	(*whichP)->Header.SEDataOffset = (RPC2_Unsigned)((*whichP)->Header.BodyLength);
	(*whichP)->Header.SEFlags |= SFTP_PIGGY;
	}

    bcopy(dPtr, (*whichP)->Body + (*whichP)->Header.BodyLength, dSize);
    (*whichP)->Header.BodyLength += dSize;
    (*whichP)->Prefix.LengthOfPacket = sizeof(struct RPC2_PacketHeader) +
			    (*whichP)->Header.BodyLength;
    return(0);
    }


PRIVATE long MakeBigEnough(whichP, extraBytes, maxSize)
    RPC2_PacketBuffer **whichP;
    long extraBytes;
    long maxSize;
    /* Checks if whichP is a packet buffer to which extraBytes can be appended.  
    	If so returns whichP unmodified.
	Otherwise, allocates new packet, copies old contents to it, and sets whichP to new packet.
	If reallocation would cause packet size to exceed maxSize, no reallocation is done.
	Returns 0 if extraBytes can be appended to whichP; -1 otherwise.
    */
    {
    long freebytes, curlen;
    RPC2_PacketBuffer *pb;

    curlen = (*whichP)->Header.BodyLength + sizeof(struct RPC2_PacketHeader);
    freebytes = (*whichP)->Prefix.BufferSize - sizeof(struct RPC2_PacketBufferPrefix) - curlen;
    if (freebytes >= extraBytes) return(0);
    if (curlen + extraBytes > maxSize) return (-1);

    /* Realloc and copy */
    RPC2_AllocBuffer(extraBytes + (*whichP)->Header.BodyLength, &pb);
    pb->Header.BodyLength = (*whichP)->Header.BodyLength;
    bcopy(&(*whichP)->Header, &pb->Header, curlen);
    *whichP = pb;	/* DON'T free old packet !!! */
    return(0);
    }


sftp_AppendFileToPacket(sEntry, whichP)
    register struct SFTP_Entry *sEntry;
    register RPC2_PacketBuffer **whichP;
    /* Tries to add a file to the end of whichP
    Returns:
 	+X  if X bytes have been piggybacked.
	-1 in case of system call failure
	-2 if appending file would make packet larger than SFTP_MAXPACKETSIZE
    */
    {
    long rc, maxbytes, filelen;
    static char GlobalJunk[SFTP_MAXBODYSIZE];	/* buffer for read();
				    avoids huge local on my stack */
    
    filelen = sftp_vfsize(sEntry->SDesc, sEntry->openfd);
    if (filelen < 0) return(-1);

    /* now check if there is space in the packet */
    maxbytes = SFTP_MAXBODYSIZE - (*whichP)->Header.BodyLength;
    if (sEntry->PacketSize < SFTP_MAXPACKETSIZE) 
	    maxbytes -= (SFTP_MAXPACKETSIZE - sEntry->PacketSize);

    if (filelen > maxbytes) return(-2);

    /* enough space: append the file! */
    rc = sftp_vfreadfile(sEntry->SDesc, sEntry->openfd, GlobalJunk, filelen);
    if (rc < 0) return(-1);
    assert(!sftp_AddPiggy(whichP, GlobalJunk, filelen, SFTP_MAXPACKETSIZE));
    sEntry->HitEOF = TRUE;

    /* cleanup and quit */
    CLOSE(sEntry);
    return(filelen);
    }


sftp_ExtractFileFromPacket(sEntry, whichP)
    register struct SFTP_Entry *sEntry;
    register RPC2_PacketBuffer *whichP;
    /* Plucks off piggybacked file.
       Returns number of bytes plucked off, or < 0 if error */
    {
    long len, rc;

    len = whichP->Header.BodyLength - whichP->Header.SEDataOffset;
    rc = sftp_vfwritefile(sEntry->SDesc, sEntry->openfd, 
	    &whichP->Body[whichP->Header.BodyLength - len], len); 
    CLOSE(sEntry);
    if (rc < 0) return (rc);

    /* shorten the packet */
    whichP->Header.BodyLength -= len;
    return(len);
    }


sftp_AppendParmsToPacket(sEntry, whichP)
    register struct SFTP_Entry *sEntry;
    register RPC2_PacketBuffer **whichP;
    /* Clients append parms to RPC request packets,
       servers to SE control packets. 
       Returns 0 on success, -1 on failure */
    {
    struct SFTP_Parms sp;

    sp.Portal = sftp_Portal;	/* structure assignment */
    sp.Portal.Tag = (PortalTag)htonl(sp.Portal.Tag);
    sp.WindowSize = htonl(sEntry->WindowSize);
    sp.SendAhead = htonl(sEntry->SendAhead);
    sp.AckPoint = htonl(sEntry->AckPoint);
    sp.PacketSize = htonl(sEntry->PacketSize);
    sp.DupThreshold = htonl(sEntry->DupThreshold);

    assert(sftp_AddPiggy(whichP, (char *)&sp, sizeof(sp), RPC2_MAXPACKETSIZE) == 0);

    switch (sEntry->WhoAmI)
	{
	case SFCLIENT:  sEntry->SentParms = TRUE;
			break;
	
	case SFSERVER:	break;
	
	default:	return(-1);
	}

    return(0);
    }


sftp_ExtractParmsFromPacket(sEntry, whichP)
    register struct SFTP_Entry *sEntry;
    register RPC2_PacketBuffer *whichP;
    /* Plucks off piggybacked parms. Returns 0 on success, -1 on failure */
    {
    struct SFTP_Parms sp;
    struct HEntry *he;

    if (whichP->Header.BodyLength - whichP->Header.SEDataOffset < sizeof(struct SFTP_Parms))
	return(-1);

    /* We copy out the data physically:
       else structure alignment problem on IBM-RTs */
    bcopy(&whichP->Body[whichP->Header.BodyLength - sizeof(struct SFTP_Parms)], &sp, sizeof(struct SFTP_Parms));

    if (sEntry->WhoAmI == SFSERVER)
	{
	sEntry->PeerPortal = sp.Portal;	/* structure assignment */
	sEntry->PeerPortal.Tag = (PortalTag)ntohl(sp.Portal.Tag);

	/* Set up host/portal linkage. */
	sEntry->HostInfo = rpc2_GetHost(&sEntry->PInfo.RemoteHost, &sEntry->PeerPortal);
	if (sEntry->HostInfo == NULL) 
	    sEntry->HostInfo = rpc2_AllocHost(&sEntry->PInfo.RemoteHost, 
					      &sEntry->PeerPortal, SMARTFTP_HE);
	}
    else
	assert(sEntry->WhoAmI == SFCLIENT);

    sp.WindowSize = ntohl(sp.WindowSize);
    sp.SendAhead = ntohl(sp.SendAhead);
    sp.AckPoint = ntohl(sp.AckPoint);
    sp.PacketSize = ntohl(sp.PacketSize);
    sp.DupThreshold = ntohl(sp.DupThreshold);

    if (sEntry->WhoAmI == SFSERVER)
	{
	/* Find smaller of two side's parms: we will send these values on the first Start */
	if (sEntry->WindowSize > sp.WindowSize) sEntry->WindowSize = sp.WindowSize;
	if (sEntry->SendAhead > sp.SendAhead) sEntry->SendAhead = sp.SendAhead;
	if (sEntry->AckPoint > sp.AckPoint) sEntry->AckPoint = sp.AckPoint;
	if (sEntry->PacketSize > sp.PacketSize) sEntry->PacketSize = sp.PacketSize;
	if (sEntry->DupThreshold > sp.DupThreshold) sEntry->DupThreshold = sp.DupThreshold;
	}
    else
	{
	/* Accept Server's parms without question. */
	sEntry->WindowSize = sp.WindowSize;
	sEntry->SendAhead = sp.SendAhead;
	sEntry->AckPoint = sp.AckPoint;
	sEntry->PacketSize = sp.PacketSize;
	sEntry->DupThreshold = sp.DupThreshold;
	}
    sEntry->GotParms = TRUE;
    say(9, SFTP_DebugLevel, ("GotParms: %ld %ld %ld %ld %ld\n", sEntry->WindowSize, sEntry->SendAhead, sEntry->AckPoint, sEntry->PacketSize, sEntry->DupThreshold));

    whichP->Header.BodyLength -= sizeof(struct SFTP_Parms);

    return(0);
    }

/*---------------------- Miscellaneous routines ----------------------*/
struct SFTP_Entry *sftp_AllocSEntry()
    {
    register struct SFTP_Entry *sfp;

    assert((sfp = (struct SFTP_Entry *)malloc(sizeof(struct SFTP_Entry))) != NULL);
    bzero(sfp, sizeof(struct SFTP_Entry));	/* all fields initialized to 0 */
    sfp->Magic = SFTPMAGIC;
    sfp->openfd = -1;
    sfp->PacketSize = SFTP_PacketSize;
    sfp->WindowSize = SFTP_WindowSize;
    sfp->RetryCount = SFTP_RetryCount;
    sfp->SendAhead = SFTP_SendAhead;
    sfp->AckPoint = SFTP_AckPoint;
    sfp->DupThreshold = SFTP_DupThreshold;
    sfp->RInterval.tv_sec = SFTP_RetryInterval/1000;
    sfp->RInterval.tv_usec = (SFTP_RetryInterval*1000) % 1000000;
    sfp->Retransmitting = FALSE;
    return(sfp);
    }

void sftp_FreeSEntry(se)
    struct SFTP_Entry *se;
    {
    register int i;

    CLOSE(se);
    if (se->PiggySDesc) sftp_FreePiggySDesc(se);
    for (i = 0; i < MAXOPACKETS; i++)
	if (se->ThesePackets[i] != NULL) SFTP_FreeBuffer(&se->ThesePackets[i]);
    free(se);
    }

void sftp_AllocPiggySDesc(se, len, direction)
    struct SFTP_Entry *se;
    long len;
    enum WhichWay direction;
    
    {
    struct FileInfoByAddr *p;

    assert(se->PiggySDesc == NULL);	/* can't already exist */
    se->PiggySDesc = (SE_Descriptor *)malloc(sizeof(SE_Descriptor));
    assert(se->PiggySDesc); /* malloc failure is fatal */

    se->PiggySDesc->Value.SmartFTPD.Tag = FILEINVM;
    se->PiggySDesc->Value.SmartFTPD.TransmissionDirection = direction;
    /* maintain quotas -- no random values! */
    if (SFTP_EnforceQuota && se->SDesc) 
	se->PiggySDesc->Value.SmartFTPD.ByteQuota = 
	    se->SDesc->Value.SmartFTPD.ByteQuota;

    p = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;
    /* 0 length malloc()s choke; fake a 1-byte file */
    if (len == 0)  p->vmfile.SeqBody = (RPC2_ByteSeq)malloc(1);
    else p->vmfile.SeqBody = (RPC2_ByteSeq)malloc(len);
    assert(p->vmfile.SeqBody);  /* malloc failure is fatal */
    p->vmfile.MaxSeqLen = len;
    p->vmfile.SeqLen = len;
    p->vmfilep = 0;
    }


void sftp_FreePiggySDesc(se)
    struct SFTP_Entry *se;
    {
    struct FileInfoByAddr *p;

    assert(se->PiggySDesc);  /* better not be NULL! */
    p = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;
    assert(p->vmfile.SeqBody); /* better not be NULL! */
    free(p->vmfile.SeqBody);
    free(se->PiggySDesc);
    se->PiggySDesc = NULL;
    }

void sftp_SetError(s, e)
    register struct SFTP_Entry *s;
    enum SFState e;
    /* separate routine for easy debugging*/
    {
    s->WhoAmI = e;
    }



/*-------------------------- Debugging routines ------------------------*/

long SFTP_PrintSED(IN SDesc, IN outFile)
    register SE_Descriptor *SDesc;
    register FILE *outFile;
    {
    register struct SFTP_Descriptor *sftpd;
    sftpd = &SDesc->Value.SmartFTPD;
    
    assert(SDesc->Tag == SMARTFTP);	/* I shouldn't be called otherwise */

    switch(SDesc->LocalStatus)
	{
	case SE_NOTSTARTED:
	    fprintf(outFile, "LocalStatus:    SE_NOTSTARTED    ");break;
	case SE_INPROGRESS:
	    fprintf(outFile, "LocalStatus:    SE_INPROGRESS    ");break;
	case SE_SUCCESS:
	    fprintf(outFile, "LocalStatus:    SE_SUCCESS    ");break;
	case SE_FAILURE:
	    fprintf(outFile, "LocalStatus:    SE_FAILURE    ");break;
	}

    switch(SDesc->RemoteStatus)
	{
	case SE_NOTSTARTED:
	    fprintf(outFile, "RemoteStatus:    SE_NOTSTARTED    ");break;
	case SE_INPROGRESS:
	    fprintf(outFile, "RemoteStatus:    SE_INPROGRESS    ");break;
	case SE_SUCCESS:
	    fprintf(outFile, "RemoteStatus:    SE_SUCCESS    ");break;
	case SE_FAILURE:
	    fprintf(outFile, "RemoteStatus:    SE_FAILURE    ");break;
	}
	
    fprintf(outFile, "Tag:    SMARTFTP\n");
    fprintf(outFile, "TransmissionDirection:    %s    hashmark:    '%c'   SeekOffset:    %ld    BytesTransferred:    %ld    ByteQuota:    %ld    QuotaExceeded:    %ld\n", (sftpd->TransmissionDirection == CLIENTTOSERVER) ? "CLIENTTOSERVER" : (sftpd->TransmissionDirection == SERVERTOCLIENT) ? "SERVERTOCLIENT" : "??????", sftpd->hashmark, sftpd->SeekOffset, sftpd->BytesTransferred, sftpd->ByteQuota, sftpd->QuotaExceeded);

    switch(sftpd->Tag)
	{
	case FILEBYNAME:
	    fprintf(outFile, "Tag:    FILEBYNAME    ProtectionBits:    0%lo    LocalFileName:    \"%s\"\n", sftpd->FileInfo.ByName.ProtectionBits, sftpd->FileInfo.ByName.LocalFileName);
	    break;
	
	case FILEBYINODE:
	    fprintf(outFile, "Tag:    FILEBYINODE   Device:    %ld    Inode:    %ld\n", sftpd->FileInfo.ByInode.Device, sftpd->FileInfo.ByInode.Inode);
	    break;

	case FILEBYFD:
	    fprintf(outFile, "Tag:    FILEBYFD   fd:    %ld\n", sftpd->FileInfo.ByFD.fd);
	    break;
			
	case FILEINVM:
	    fprintf(outFile, "Tag:    FILEINVM   SeqBody:  0x%lx    MaxSeqLen:    %ld    SeqLen: %ld\n", sftpd->FileInfo.ByAddr.vmfile.SeqBody, sftpd->FileInfo.ByAddr.vmfile.MaxSeqLen, sftpd->FileInfo.ByAddr.vmfile.SeqLen);
	    break;
			
	default:
	    fprintf(outFile, "Tag: ???????\n");
	    break;	
	}
    }


