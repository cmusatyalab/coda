/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2025 Carnegie Mellon University
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
#include <limits.h>
#include <unistd.h>

#include <rpc2/se.h>
#include <rpc2/sftp.h>

#include "rpc2.private.h"

/*----------------------- Local procedure specs  ----------------------*/
static long GetFile();
static long PutFile();
static RPC2_PacketBuffer *sftp_DequeuePacket(struct SFTP_Entry *sEntry);
static RPC2_PacketBuffer *AwaitPacket(struct SFTP_Entry *sEntry, int retry,
                                      int outbytes, int inbytes);
static long MakeBigEnough();

/*---------------------------  Local macros ---------------------------*/
#define FAIL(se, rCode)   \
    {                     \
        sftp_vfclose(se); \
        se->SDesc = NULL; \
        return (rCode);   \
    }

#define QUIT(se, RC1, RC2)                                  \
    do {                                                    \
        while (se->RecvQueue) {                             \
            RPC2_PacketBuffer *pb = sftp_DequeuePacket(se); \
            SFTP_FreeBuffer(&pb);                           \
        }                                                   \
        se->SDesc->LocalStatus = RC1;                       \
        sftp_vfclose(se);                                   \
        se->SDesc = NULL;                                   \
        return (RC2);                                       \
    } while (0)

#define BOGUS(pb) \
    (sftp_TraceBogus(1, __LINE__), sftp_bogus++, SFTP_FreeBuffer(&pb))

/*------------- Procedures directly invoked by RPC2 ---------------*/

long SFTP_Init()
{
    say(1, SFTP_DebugLevel, "SFTP_Init()\n");

    sftp_InitTrace();

    /* Register SFTP packet handler with RPC2 socket listener */
    SL_RegisterHandler(SFTPVERSION, sftp_ExaminePacket);

    return (RPC2_SUCCESS);
}

void SFTP_SetDefaults(SFTP_Initializer *initPtr)
/* Should be called before SFTP_Activate() */
{
    initPtr->PacketSize    = SFTP_DEFPACKETSIZE;
    initPtr->WindowSize    = SFTP_DEFWINDOWSIZE;
    initPtr->RetryCount    = 10;
    initPtr->RetryInterval = 2000; /* milliseconds */
    initPtr->SendAhead =
        SFTP_DEFSENDAHEAD; /* make sure it is less than 16, for readv() */
    initPtr->AckPoint     = SFTP_DEFSENDAHEAD; /* same as SendAhead */
    initPtr->EnforceQuota = 0;
    initPtr->DoPiggy      = TRUE;
    initPtr->DupThreshold = 4;
    initPtr->MaxPackets   = -1;
    initPtr->Port.Tag     = RPC2_PORTBYINETNUMBER;
    initPtr->Port.Value.InetPortNumber = htons(0);
}

void SFTP_Activate(SFTP_Initializer *initPtr)
/* Should be called before RPC2_Init() */
{
    struct SE_Definition *sed;
    long size;

    if (initPtr != NULL) {
        SFTP_PacketSize = initPtr->PacketSize;
        SFTP_WindowSize = initPtr->WindowSize;
        // SFTP_RetryCount = initPtr->RetryCount;
        // SFTP_RetryInterval = initPtr->RetryInterval;	/* milliseconds */
        SFTP_EnforceQuota = initPtr->EnforceQuota;
        SFTP_SendAhead    = initPtr->SendAhead;
        SFTP_AckPoint     = initPtr->AckPoint;
        SFTP_DoPiggy      = initPtr->DoPiggy;
        SFTP_DupThreshold = initPtr->DupThreshold;
        SFTP_MaxPackets   = initPtr->MaxPackets;
    }
    assert(SFTP_SendAhead <= 16); /* 'cause of readv() bogosity */

    /* Enlarge table by one */
    SE_DefCount++;
    size = sizeof(struct SE_Definition) * SE_DefCount;
    if (SE_DefSpecs == NULL)
        /* The realloc() on the romp dumps core if SE_DefSpecs is NULL */
        SE_DefSpecs = (struct SE_Definition *)malloc(size);
    else
        SE_DefSpecs = (struct SE_Definition *)realloc(SE_DefSpecs, size);
    assert(SE_DefSpecs != NULL);

    /* Add this side effect's info to last entry in table */
    sed                       = &SE_DefSpecs[SE_DefCount - 1];
    sed->SideEffectType       = SMARTFTP;
    sed->SE_Init              = SFTP_Init;
    sed->SE_Bind1             = SFTP_Bind1;
    sed->SE_Bind2             = SFTP_Bind2;
    sed->SE_Unbind            = SFTP_Unbind;
    sed->SE_NewConnection     = SFTP_NewConn;
    sed->SE_MakeRPC1          = SFTP_MakeRPC1;
    sed->SE_MakeRPC2          = SFTP_MakeRPC2;
    sed->SE_MultiRPC1         = SFTP_MultiRPC1;
    sed->SE_MultiRPC2         = SFTP_MultiRPC2;
    sed->SE_CreateMgrp        = SFTP_CreateMgrp;
    sed->SE_AddToMgrp         = SFTP_AddToMgrp;
    sed->SE_InitMulticast     = SFTP_InitMulticast;
    sed->SE_DeleteMgrp        = SFTP_DeleteMgrp;
    sed->SE_GetRequest        = SFTP_GetRequest;
    sed->SE_InitSideEffect    = SFTP_InitSE;
    sed->SE_CheckSideEffect   = SFTP_CheckSE;
    sed->SE_SendResponse      = SFTP_SendResponse;
    sed->SE_PrintSEDescriptor = SFTP_PrintSED;
    sed->SE_GetSideEffectTime = SFTP_GetTime;
    sed->SE_GetHostInfo       = SFTP_GetHostInfo;
}

