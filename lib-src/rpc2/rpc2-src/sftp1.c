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
    SFTP: a smart file transfer protocol using windowing and piggybacking
    sftp1.c contains the SFTP interface to RPC2 

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

/*----------------------- Local procedure specs  ----------------------*/
static long GetFile();
static long PutFile();
static RPC2_PacketBuffer *AwaitPacket();
static void AddTimerEntry();
static long MakeBigEnough();

/*---------------------------  Local macros ---------------------------*/
#define FAIL(se, rCode)\
	    {\
	    sftp_vfclose(se);\
	    se->SDesc = NULL;\
	    return(rCode);\
	    }

#define QUIT(se, RC1, RC2)\
    se->SDesc->LocalStatus = RC1;\
    sftp_vfclose(se);\
    se->SDesc = NULL;\
    return(RC2);


#define BOGUS(pb)\
    (sftp_TraceBogus(1, __LINE__), sftp_bogus++, SFTP_FreeBuffer(&pb))



/*------------- Procedures directly invoked by RPC2 ---------------*/

long SFTP_Init()
{
    char *sname;
    
    say(0, SFTP_DebugLevel, "SFTP_Init()\n");

    /* initialize the sftp timer chain */
    TM_Init(&sftp_Chain);

    if (sftp_Port.Tag)
    {
        /* Create socket for SFTP packets */
        if (rpc2_CreateIPSocket(&sftp_Socket, &sftp_Port) != RPC2_SUCCESS)
            return(RPC2_FAIL);
    }

    /* Create SFTP listener process */
    sname = "sftp_Listener";
    LWP_CreateProcess((PFIC)sftp_Listener, 16384, LWP_NORMAL_PRIORITY,
		      sname, sname, &sftp_ListenerPID);

    sftp_InitTrace();

    /* Register SFTP packet handler with RPC2 socket listener */
    SL_RegisterHandler(SFTPVERSION, sftp_ExaminePacket);

    return (RPC2_SUCCESS);
}

void SFTP_SetDefaults(initPtr)
    SFTP_Initializer *initPtr;
    /* Should be called before SFTP_Activate() */
    {
    initPtr->PacketSize = SFTP_DEFPACKETSIZE;
    initPtr->WindowSize = SFTP_DEFWINDOWSIZE;
    initPtr->RetryCount = 10;
    initPtr->RetryInterval = 2000;	/* milliseconds */
    initPtr->SendAhead = SFTP_DEFSENDAHEAD;   /* make sure it is less than 16, for readv() */
    initPtr->AckPoint = SFTP_DEFSENDAHEAD;    /* same as SendAhead */
    initPtr->EnforceQuota = 0;
    initPtr->DoPiggy = TRUE;
    initPtr->DupThreshold = 16;
    initPtr->MaxPackets = -1;
    initPtr->Port.Tag = RPC2_PORTBYINETNUMBER;
    initPtr->Port.Value.InetPortNumber = htons(0);
    }


void SFTP_Activate(initPtr)
    SFTP_Initializer *initPtr;
    /* Should be called before RPC2_Init() */
    {
    struct SE_Definition *sed;
    long size;

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
	sftp_Port = initPtr->Port;			/* structure assignment */
	}
    assert(SFTP_SendAhead <= 16);	/* 'cause of readv() bogosity */

    /* Enlarge table by one */
    SE_DefCount++;
    size = sizeof(struct SE_Definition)*SE_DefCount;
    if (SE_DefSpecs == NULL)
	/* The realloc() on the romp dumps core if SE_DefSpecs is NULL */
	assert((SE_DefSpecs = (struct SE_Definition *)malloc(size)) != NULL);
    else
	assert((SE_DefSpecs = (struct SE_Definition *)realloc(SE_DefSpecs, size)) != NULL);

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
    struct SFTP_Entry *se;

    say(0, SFTP_DebugLevel, "SFTP_Bind()\n");

    se = sftp_AllocSEntry();	/* malloc and initialize SFTP_Entry */
    se->WhoAmI = SFCLIENT;
    se->LocalHandle = ConnHandle;
    RPC2_SetSEPointer(ConnHandle, se);
    return(RPC2_SUCCESS);
    }