long SFTP_Bind1(IN RPC2_Handle ConnHandle, IN RPC2_CountedBS *ClientIdent)
{
    struct SFTP_Entry *se;

    say(1, SFTP_DebugLevel, "SFTP_Bind()\n");

    se              = sftp_AllocSEntry(); /* malloc and initialize SFTP_Entry */
    se->WhoAmI      = SFCLIENT;
    se->LocalHandle = ConnHandle;
    se->sa          = rpc2_GetSA(ConnHandle);
    RPC2_SetSEPointer(ConnHandle, se);
    return (RPC2_SUCCESS);
}

long SFTP_Bind2(IN RPC2_Handle ConnHandle, IN RPC2_Unsigned BindTime)
{
    struct SFTP_Entry *se;

    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    RPC2_GetPeerInfo(ConnHandle, &se->PInfo);

    /* Depending on rpc2_ipv6ready, rpc2_splitaddrinfo might return a simple
     * IPv4 address. Convert it back to the more useful RPC2_addrinfo... */
    rpc2_simplifyHost(&se->PInfo.RemoteHost, &se->PInfo.RemotePort);

    assert(se->PInfo.RemoteHost.Tag == RPC2_HOSTBYADDRINFO);
    se->HostInfo = rpc2_GetHost(se->PInfo.RemoteHost.Value.AddrInfo);
    assert(se->HostInfo);

    return (RPC2_SUCCESS);
}

long SFTP_Unbind(IN RPC2_Handle ConnHandle)
{
    struct SFTP_Entry *se;

    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    if (se)
        sftp_FreeSEntry(se);
    RPC2_SetSEPointer(ConnHandle, NULL);
    return (RPC2_SUCCESS);
}

long SFTP_NewConn(IN RPC2_Handle ConnHandle, IN RPC2_CountedBS *ClientIdent)
{
    struct SFTP_Entry *se;

    say(1, SFTP_DebugLevel, "SFTP_NewConn()\n");

    se              = sftp_AllocSEntry(); /* malloc and initialize */
    se->WhoAmI      = SFSERVER;
    se->LocalHandle = ConnHandle;
    RPC2_GetPeerInfo(ConnHandle, &se->PInfo);

    /* Depending on rpc2_ipv6ready, rpc2_splitaddrinfo might return a simple
     * IPv4 address. Convert it back to the more useful RPC2_addrinfo... */
    rpc2_simplifyHost(&se->PInfo.RemoteHost, &se->PInfo.RemotePort);

    assert(se->PInfo.RemoteHost.Tag == RPC2_HOSTBYADDRINFO);
    se->HostInfo = rpc2_GetHost(se->PInfo.RemoteHost.Value.AddrInfo);
    assert(se->HostInfo);

    se->sa = rpc2_GetSA(ConnHandle);
    RPC2_SetSEPointer(ConnHandle, se);

    return (RPC2_SUCCESS);
}

long SFTP_MakeRPC1(IN RPC2_Handle ConnHandle, INOUT SE_Descriptor *SDesc,
                   INOUT RPC2_PacketBuffer **RequestPtr)
{
    struct SFTP_Entry *se;
    int rc;
    off_t len;

    say(1, SFTP_DebugLevel, "SFTP_MakeRPC1 ()\n");

    SDesc->LocalStatus  = SE_SUCCESS; /* non-execution == success */
    SDesc->RemoteStatus = SE_SUCCESS; /* non-execution == success */
    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);
    if (se->WhoAmI != SFCLIENT)
        FAIL(se, RPC2_SEFAIL2);
    se->ThisRPCCall =
        (*RequestPtr)->Header.SeqNumber; /* remember new call has begun */
    se->SDesc = SDesc;
    sftp_Progress(SDesc, 0);

    se->XferState = XferNotStarted;
    se->HitEOF    = FALSE;
    if (SDesc->Value.SmartFTPD.TransmissionDirection == CLIENTTOSERVER) {
        se->SendMostRecent   = se->SendLastContig;
        se->SendWorriedLimit = se->SendLastContig;
        se->SendAckLimit     = se->SendLastContig;
        memset(se->SendTheseBits, 0, BITMASKWIDTH * sizeof(long));
        se->ReadAheadCount = 0;
        rc                 = sftp_InitIO(se);
    } else {
        se->RecvMostRecent = se->RecvLastContig;
        memset(se->RecvTheseBits, 0, BITMASKWIDTH * sizeof(long));
        rc = sftp_InitIO(se);
    }
    if (rc < 0) {
        SDesc->LocalStatus = SE_FAILURE;
        FAIL(se, RPC2_SEFAIL1);
    }

    /* Piggyback SFTP parms if this is the very first call */
    if (se->SentParms == FALSE) {
        rc = sftp_AppendParmsToPacket(se, RequestPtr);
        if (rc < 0)
            FAIL(se, RPC2_SEFAIL4);
    } else
        /* Try piggybacking file on store */
        if (SDesc->Value.SmartFTPD.TransmissionDirection == CLIENTTOSERVER &&
            SFTP_DoPiggy) {
            len = sftp_AppendFileToPacket(se, RequestPtr);
            switch (len) {
            case -1:
                FAIL(se, RPC2_SEFAIL4);

            case -2:
                break; /* file too big to fit */

            default:
                sftp_Progress(SDesc, len);
                sftp_didpiggy++;
                break;
            }
        }

    return (RPC2_SUCCESS);
}

long SFTP_MakeRPC2(IN RPC2_Handle ConnHandle, INOUT SE_Descriptor *SDesc,
                   INOUT RPC2_PacketBuffer *Reply)
{
    struct SFTP_Entry *se;
    int i;
    off_t nbytes;

    say(1, SFTP_DebugLevel, "SFTP_MakeRPC2()\n");

    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS);

    SDesc->LocalStatus = SDesc->RemoteStatus = SE_SUCCESS; /* tentatively */

    /* Pluck off  file if piggybacked  */
    if (Reply != NULL && (Reply->Header.SEFlags & SFTP_PIGGY) &&
        SDesc->Value.SmartFTPD.TransmissionDirection == SERVERTOCLIENT) {
        nbytes = sftp_ExtractFileFromPacket(se, Reply);
        if (nbytes >= 0) {
            sftp_didpiggy++;
            sftp_Progress(SDesc, nbytes);
        } else {
            SDesc->LocalStatus = SE_FAILURE;
            sftp_SetError(se, DISKERROR); /* what else could it be? */
        }
    }

    /* Clean up local state */
    for (i = 0; i < MAXOPACKETS; i++)
        if (se->ThesePackets[i] != NULL)
            SFTP_FreeBuffer(&se->ThesePackets[i]);
    sftp_vfclose(se);
    se->SDesc          = NULL;
    se->SendLastContig = se->SendMostRecent;
    se->RecvLastContig = se->RecvMostRecent;
    memset(se->SendTheseBits, 0, sizeof(int) * BITMASKWIDTH);
    memset(se->RecvTheseBits, 0, sizeof(int) * BITMASKWIDTH);

    /* Determine return code */
    if (Reply == NULL)
        return (RPC2_SUCCESS); /* so base RPC2 code will carry on */
    if (se->WhoAmI == DISKERROR) {
        SDesc->LocalStatus = SE_FAILURE;
        return (RPC2_SEFAIL3);
    }
    if (se->XferState == XferInProgress &&
        !(Reply->Header.SEFlags & SFTP_ALLOVER)) {
        sftp_SetError(se, ERROR);
        SDesc->RemoteStatus = SE_FAILURE;
        return (RPC2_SEFAIL2);
    }

    return (RPC2_SUCCESS);
}

long SFTP_GetRequest(RPC2_Handle ConnHandle, RPC2_PacketBuffer *Request)
{
    struct SFTP_Entry *se;
    off_t len;

    say(1, SFTP_DebugLevel, "SFTP_GetRequest()\n");

    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS && se != NULL);
    if (se->WhoAmI != SFSERVER)
        FAIL(se, RPC2_SEFAIL2);
    se->ThisRPCCall =
        Request->Header.SeqNumber; /* acquire client's RPC call number */

    se->PiggySDesc = NULL; /* default is no piggybacked file */
    if ((Request->Header.SEFlags & SFTP_PIGGY)) {
        if (se->GotParms == FALSE) {
            if (sftp_ExtractParmsFromPacket(se, Request) < 0)
                FAIL(se, RPC2_SEFAIL2);
        } else {
            /* Save file for future InitSE and CheckSE */
            len = Request->Header.BodyLength - Request->Header.SEDataOffset;
            sftp_AllocPiggySDesc(se, len, CLIENTTOSERVER);
            se->SDesc = se->PiggySDesc;
            assert(sftp_ExtractFileFromPacket(se, Request) >= 0);
            sftp_didpiggy++;
        }
    }

    return (RPC2_SUCCESS);
}

long SFTP_InitSE(RPC2_Handle ConnHandle, SE_Descriptor *SDesc)
{
    struct SFTP_Entry *se;
    int rc;

    say(1, SFTP_DebugLevel, "SFTP_InitSE ()\n");

    SDesc->LocalStatus  = SE_NOTSTARTED;
    SDesc->RemoteStatus = SE_NOTSTARTED;
    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS && se != NULL);
    if (se->WhoAmI != SFSERVER)
        FAIL(se, RPC2_SEFAIL2);
    if (se->GotParms == FALSE)
        FAIL(se, RPC2_SEFAIL2);
    se->SDesc = SDesc;

    rc = sftp_InitIO(se);
    if (rc < 0) {
        SDesc->LocalStatus = SE_FAILURE;
        se->SDesc          = NULL;
        return (RPC2_SEFAIL1);
    }
    return (RPC2_SUCCESS);
}