long SFTP_Bind2(IN RPC2_Handle ConnHandle, IN RPC2_Unsigned BindTime)
{
    struct SFTP_Entry *se;
    int retry;

    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    RPC2_GetPeerInfo(ConnHandle, &se->PInfo);
    se->HostInfo = rpc2_GetHost(&se->PInfo.RemoteHost);
    if (BindTime)
    {
        /* XXX Do some estimate of the amount of transferred data --JH */
	RPC2_UpdateEstimates(se->HostInfo, BindTime,
			     sizeof(struct RPC2_PacketHeader),
			     sizeof(struct RPC2_PacketHeader));

	retry = 1;
        rpc2_RetryInterval(ConnHandle, sizeof(struct RPC2_PacketHeader),
			   sizeof(struct RPC2_PacketHeader), &retry,
			   se->RetryCount, &se->RInterval);
    }
    
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
    struct SFTP_Entry *se;

    say(0, SFTP_DebugLevel, "SFTP_NewConn()\n");

    se = sftp_AllocSEntry();	/* malloc and initialize */
    se->WhoAmI = SFSERVER;
    se->LocalHandle = ConnHandle;
    RPC2_GetPeerInfo(ConnHandle, &se->PInfo);
    se->HostInfo = rpc2_GetHost(&se->PInfo.RemoteHost);
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

    say(0, SFTP_DebugLevel, "SFTP_MakeRPC1 ()\n");

    SDesc->LocalStatus = SE_SUCCESS;	/* non-execution == success */
    SDesc->RemoteStatus = SE_SUCCESS;	/* non-execution == success */
    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    if (se->WhoAmI != SFCLIENT) FAIL(se, RPC2_SEFAIL2);
    se->ThisRPCCall = (*RequestPtr)->Header.SeqNumber;	/* remember new call has begun */
    se->SDesc = SDesc;
    sftp_Progress(SDesc, 0);

    se->XferState = XferNotStarted;
    se->UseMulticast = FALSE;
    se->HitEOF = FALSE;
    if (SDesc->Value.SmartFTPD.TransmissionDirection == CLIENTTOSERVER)
	{
	se->SendMostRecent = se->SendLastContig;
	se->SendWorriedLimit = se->SendLastContig;
	se->SendAckLimit = se->SendLastContig;
	memset(se->SendTheseBits, 0, BITMASKWIDTH*sizeof(long));
	se->ReadAheadCount = 0;
	rc = sftp_InitIO(se);
	}
    else
	{
	se->RecvMostRecent = se->RecvLastContig;
	memset(se->RecvTheseBits, 0, BITMASKWIDTH*sizeof(long));
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
		
		default:        sftp_Progress(SDesc, rc);
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
    int i, nbytes;

    say(0, SFTP_DebugLevel, "SFTP_MakeRPC2()\n");
    
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
	    sftp_Progress(SDesc, nbytes);
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
    sftp_vfclose(se);
    se->SDesc = NULL;
    se->SendLastContig = se->SendMostRecent;
    se->RecvLastContig = se->RecvMostRecent;
    memset(se->SendTheseBits, 0, sizeof(int)*BITMASKWIDTH);
    memset(se->RecvTheseBits, 0, sizeof(int)*BITMASKWIDTH);

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


long SFTP_GetRequest(RPC2_Handle ConnHandle, RPC2_PacketBuffer *Request)
{
    struct SFTP_Entry *se;
    long len;
    int retry;

    say(0, SFTP_DebugLevel, "SFTP_GetRequest()\n");

    assert (RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS &&  se != NULL);
    if (se->WhoAmI != SFSERVER) FAIL(se, RPC2_SEFAIL2);
    se->ThisRPCCall = Request->Header.SeqNumber;   /* acquire client's RPC call number */

    if (Request->Header.BindTime)
    {
        /* XXX Do some estimate of the amount of transferred data --JH */
	RPC2_UpdateEstimates(se->HostInfo, Request->Header.BindTime,
			     sizeof(struct RPC2_PacketHeader),
			     sizeof(struct RPC2_PacketHeader));
	retry = 1;
        rpc2_RetryInterval(ConnHandle, sizeof(struct RPC2_PacketHeader),
			   sizeof(struct RPC2_PacketHeader), &retry,
			   se->RetryCount, &se->RInterval);
    }

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


long SFTP_InitSE(RPC2_Handle ConnHandle, SE_Descriptor *SDesc)
{
    struct SFTP_Entry *se;
    int rc;

    say(0, SFTP_DebugLevel, "SFTP_InitSE ()\n");
	
    SDesc->LocalStatus = SE_NOTSTARTED;
    SDesc->RemoteStatus = SE_NOTSTARTED;
    assert (RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS &&  se != NULL);
    if (se->WhoAmI != SFSERVER) FAIL(se, RPC2_SEFAIL2);
    if (se->GotParms == FALSE) FAIL(se, RPC2_SEFAIL2);
    se->SDesc = SDesc;

    rc = sftp_InitIO(se);
    if (rc < 0) {
	    SDesc->LocalStatus = SE_FAILURE;
	    se->SDesc = NULL;
	    return(RPC2_SEFAIL1);
    }
    return(RPC2_SUCCESS);
}


long SFTP_CheckSE(RPC2_Handle ConnHandle, SE_Descriptor *SDesc, long Flags)
{
    long rc, flen;
    struct SFTP_Entry *se;
    struct SFTP_Descriptor *sftpd;
    struct FileInfoByAddr *p;
	

    say(0, SFTP_DebugLevel, "SFTP_CheckSE()\n");

    if (Flags == 0) return(RPC2_SUCCESS);
    assert (RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS &&  se != NULL);
    if (se->WhoAmI != SFSERVER) FAIL(se, RPC2_SEFAIL2);
    se->SDesc = SDesc;
    
    if (SDesc->LocalStatus != SE_NOTSTARTED || SDesc->RemoteStatus != SE_NOTSTARTED)
	return(RPC2_SUCCESS);	/* SDesc error conditions are self-describing */
    SDesc->LocalStatus = SDesc->RemoteStatus = SE_INPROGRESS;
    sftp_Progress(SDesc, 0);

    sftpd = &SDesc->Value.SmartFTPD;
    if (sftpd->hashmark != 0)
	switch(sftpd->Tag)
	    {
	    case FILEBYNAME:
		say(0, SFTP_DebugLevel, "%s: ", sftpd->FileInfo.ByName.LocalFileName);
	    	break;

	    case FILEBYINODE:
		say(0, SFTP_DebugLevel, "%ld.%ld: ", sftpd->FileInfo.ByInode.Device, sftpd->FileInfo.ByInode.Inode);
	    	break;

	    case FILEBYFD:
		say(0, SFTP_DebugLevel, "%ld: ", sftpd->FileInfo.ByFD.fd);
	    	break;

	    case FILEINVM:
		say(0, SFTP_DebugLevel, "%p[%ld, %ld]: ", sftpd->FileInfo.ByAddr.vmfile.SeqBody, sftpd->FileInfo.ByAddr.vmfile.MaxSeqLen,  sftpd->FileInfo.ByAddr.vmfile.SeqLen);
	    	break;
	    }

    switch(sftpd->TransmissionDirection)
	{
	case CLIENTTOSERVER:
	    if (se->PiggySDesc) {
		/* use squirrelled-away data */
		p = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;
		rc = sftp_vfwritefile(se, p->vmfile.SeqBody, p->vmfile.SeqLen);
		if (rc < 0) {
		    sftp_SetError(se, DISKERROR);
		    se->SDesc->LocalStatus = SE_FAILURE;
		} else {
		    rc = RPC2_SUCCESS;
		    se->SDesc->LocalStatus = SE_SUCCESS;
		    sftp_Progress(se->SDesc, p->vmfile.SeqLen);
		}
		sftp_FreePiggySDesc(se); /* get rid of saved file data */
	    } else {
	        /* full-fledged file transfer */
		rc = GetFile(se);
	    }
	    break;

	case SERVERTOCLIENT:
	    flen = sftp_piggybackfilesize(se);
	    if (SFTP_DoPiggy == FALSE
		|| ((Flags & SE_AWAITREMOTESTATUS) != 0)
		|| (flen >= SFTP_MAXBODYSIZE)) {
		/* can't defer transfer to SendResponse */
		rc = PutFile(se);
	    } else {
		/*  Squirrel away contents of file.
		We have to save the data right away, because the
		the server may delete or modify the file after
		CheckSideEffect but before  SendResponse. */

		sftp_AllocPiggySDesc(se, flen, SERVERTOCLIENT);
		p = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;	    
		rc = sftp_piggybackfileread(se, p->vmfile.SeqBody);
		if (rc < 0)
		    {
		    sftp_SetError(se, DISKERROR);
		    se->SDesc->LocalStatus = SE_FAILURE;
		    }
		else
		    {
		    rc = RPC2_SUCCESS;
		    se->SDesc->LocalStatus = SE_SUCCESS;
		    sftp_Progress(se->SDesc, p->vmfile.SeqLen);
		    }
	    }
	    break;

	default: FAIL(se, RPC2_SEFAIL1);
    }

    sftp_vfclose(se);
    se->SDesc = NULL;
    return(rc);
}


long SFTP_SendResponse(IN ConnHandle, IN Reply)
    RPC2_Handle ConnHandle;
    RPC2_PacketBuffer **Reply;
    {
    struct SFTP_Entry *se;
    long rc;

    say(0, SFTP_DebugLevel, "SFTP_SendResponse()\n");

    assert (RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS &&  se != NULL);
    
    /* SDesc is pretty much guaranteed to be an invalid pointer by now. It is
     * commonly on the stack when InitSE and CheckSE are called, but the stack
     * has been unwound by the time we hit SendResponse. It should be ok,
     * cleanup has already happened in CheckSE */
    se->SDesc = NULL;

    rc = RPC2_SUCCESS;
    if (se->PiggySDesc) {
	/* Deal with file saved for piggybacking */

	if (se->PiggySDesc->Value.SmartFTPD.TransmissionDirection == SERVERTOCLIENT)
	{ /* File is indeed for the client;  the only other way PiggySDesc can
	     be non-null is when a file is stored piggybacked, but the server
	     does not do an InitSideEffect or CheckSideEffect */

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
	/* clean up state */
	sftp_vfclose(se);
	sftp_FreePiggySDesc(se);   /* always get rid of the file */
    }


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


long SFTP_GetTime(IN RPC2_Handle ConnHandle, INOUT struct timeval *Time) 
{
    struct SFTP_Entry *se;
    long rc;

    say(0, SFTP_DebugLevel, "SFTP_GetTime()\n");

    se = NULL;
    /* 
     * We might legitimately fail here because there is no connection, 
     * there is a connection but it is not completely set up.
     */
    if ((rc = RPC2_GetSEPointer(ConnHandle, &se)) != RPC2_SUCCESS)
	    return(rc);

    if (se == NULL || se->HostInfo == NULL) return(RPC2_NOCONNECTION);

    *Time = se->LastWord;
    return(RPC2_SUCCESS);
}


long SFTP_GetHostInfo(IN ConnHandle, INOUT HPtr) 
    RPC2_Handle ConnHandle;
    struct HEntry **HPtr;
    {
    struct SFTP_Entry *se;
    long rc;

    say(0, SFTP_DebugLevel, "SFTP_GetHostInfo()\n");

    se = NULL;
    if ((rc = RPC2_GetSEPointer(ConnHandle, &se)) != RPC2_SUCCESS)
	    return(rc);

    if (se == NULL) return(RPC2_NOCONNECTION);

    /* 
     * There may still be no host info.  If not, see if some of
     * the right type has appeared since the bind.
     */
    if (se->HostInfo == NULL) 
	se->HostInfo = rpc2_GetHost(&se->PInfo.RemoteHost);
	
    *HPtr = se->HostInfo;
    return(RPC2_SUCCESS);
    }



/*-------------------- Data transmission routines -----------------------*/
static long GetFile(struct SFTP_Entry *sEntry)
    /* Local file is already opened */
{
    struct CEntry *ce;
    long packetsize;
    long startmode = TRUE;
    RPC2_PacketBuffer *pb;
    int i;
    
    sEntry->XferState = XferInProgress;
    sftp_Progress(sEntry->SDesc, 0);
    sEntry->SDesc->Value.SmartFTPD.QuotaExceeded = 0;
    sEntry->HitEOF = FALSE;
    sEntry->RecvMostRecent = sEntry->RecvLastContig;
    sEntry->RecvFirst = sEntry->RecvLastContig + 1;
    memset(sEntry->RecvTheseBits, 0, sizeof(int)*BITMASKWIDTH);

    ce = rpc2_GetConn(sEntry->LocalHandle);
    sEntry->TimeEcho = ce ? ce->TimeStampEcho : 0;
    sEntry->RequestTime = ce ? ce->RequestTime : 0;

    if (sftp_SendStart(sEntry) < 0)
    	{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}	/* Initiate */

    /* Set timeout to be large enough for size of (possibly sent) ack + size
     * of the next data packet */
    packetsize = sEntry->PacketSize + sizeof(struct RPC2_PacketHeader);
    while (sEntry->XferState == XferInProgress) {
	for (i = 1; i <= sEntry->RetryCount; i++) {
	    /* get a new retry interval estimate */
	    rpc2_RetryInterval(sEntry->LocalHandle, packetsize,
			       sizeof(struct RPC2_PacketHeader), &i,
			       sEntry->RetryCount, &sEntry->RInterval);

	    pb = (RPC2_PacketBuffer *)AwaitPacket(&sEntry->RInterval, sEntry);

	    /* Make sure nothing bad happened while we were waiting */
	    if (sEntry->WhoAmI == ERROR) {
		if (pb) { SFTP_FreeBuffer(&pb); }
		QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
	    }
	    if (sEntry->WhoAmI == DISKERROR) {
		if (pb) { SFTP_FreeBuffer(&pb); }
		QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL3);
	    }

	    /* Did we receive a packet? */
	    if (pb == NULL) {
		sftp_timeouts++;
		sEntry->Retransmitting = TRUE;
		say(4, SFTP_DebugLevel, "GetFile: Backoff\n");

		if (startmode)
		{
		    if (sftp_SendStart(sEntry) < 0) {
			QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
		    }
		}
		else
		{
		    if (sftp_SendTrigger(sEntry) < 0) {
			QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
		    }
		}
		continue;
	    }

	    /* If so process it */
	    switch((int) pb->Header.Opcode) {
		/* punt CtrlSeqNumber consistency for now */
	    case SFTP_NAK:
		assert(FALSE); /* my SEntry state should already be ERRORR */
	    case SFTP_DATA:
		goto GotData;

	    case SFTP_BUSY:
		sftp_Recvd.Busies++;
		if (startmode) {
		    sftp_busy++;
		    i = 0;
		    SFTP_FreeBuffer(&pb);
		}
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

	if (sftp_DataArrived(pb, sEntry) < 0) {
	    if (sEntry->WhoAmI == DISKERROR) {
		QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL3);
	    }
	    else {
		SFTP_FreeBuffer(&pb);
		QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
	    }
	}
    }

    QUIT(sEntry, SE_SUCCESS, RPC2_SUCCESS);
}


/* Local file is already opened */
static long PutFile(struct SFTP_Entry *sEntry)
{
    RPC2_PacketBuffer *pb;
    struct CEntry     *ce;
    int i, rc = 0;
    unsigned long bytes;
    
    sftp_Progress(sEntry->SDesc, 0);
    sEntry->SDesc->Value.SmartFTPD.QuotaExceeded = 0;
    sEntry->HitEOF = FALSE;
    sEntry->XferState = XferInProgress;
    sEntry->SendMostRecent = sEntry->SendLastContig;	/* Really redundant */
    sEntry->SendWorriedLimit = sEntry->SendLastContig;  /* Really redundant */
    sEntry->SendAckLimit = sEntry->SendLastContig;      /* Really redundant */

    /* Kip: instead of sEntry->TimeEcho = 0, we will go ahead and
     * use the Timestamp from the first rpc2 packet since the putfile
     * may not span more than one round-trip, and no RTT update would
     * occur. */
    ce = rpc2_GetConn(sEntry->LocalHandle);
    sEntry->TimeEcho = ce ? ce->TimeStampEcho : 0;
    sEntry->RequestTime = ce ? ce->RequestTime : 0;

    memset(sEntry->SendTheseBits, 0, sizeof(int)*BITMASKWIDTH);

    if (sftp_SendStrategy(sEntry) < 0)
	{QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);}

    /* estimated size of an sftp data transfer */
    bytes = ((sEntry->PacketSize + sizeof(struct RPC2_PacketHeader)) *
	     sEntry->AckPoint);

    while (sEntry->XferState == XferInProgress) {
	for (i = 1; i <= sEntry->RetryCount; i++) {
	    /* get a new retry interval estimate */
	    rpc2_RetryInterval(sEntry->LocalHandle, sizeof(struct RPC2_PacketHeader),
			       bytes, &i, sEntry->RetryCount, &sEntry->RInterval);

	    pb = (RPC2_PacketBuffer *)AwaitPacket(&sEntry->RInterval, sEntry);

	    /* Make sure nothing bad happened while we were waiting */
	    if (sEntry->WhoAmI == ERROR) {
		if (pb) { SFTP_FreeBuffer(&pb); }
		QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
	    }
	    if (sEntry->WhoAmI == DISKERROR) {
		if (pb) { SFTP_FreeBuffer(&pb); }
		QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL3);
	    }

	    /* Things still look ok */
	    if (pb != NULL)
		goto GotAck;

	    say(4, SFTP_DebugLevel, "PutFile: backing off\n");
	    sftp_timeouts++;
	    sEntry->Retransmitting = TRUE;

	    if (sftp_SendStrategy(sEntry) < 0) {
		QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
	    }
	}
	sftp_SetError(sEntry, ERROR);
	QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
GotAck:
	if (i == 1) /* got an ack on the first try. allow RTT updates now */
	    sEntry->Retransmitting = FALSE;

	switch ((int) pb->Header.Opcode) {
	case SFTP_NAK:
	    assert(FALSE);  /* should have already set sEntry state */
	    
	case SFTP_ACK:
	    rc = sftp_AckArrived(pb, sEntry);
	    break;
	    
	default:
	    break;	    
	}

	SFTP_FreeBuffer(&pb);
	if (rc < 0) { /* sftp_AckArrived failed */
	    QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
	}
    }

    QUIT(sEntry, SE_SUCCESS, RPC2_SUCCESS);
}


static RPC2_PacketBuffer *AwaitPacket(tOut, sEntry)
    struct timeval *tOut;
    struct SFTP_Entry *sEntry;
    
    /* Awaits for a packet on sEntry or  timeout tOut
	Returns pointer to arrived packet or NULL
    */
    {
    struct SLSlot *sls;

    if (LWP_GetRock(SMARTFTP, (char **)&sls) != LWP_SUCCESS)
	{
	sls = (struct SLSlot *) malloc(sizeof(struct SLSlot));
	memset(sls, 0, sizeof(struct SLSlot));
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
    assert(0);
    return NULL;
    }
    

static void AddTimerEntry(whichElem)
    struct TM_Elem *whichElem;
    {
    struct TM_Elem *t;
    long mytime;

    mytime = whichElem->TotalTime.tv_sec*1000000 + whichElem->TotalTime.tv_usec;
    t = TM_GetEarliest(sftp_Chain);
    if (t == NULL || (t->TimeLeft.tv_sec*1000000 + t->TimeLeft.tv_usec) > mytime)
	IOMGR_Cancel(sftp_ListenerPID);
    TM_Insert(sftp_Chain, whichElem);
    }


/*---------------------- Piggybacking routines -------------------------*/

int sftp_AddPiggy(RPC2_PacketBuffer **whichP, char *dPtr, long dSize,
		  long maxSize)
/* whichP	- packet to be enlarged
 * dPtr		- data to be piggybacked
 * dSize	- length of data at dPtr
 * maxSize	- how large whichP can grow to */
    /* If specified data can be piggy backed within a packet no larger than maxSize,
	adds the data and sets SEFlags and SEDataOffset.
	Enlarges packet if needed.
	Returns 0 if data has been piggybacked, -1 if maxSize would be exceeded
    */
{
    say(9, SFTP_DebugLevel, "sftp_AddPiggy: %ld\n", dSize);
    
    if (MakeBigEnough(whichP, dSize, maxSize) < 0) return (-1);

    /* We allow multiple piggybacked items, so only initialize SEDataOffset once. */
    if (((*whichP)->Header.SEFlags & SFTP_PIGGY) == 0)
	{
	(*whichP)->Header.SEDataOffset = (RPC2_Unsigned)((*whichP)->Header.BodyLength);
	(*whichP)->Header.SEFlags |= SFTP_PIGGY;
	}

    memcpy((*whichP)->Body + (*whichP)->Header.BodyLength, dPtr, dSize);
    (*whichP)->Header.BodyLength += dSize;
    (*whichP)->Prefix.LengthOfPacket = sizeof(struct RPC2_PacketHeader) +
			    (*whichP)->Header.BodyLength;
    return(0);
}


static long MakeBigEnough(RPC2_PacketBuffer **whichP, long extraBytes,
			  long maxSize)
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
    memcpy(&pb->Header, &(*whichP)->Header, curlen);
    *whichP = pb;	/* DON'T free old packet !!! */
    return(0);
}


long sftp_AppendFileToPacket(struct SFTP_Entry *sEntry,
			     RPC2_PacketBuffer **whichP)
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
    
    filelen = sftp_piggybackfilesize(sEntry);
    if (filelen < 0) return(-1);

    /* now check if there is space in the packet */
    maxbytes = SFTP_MAXBODYSIZE - (*whichP)->Header.BodyLength;

#if 1
    /* Only piggy as much as a default sftp packet */
    if (sEntry->PacketSize < SFTP_MAXPACKETSIZE) 
	    maxbytes -= (SFTP_MAXPACKETSIZE - sEntry->PacketSize);
#endif

    if (filelen > maxbytes) return(-2);

    /* enough space: append the file! */
    rc = sftp_piggybackfileread(sEntry, GlobalJunk);
    if (rc < 0) return(-1);
    assert(!sftp_AddPiggy(whichP, GlobalJunk, filelen, SFTP_MAXPACKETSIZE));
    sEntry->HitEOF = TRUE;

    /* cleanup and quit */
    sftp_vfclose(sEntry);
    return(filelen);
}


long sftp_ExtractFileFromPacket(struct SFTP_Entry *sEntry,
				RPC2_PacketBuffer *whichP)
    /* Plucks off piggybacked file.
       Returns number of bytes plucked off, or < 0 if error */
{
    long len, rc;

    len = whichP->Header.BodyLength - whichP->Header.SEDataOffset;
    rc = sftp_vfwritefile(sEntry, &whichP->Body[whichP->Header.BodyLength-len],
			  len); 
    sftp_vfclose(sEntry);
    if (rc < 0) return (rc);

    /* shorten the packet */
    whichP->Header.BodyLength -= len;
    return(len);
}


int sftp_AppendParmsToPacket(struct SFTP_Entry *sEntry,
			     RPC2_PacketBuffer **whichP)
    /* Clients append parms to RPC request packets,
       servers to SE control packets. 
       Returns 0 on success, -1 on failure */
{
    struct SFTP_Parms sp;

    sp.Port = sftp_Port;	/* structure assignment */
    sp.Port.Tag = (PortTag)htonl(sp.Port.Tag);
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


int sftp_ExtractParmsFromPacket(struct SFTP_Entry *sEntry,
				RPC2_PacketBuffer *whichP)
    /* Plucks off piggybacked parms. Returns 0 on success, -1 on failure */
{
    struct SFTP_Parms sp;

    if (whichP->Header.BodyLength - whichP->Header.SEDataOffset < sizeof(struct SFTP_Parms))
	return(-1);

    /* We copy out the data physically:
       else structure alignment problem on IBM-RTs */
    memcpy(&sp, &whichP->Body[whichP->Header.BodyLength - sizeof(struct SFTP_Parms)], sizeof(struct SFTP_Parms));

    sEntry->PeerPort = sp.Port;
    sEntry->PeerPort.Tag = (PortTag)ntohl(sp.Port.Tag);

    if (sEntry->WhoAmI == SFSERVER)
    {
	/* Set up host/port linkage. */
	sEntry->HostInfo = rpc2_GetHost(&sEntry->PInfo.RemoteHost);
	if (sEntry->HostInfo == NULL) 
	    sEntry->HostInfo = rpc2_AllocHost(&sEntry->PInfo.RemoteHost);
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
    say(9, SFTP_DebugLevel, "GotParms: %ld %ld %ld %ld %ld\n", sEntry->WindowSize, sEntry->SendAhead, sEntry->AckPoint, sEntry->PacketSize, sEntry->DupThreshold);

    whichP->Header.BodyLength -= sizeof(struct SFTP_Parms);

    return(0);
}

/*---------------------- Miscellaneous routines ----------------------*/
struct SFTP_Entry *sftp_AllocSEntry(void)
    {
    struct SFTP_Entry *sfp;

    assert((sfp = (struct SFTP_Entry *)malloc(sizeof(struct SFTP_Entry))) != NULL);
    memset(sfp, 0, sizeof(struct SFTP_Entry));	/* all fields initialized to 0 */
    sfp->Magic = SFTPMAGIC;
    sfp->openfd = -1;
    sfp->fd_offset = 0;
    sfp->PacketSize = SFTP_PacketSize;
    sfp->WindowSize = SFTP_WindowSize;
    sfp->RetryCount = SFTP_RetryCount;
    sfp->SendAhead = SFTP_SendAhead;
    sfp->AckPoint = SFTP_AckPoint;
    sfp->DupThreshold = SFTP_DupThreshold;
    sfp->RInterval.tv_sec = SFTP_RetryInterval/1000;
    sfp->RInterval.tv_usec = (SFTP_RetryInterval*1000) % 1000000;
    sfp->Retransmitting = FALSE;
    sfp->RequestTime = 0;
    CLRTIME(&sfp->LastWord);
    return(sfp);
    }

void sftp_FreeSEntry(struct SFTP_Entry *se)
{
    int i;

    sftp_vfclose(se);
    if (se->PiggySDesc) sftp_FreePiggySDesc(se);
    for (i = 0; i < MAXOPACKETS; i++)
	if (se->ThesePackets[i] != NULL) SFTP_FreeBuffer(&se->ThesePackets[i]);
    free(se);
}

void sftp_AllocPiggySDesc(struct SFTP_Entry *se, long len,
			  enum WhichWay direction)
{
    struct FileInfoByAddr *p;

    assert(se->PiggySDesc == NULL);	/* can't already exist */
    se->PiggySDesc = (SE_Descriptor *)malloc(sizeof(SE_Descriptor));
    assert(se->PiggySDesc); /* malloc failure is fatal */

    memset(se->PiggySDesc, 0, sizeof(SE_Descriptor));
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


void sftp_FreePiggySDesc(struct SFTP_Entry *se)
{
    struct FileInfoByAddr *p;

    assert(se->PiggySDesc);  /* better not be NULL! */
    p = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;
    assert(p->vmfile.SeqBody); /* better not be NULL! */
    free(p->vmfile.SeqBody);
    free(se->PiggySDesc);
    se->PiggySDesc = NULL;
}

void sftp_SetError(struct SFTP_Entry *s, enum SFState e)
    /* separate routine for easy debugging*/
{
    s->WhoAmI = e;
}



/*-------------------------- Debugging routines ------------------------*/

long SFTP_PrintSED(IN SDesc, IN outFile)
    SE_Descriptor *SDesc;
    FILE *outFile;
    {
    struct SFTP_Descriptor *sftpd;
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
	    fprintf(outFile, "Tag:    FILEINVM   SeqBody:  %p    MaxSeqLen:    %ld    SeqLen: %ld\n", sftpd->FileInfo.ByAddr.vmfile.SeqBody, sftpd->FileInfo.ByAddr.vmfile.MaxSeqLen, sftpd->FileInfo.ByAddr.vmfile.SeqLen);
	    break;
			
	default:
	    fprintf(outFile, "Tag: ???????\n");
	    break;	
	}
    return 1;
    }