long SFTP_CheckSE(RPC2_Handle ConnHandle, SE_Descriptor *SDesc, long Flags)
{
    long rc;
    off_t flen;
    struct SFTP_Entry *se;
    struct SFTP_Descriptor *sftpd;
    struct FileInfoByAddr *p;

    say(1, SFTP_DebugLevel, "SFTP_CheckSE()\n");

    if (Flags == 0)
        return (RPC2_SUCCESS);
    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS && se != NULL);
    if (se->WhoAmI != SFSERVER)
        FAIL(se, RPC2_SEFAIL2);
    se->SDesc = SDesc;

    if (SDesc->LocalStatus != SE_NOTSTARTED ||
        SDesc->RemoteStatus != SE_NOTSTARTED)
        return (RPC2_SUCCESS); /* SDesc error conditions are self-describing */
    SDesc->LocalStatus = SDesc->RemoteStatus = SE_INPROGRESS;
    sftp_Progress(SDesc, 0);

    sftpd = &SDesc->Value.SmartFTPD;
    if (sftpd->hashmark != 0)
        switch (sftpd->Tag) {
        case FILEBYNAME:
            say(1, SFTP_DebugLevel,
                "%s: ", sftpd->FileInfo.ByName.LocalFileName);
            break;

        case FILEBYINODE:
            say(1, SFTP_DebugLevel, "%ld.%ld: ", sftpd->FileInfo.ByInode.Device,
                sftpd->FileInfo.ByInode.Inode);
            break;

        case FILEBYFD:
            say(1, SFTP_DebugLevel, "%ld: ", sftpd->FileInfo.ByFD.fd);
            break;

        case FILEINVM:
            say(1, SFTP_DebugLevel,
                "%p[%u, %u]: ", sftpd->FileInfo.ByAddr.vmfile.SeqBody,
                sftpd->FileInfo.ByAddr.vmfile.MaxSeqLen,
                sftpd->FileInfo.ByAddr.vmfile.SeqLen);
            break;
        }

    switch (sftpd->TransmissionDirection) {
    case CLIENTTOSERVER:
        if (se->PiggySDesc) {
            /* use squirrelled-away data */
            p  = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;
            rc = sftp_vfwritefile(se, (char *)p->vmfile.SeqBody,
                                  p->vmfile.SeqLen);
            if (rc < 0) {
                sftp_SetError(se, DISKERROR);
                se->SDesc->LocalStatus = SE_FAILURE;
            } else {
                rc                     = RPC2_SUCCESS;
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
        if (SFTP_DoPiggy == FALSE || ((Flags & SE_AWAITREMOTESTATUS) != 0) ||
            (flen >= SFTP_MAXBODYSIZE)) {
            /* can't defer transfer to SendResponse */
            rc = PutFile(se);
        } else {
            /* Squirrel away contents of file.
               We have to save the data right away, because the
               the server may delete or modify the file after
               CheckSideEffect but before  SendResponse. */

            sftp_AllocPiggySDesc(se, flen, SERVERTOCLIENT);
            p  = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;
            rc = sftp_piggybackfileread(se, (char *)p->vmfile.SeqBody);
            if (rc < 0) {
                sftp_SetError(se, DISKERROR);
                se->SDesc->LocalStatus = SE_FAILURE;
            } else {
                rc                     = RPC2_SUCCESS;
                se->SDesc->LocalStatus = SE_SUCCESS;
                sftp_Progress(se->SDesc, p->vmfile.SeqLen);
            }
        }
        break;

    default:
        FAIL(se, RPC2_SEFAIL1);
    }

    sftp_vfclose(se);
    se->SDesc = NULL;
    return (rc);
}

long SFTP_SendResponse(IN RPC2_Handle ConnHandle, IN RPC2_PacketBuffer **Reply)
{
    struct SFTP_Entry *se;
    long rc;

    say(1, SFTP_DebugLevel, "SFTP_SendResponse()\n");

    assert(RPC2_GetSEPointer(ConnHandle, &se) == RPC2_SUCCESS && se != NULL);

    /* SDesc is pretty much guaranteed to be an invalid pointer by now. It is
     * commonly on the stack when InitSE and CheckSE are called, but the stack
     * has been unwound by the time we hit SendResponse. It should be ok,
     * cleanup has already happened in CheckSE */
    se->SDesc = NULL;

    rc = RPC2_SUCCESS;
    if (se->PiggySDesc) {
        /* Deal with file saved for piggybacking */

        if (se->PiggySDesc->Value.SmartFTPD.TransmissionDirection ==
            SERVERTOCLIENT) {
            /* File is indeed for the client;  the only other way PiggySDesc can
               be non-null is when a file is stored piggybacked, but the server
               does not do an InitSideEffect or CheckSideEffect */

            se->SDesc = se->PiggySDesc; /* so that PutFile is happy */
            switch (sftp_AppendFileToPacket(se, Reply)) {
            case -1:
                rc = RPC2_SEFAIL4;
                break;

            case -2: /* File was too big */
                rc = PutFile(se);
                break;

            default: /* bytes were appended */
                rc = RPC2_SUCCESS;
                sftp_didpiggy++;
                break;
            }
        }
        /* clean up state */
        sftp_vfclose(se);
        sftp_FreePiggySDesc(se); /* always get rid of the file */
    }

    if (se->WhoAmI == ERROR) {
        (*Reply)->Header.SEFlags &= ~SFTP_ALLOVER; /* confirm SE failure */
        return (RPC2_SUCCESS); /* but allow this SendResponse to get out */
    } else {
        /* indicate all outstanding packets received */
        (*Reply)->Header.SEFlags |= SFTP_ALLOVER;
    }

    return (rc);
}

long SFTP_GetTime(IN RPC2_Handle ConnHandle, INOUT struct timeval *Time)
{
    struct SFTP_Entry *se;
    long rc;

    say(1, SFTP_DebugLevel, "SFTP_GetTime()\n");

    se = NULL;
    /*
     * We might legitimately fail here because there is no connection,
     * there is a connection but it is not completely set up.
     */
    if ((rc = RPC2_GetSEPointer(ConnHandle, &se)) != RPC2_SUCCESS)
        return (rc);

    if (se == NULL || se->HostInfo == NULL)
        return (RPC2_NOCONNECTION);

    *Time = se->LastWord;
    return (RPC2_SUCCESS);
}

long SFTP_GetHostInfo(IN RPC2_Handle ConnHandle, INOUT struct HEntry **HPtr)
{
    struct SFTP_Entry *se;
    long rc;

    say(1, SFTP_DebugLevel, "SFTP_GetHostInfo()\n");

    se = NULL;
    if ((rc = RPC2_GetSEPointer(ConnHandle, &se)) != RPC2_SUCCESS)
        return (rc);

    if (se == NULL)
        return (RPC2_NOCONNECTION);

    assert(se->HostInfo);
    *HPtr = se->HostInfo;
    return (RPC2_SUCCESS);
}

/*-------------------- Data transmission routines -----------------------*/
static long GetFile(struct SFTP_Entry *sEntry)
/* Local file is already opened */
{
    struct CEntry *ce;
    long packetsize;
    int startmode = TRUE;
    int rxmit     = 0;
    RPC2_PacketBuffer *pb;
    int rc;

    sEntry->XferState = XferInProgress;
    sftp_Progress(sEntry->SDesc, 0);
    sEntry->SDesc->Value.SmartFTPD.QuotaExceeded = 0;
    sEntry->HitEOF                               = FALSE;
    sEntry->RecvMostRecent                       = sEntry->RecvLastContig;
    sEntry->RecvFirst                            = sEntry->RecvLastContig + 1;
    memset(sEntry->RecvTheseBits, 0, sizeof(int) * BITMASKWIDTH);

    ce                  = rpc2_GetConn(sEntry->LocalHandle);
    sEntry->TimeEcho    = ce ? ce->TimeStampEcho : 0;
    sEntry->RequestTime = ce ? ce->RequestTime : 0;

    /* Set timeout to be large enough for size of (possibly sent) ack + size
     * of the next data packet */
    packetsize = sEntry->PacketSize + sizeof(struct RPC2_PacketHeader);
    while (sEntry->XferState == XferInProgress) {
        /* normally the trigger is sent when data arrives, so we only need to
         * send here if the retransmission timer expired */
        if (startmode || rxmit > 0) {
            if (rxmit > Retry_N) {
                sftp_SetError(sEntry, ERROR);
                QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
            }

            if (startmode)
                rc = sftp_SendStart(sEntry);
            else
                rc = sftp_SendTrigger(sEntry);

            if (rc < 0)
                QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
        }

        pb = AwaitPacket(sEntry, rxmit, sizeof(struct RPC2_PacketHeader),
                         packetsize);

        /* Make sure nothing bad happened while we were waiting */
        if (sEntry->WhoAmI == ERROR) {
            if (pb) {
                SFTP_FreeBuffer(&pb);
            }
            QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
        }
        if (sEntry->WhoAmI == DISKERROR) {
            if (pb) {
                SFTP_FreeBuffer(&pb);
            }
            QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL3);
        }

        /* Did we receive a packet? */
        if (pb == NULL) {
            rxmit++;
            sftp_timeouts++;
            sEntry->Retransmitting = TRUE;
            say(4, SFTP_DebugLevel, "GetFile: Backoff\n");
            continue;
        }

        /* If so process it */
        switch ((int)pb->Header.Opcode) {
        case SFTP_BUSY:
            sftp_Recvd.Busies++;
            if (startmode) {
                sftp_busy++;
                rxmit = -1;
                SFTP_FreeBuffer(&pb);
                continue;
            }
        /* fall through */
        default:
            BOGUS(pb);
            continue;

        case SFTP_DATA:
            break;
        }
        rxmit     = 0;
        startmode = FALSE;

        rc = sftp_DataArrived(pb, sEntry);
        if (rc < 0) {
            if (sEntry->WhoAmI == DISKERROR)
                QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL3);
            else
                QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
        }
    }
    QUIT(sEntry, SE_SUCCESS, RPC2_SUCCESS);
}

/* Local file is already opened */
static long PutFile(struct SFTP_Entry *sEntry)
{
    RPC2_PacketBuffer *pb;
    struct CEntry *ce;
    int i, rc = 0;
    unsigned long bytes;

    sftp_Progress(sEntry->SDesc, 0);
    sEntry->SDesc->Value.SmartFTPD.QuotaExceeded = 0;
    sEntry->HitEOF                               = FALSE;
    sEntry->XferState                            = XferInProgress;
    sEntry->SendMostRecent   = sEntry->SendLastContig; /* Really redundant */
    sEntry->SendWorriedLimit = sEntry->SendLastContig; /* Really redundant */
    sEntry->SendAckLimit     = sEntry->SendLastContig; /* Really redundant */

    /* Kip: instead of sEntry->TimeEcho = 0, we will go ahead and
     * use the Timestamp from the first rpc2 packet since the putfile
     * may not span more than one round-trip, and no RTT update would
     * occur. */
    ce                  = rpc2_GetConn(sEntry->LocalHandle);
    sEntry->TimeEcho    = ce ? ce->TimeStampEcho : 0;
    sEntry->RequestTime = ce ? ce->RequestTime : 0;

    memset(sEntry->SendTheseBits, 0, sizeof(int) * BITMASKWIDTH);

    if (sftp_SendStrategy(sEntry) < 0) {
        QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
    }

    /* estimated size of an sftp data transfer */
    bytes = ((sEntry->PacketSize + sizeof(struct RPC2_PacketHeader)) *
             sEntry->AckPoint);

    while (sEntry->XferState == XferInProgress) {
        for (i = 0; i < Retry_N; i++) {
            pb =
                AwaitPacket(sEntry, i, bytes, sizeof(struct RPC2_PacketHeader));

            /* Make sure nothing bad happened while we were waiting */
            if (sEntry->WhoAmI == ERROR) {
                if (pb) {
                    SFTP_FreeBuffer(&pb);
                }
                QUIT(sEntry, SE_FAILURE, RPC2_SEFAIL2);
            }
            if (sEntry->WhoAmI == DISKERROR) {
                if (pb) {
                    SFTP_FreeBuffer(&pb);
                }
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
        sEntry->Retransmitting = FALSE;

        switch ((int)pb->Header.Opcode) {
        case SFTP_NAK:
            assert(FALSE); /* should have already set sEntry state */

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

static RPC2_PacketBuffer *sftp_DequeuePacket(struct SFTP_Entry *sEntry)
{
    RPC2_PacketBuffer *victim = rpc2_LE2PB(sEntry->RecvQueue);
    if (victim)
        rpc2_MoveEntry(&sEntry->RecvQueue, &rpc2_PBList, &victim->Prefix.LE,
                       &sEntry->RecvQueueLen, &rpc2_PBCount);
    return victim;
}

static RPC2_PacketBuffer *AwaitPacket(struct SFTP_Entry *sEntry, int retry,
                                      int outbytes, int inbytes)
/* Awaits for a packet on sEntry
   Returns pointer to arrived packet or NULL
*/
{
    struct SL_Entry *sl;
    struct CEntry *ce;
    int rc;

    /* Check if there is something already queued up */
    if (sEntry->RecvQueue)
        return sftp_DequeuePacket(sEntry);

    if (LWP_GetRock(SMARTFTP, (void *)&sl) != LWP_SUCCESS) {
        sl = rpc2_AllocSle(OTHER, NULL);
        assert(LWP_NewRock(SMARTFTP, (char *)sl) == LWP_SUCCESS);
    }

    ce = rpc2_GetConn(sEntry->LocalHandle);
    rc = rpc2_RetryInterval(ce, retry, &sl->RInterval, outbytes, inbytes, 1);
    if (rc) {
        sl->ReturnCode = 0;
        return NULL; /* TIMEOUT */
    }

    sEntry->Sleeper = sl;
    rpc2_ActivateSle(sl, &sl->RInterval);
    LWP_WaitProcess((char *)sl);

    assert(sl->ReturnCode == TIMEOUT || sl->ReturnCode == ARRIVED);

    sl->ReturnCode = 0;
    return sftp_DequeuePacket(sEntry);
}

/*---------------------- Piggybacking routines -------------------------*/

static long MakeBigEnough(RPC2_PacketBuffer **whichP, off_t extraBytes,
                          long maxSize)
/* Checks if whichP is a packet buffer to which extraBytes can be appended.
   If so returns whichP unmodified.
   Otherwise, allocates new packet, copies old contents to it, and sets
   whichP to new packet. If reallocation would cause packet size to exceed
   maxSize, no reallocation is done.
   Returns 0 if extraBytes can be appended to whichP; -1 otherwise.
*/
{
    long freebytes, curlen;
    RPC2_PacketBuffer *pb;

    curlen    = (*whichP)->Header.BodyLength + sizeof(struct RPC2_PacketHeader);
    freebytes = (*whichP)->Prefix.BufferSize -
                sizeof(struct RPC2_PacketBufferPrefix) - curlen;
    if (freebytes >= extraBytes)
        return (0);
    if (curlen + extraBytes > maxSize)
        return (-1);

    /* Realloc and copy */
    assert(extraBytes <= INT_MAX); /* LFS */
    RPC2_AllocBuffer(extraBytes + (*whichP)->Header.BodyLength, &pb);
    // pb->Header.BodyLength = (*whichP)->Header.BodyLength;
    memcpy(&pb->Header, &(*whichP)->Header, curlen);
    pb->Prefix.sa = (*whichP)->Prefix.sa;
    *whichP       = pb; /* DON'T free old packet !!! */
    return (0);
}

int sftp_AddPiggy(RPC2_PacketBuffer **whichP, char *dPtr, off_t dSize,
                  unsigned int maxSize)
/* whichP	- packet to be enlarged
 * dPtr		- data to be piggybacked
 * dSize	- length of data at dPtr
 * maxSize	- how large whichP can grow to */
/* If specified data can be piggy backed within a packet no larger than maxSize,
   adds the data and sets SEFlags and SEDataOffset. Enlarges packet if needed.
   Returns 0 if data has been piggybacked, -1 if maxSize would be exceeded
*/
{
    assert(dSize <= INT_MAX); /* LFS */
    say(9, SFTP_DebugLevel, "sftp_AddPiggy: %d\n", (int)dSize);

    if (MakeBigEnough(whichP, dSize, maxSize) < 0)
        return (-1);

    /* We allow multiple piggybacked items, so only initialize SEDataOffset
     * once. */
    if (((*whichP)->Header.SEFlags & SFTP_PIGGY) == 0) {
        (*whichP)->Header.SEDataOffset =
            (RPC2_Unsigned)((*whichP)->Header.BodyLength);
        (*whichP)->Header.SEFlags |= SFTP_PIGGY;
    }

    memcpy((*whichP)->Body + (*whichP)->Header.BodyLength, dPtr, dSize);
    (*whichP)->Header.BodyLength += dSize;
    (*whichP)->Prefix.LengthOfPacket =
        sizeof(struct RPC2_PacketHeader) + (*whichP)->Header.BodyLength;
    return (0);
}

off_t sftp_AppendFileToPacket(struct SFTP_Entry *sEntry,
                              RPC2_PacketBuffer **whichP)
/* Tries to add a file to the end of whichP
   Returns:
    +X  if X bytes have been piggybacked.
    -1 in case of system call failure
    -2 if appending file would make packet larger than SFTP_MAXPACKETSIZE
*/
{
    long rc, maxbytes;
    off_t filelen;
    struct CEntry *ce;
    /* buffer for read(); avoids huge local on my stack */
    static char GlobalJunk[SFTP_MAXBODYSIZE];

    filelen = sftp_piggybackfilesize(sEntry);
    if (filelen < 0)
        return (-1);

    /* now check if there is space in the packet */
    maxbytes = SFTP_MAXBODYSIZE - (*whichP)->Header.BodyLength;

#if 1
    /* Only piggy as much as a default sftp packet */
    if (sEntry->PacketSize < SFTP_MAXPACKETSIZE)
        maxbytes -= (SFTP_MAXPACKETSIZE - sEntry->PacketSize);
#endif

    if (filelen > (off_t)maxbytes)
        return (-2);

    /* enough space: append the file! */
    rc = sftp_piggybackfileread(sEntry, GlobalJunk);
    if (rc < 0)
        return (-1);
    assert(!sftp_AddPiggy(whichP, GlobalJunk, filelen, SFTP_MAXPACKETSIZE));
    sEntry->HitEOF = TRUE;
    ce             = rpc2_GetConn(sEntry->LocalHandle);
    if (ce)
        ce->reqsize += filelen;

    /* cleanup and quit */
    sftp_vfclose(sEntry);
    return (filelen);
}

off_t sftp_ExtractFileFromPacket(struct SFTP_Entry *sEntry,
                                 RPC2_PacketBuffer *whichP)
/* Plucks off piggybacked file.
   Returns number of bytes plucked off, or < 0 if error */
{
    long rc;
    off_t len;

    len = whichP->Header.BodyLength - whichP->Header.SEDataOffset;
    rc  = sftp_vfwritefile(
        sEntry, (char *)&whichP->Body[whichP->Header.BodyLength - len], len);
    sftp_vfclose(sEntry);
    if (rc < 0)
        return (rc);

    /* shorten the packet */
    whichP->Header.BodyLength -= len;
    return (len);
}

int sftp_AppendParmsToPacket(struct SFTP_Entry *sEntry,
                             RPC2_PacketBuffer **whichP)
/* Clients append parms to RPC request packets,
   servers to SE control packets.
   Returns 0 on success, -1 on failure */
{
    struct SFTP_Parms sp;
    RPC2_PortIdent nullport;
    nullport.Tag                  = 0;
    nullport.Value.InetPortNumber = 0;

    sp.Port         = nullport; /* structure assignment */
    sp.Port.Tag     = (PortTag)htonl(sp.Port.Tag);
    sp.WindowSize   = htonl(sEntry->WindowSize);
    sp.SendAhead    = htonl(sEntry->SendAhead);
    sp.AckPoint     = htonl(sEntry->AckPoint);
    sp.PacketSize   = htonl(sEntry->PacketSize);
    sp.DupThreshold = htonl(sEntry->DupThreshold);

    assert(sftp_AddPiggy(whichP, (char *)&sp, sizeof(sp), RPC2_MAXPACKETSIZE) ==
           0);

    switch (sEntry->WhoAmI) {
    case SFCLIENT:
        sEntry->SentParms = TRUE;
        break;

    case SFSERVER:
        break;

    default:
        return (-1);
    }

    return (0);
}

int sftp_ExtractParmsFromPacket(struct SFTP_Entry *sEntry,
                                RPC2_PacketBuffer *whichP)
/* Plucks off piggybacked parms. Returns 0 on success, -1 on failure */
{
    struct SFTP_Parms sp;

    if (whichP->Header.BodyLength - whichP->Header.SEDataOffset <
        sizeof(struct SFTP_Parms))
        return (-1);

    /* We copy out the data physically:
       else structure alignment problem on IBM-RTs */
    memcpy(&sp,
           &whichP->Body[whichP->Header.BodyLength - sizeof(struct SFTP_Parms)],
           sizeof(struct SFTP_Parms));

    sp.WindowSize   = ntohl(sp.WindowSize);
    sp.SendAhead    = ntohl(sp.SendAhead);
    sp.AckPoint     = ntohl(sp.AckPoint);
    sp.PacketSize   = ntohl(sp.PacketSize);
    sp.DupThreshold = ntohl(sp.DupThreshold);

    if (sEntry->WhoAmI == SFSERVER) {
        /* Find smaller of two side's parms: we will send these values on the first
         * Start */
        if (sEntry->WindowSize > sp.WindowSize)
            sEntry->WindowSize = sp.WindowSize;
        if (sEntry->SendAhead > sp.SendAhead)
            sEntry->SendAhead = sp.SendAhead;
        if (sEntry->AckPoint > sp.AckPoint)
            sEntry->AckPoint = sp.AckPoint;
        if (sEntry->PacketSize > sp.PacketSize)
            sEntry->PacketSize = sp.PacketSize;
        if (sEntry->DupThreshold > sp.DupThreshold)
            sEntry->DupThreshold = sp.DupThreshold;
    } else {
        /* Accept Server's parms without question. */
        sEntry->WindowSize   = sp.WindowSize;
        sEntry->SendAhead    = sp.SendAhead;
        sEntry->AckPoint     = sp.AckPoint;
        sEntry->PacketSize   = sp.PacketSize;
        sEntry->DupThreshold = sp.DupThreshold;
    }
    sEntry->GotParms = TRUE;

    if (sEntry->WindowSize < SFTP_MINWINDOWSIZE)
        sEntry->WindowSize = SFTP_MINWINDOWSIZE;
    if (sEntry->SendAhead < SFTP_MINSENDAHEAD)
        sEntry->SendAhead = SFTP_MINSENDAHEAD;
    if (sEntry->PacketSize < SFTP_MINPACKETSIZE)
        sEntry->PacketSize = SFTP_MINPACKETSIZE;

    say(9, SFTP_DebugLevel, "GotParms: %d %d %d %d %d\n", sEntry->WindowSize,
        sEntry->SendAhead, sEntry->AckPoint, sEntry->PacketSize,
        sEntry->DupThreshold);

    whichP->Header.BodyLength -= sizeof(struct SFTP_Parms);

    return (0);
}

/*---------------------- Miscellaneous routines ----------------------*/
struct SFTP_Entry *sftp_AllocSEntry(void)
{
    struct SFTP_Entry *sfp;

    assert((sfp = (struct SFTP_Entry *)malloc(sizeof(struct SFTP_Entry))) !=
           NULL);
    memset(sfp, 0, sizeof(struct SFTP_Entry)); /* all fields initialized to 0 */
    sfp->Magic          = SFTPMAGIC;
    sfp->openfd         = -1;
    sfp->fd_offset      = 0;
    sfp->PacketSize     = SFTP_PacketSize;
    sfp->WindowSize     = SFTP_WindowSize;
    sfp->SendAhead      = SFTP_SendAhead;
    sfp->AckPoint       = SFTP_AckPoint;
    sfp->DupThreshold   = SFTP_DupThreshold;
    sfp->Retransmitting = FALSE;
    sfp->RequestTime    = 0;
    sfp->RecvQueue      = NULL;
    sfp->RecvQueueLen   = 0;
    CLRTIME(&sfp->LastWord);
    return (sfp);
}

void sftp_FreeSEntry(struct SFTP_Entry *se)
{
    struct SL_Entry *sl;
    int i;

    /* wake up and destroy any threads still waiting for incoming packets */
    sl = se->Sleeper;
    if (sl) {
        se->WhoAmI  = ERROR;
        se->Sleeper = NULL;
        rpc2_DeactivateSle(sl, TIMEOUT);
        LWP_SignalProcess((char *)sl);
    }

    sftp_vfclose(se);
    if (se->PiggySDesc)
        sftp_FreePiggySDesc(se);
    for (i = 0; i < MAXOPACKETS; i++)
        if (se->ThesePackets[i] != NULL)
            SFTP_FreeBuffer(&se->ThesePackets[i]);
    if (se->HostInfo)
        rpc2_FreeHost(&se->HostInfo);
    free(se);
}

void sftp_AllocPiggySDesc(struct SFTP_Entry *se, off_t len,
                          enum WhichWay direction)
{
    struct FileInfoByAddr *p;

    assert(se->PiggySDesc == NULL); /* can't already exist */
    se->PiggySDesc = (SE_Descriptor *)malloc(sizeof(SE_Descriptor));
    assert(se->PiggySDesc); /* malloc failure is fatal */

    memset(se->PiggySDesc, 0, sizeof(SE_Descriptor));
    se->PiggySDesc->Value.SmartFTPD.Tag                   = FILEINVM;
    se->PiggySDesc->Value.SmartFTPD.TransmissionDirection = direction;
    /* maintain quotas -- no random values! */
    if (SFTP_EnforceQuota && se->SDesc)
        se->PiggySDesc->Value.SmartFTPD.ByteQuota =
            se->SDesc->Value.SmartFTPD.ByteQuota;

    p = &se->PiggySDesc->Value.SmartFTPD.FileInfo.ByAddr;
    /* 0 length malloc()s choke; fake a 1-byte file */
    if (len == 0)
        p->vmfile.SeqBody = (RPC2_ByteSeq)malloc(1);
    else
        p->vmfile.SeqBody = (RPC2_ByteSeq)malloc(len);
    assert(p->vmfile.SeqBody); /* malloc failure is fatal */
    assert(len <= INT_MAX); /* LFS */
    p->vmfile.MaxSeqLen = len;
    p->vmfile.SeqLen    = len;
    p->vmfilep          = 0;
}

void sftp_FreePiggySDesc(struct SFTP_Entry *se)
{
    struct FileInfoByAddr *p;

    assert(se->PiggySDesc); /* better not be NULL! */
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

long SFTP_PrintSED(IN SDesc, IN outFile) SE_Descriptor *SDesc;
FILE *outFile;
{
    struct SFTP_Descriptor *sftpd;
    sftpd = &SDesc->Value.SmartFTPD;

    assert(SDesc->Tag == SMARTFTP); /* I shouldn't be called otherwise */

    switch (SDesc->LocalStatus) {
    case SE_NOTSTARTED:
        fprintf(outFile, "LocalStatus:    SE_NOTSTARTED    ");
        break;
    case SE_INPROGRESS:
        fprintf(outFile, "LocalStatus:    SE_INPROGRESS    ");
        break;
    case SE_SUCCESS:
        fprintf(outFile, "LocalStatus:    SE_SUCCESS    ");
        break;
    case SE_FAILURE:
        fprintf(outFile, "LocalStatus:    SE_FAILURE    ");
        break;
    }

    switch (SDesc->RemoteStatus) {
    case SE_NOTSTARTED:
        fprintf(outFile, "RemoteStatus:    SE_NOTSTARTED    ");
        break;
    case SE_INPROGRESS:
        fprintf(outFile, "RemoteStatus:    SE_INPROGRESS    ");
        break;
    case SE_SUCCESS:
        fprintf(outFile, "RemoteStatus:    SE_SUCCESS    ");
        break;
    case SE_FAILURE:
        fprintf(outFile, "RemoteStatus:    SE_FAILURE    ");
        break;
    }

    fprintf(outFile, "Tag:    SMARTFTP\n");
    fprintf(
        outFile,
        "TransmissionDirection:    %s    hashmark:    '%c'   "
        "SeekOffset:    %ld    BytesTransferred:    %ld    "
        "ByteQuota:    %ld    QuotaExceeded:    %ld\n",
        (sftpd->TransmissionDirection == CLIENTTOSERVER) ? "CLIENTTOSERVER" :
        (sftpd->TransmissionDirection == SERVERTOCLIENT) ? "SERVERTOCLIENT" :
                                                           "??????",
        sftpd->hashmark, sftpd->SeekOffset, sftpd->BytesTransferred,
        sftpd->ByteQuota, sftpd->QuotaExceeded);

    switch (sftpd->Tag) {
    case FILEBYNAME:
        fprintf(outFile,
                "Tag:    FILEBYNAME    ProtectionBits:    0%lo    "
                "LocalFileName:    \"%s\"\n",
                sftpd->FileInfo.ByName.ProtectionBits,
                sftpd->FileInfo.ByName.LocalFileName);
        break;

    case FILEBYINODE:
        fprintf(outFile,
                "Tag:    FILEBYINODE   Device:    %ld    Inode:    %ld\n",
                sftpd->FileInfo.ByInode.Device, sftpd->FileInfo.ByInode.Inode);
        break;

    case FILEBYFD:
        fprintf(outFile, "Tag:    FILEBYFD   fd:    %ld\n",
                sftpd->FileInfo.ByFD.fd);
        break;

    case FILEINVM:
        fprintf(
            outFile,
            "Tag:    FILEINVM   SeqBody:  %p    MaxSeqLen:    %u    SeqLen: %u\n",
            sftpd->FileInfo.ByAddr.vmfile.SeqBody,
            sftpd->FileInfo.ByAddr.vmfile.MaxSeqLen,
            sftpd->FileInfo.ByAddr.vmfile.SeqLen);
        break;

    default:
        fprintf(outFile, "Tag: ???????\n");
        break;
    }
    return 1;
}
