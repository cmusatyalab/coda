/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2026 Carnegie Mellon University
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

#include <stdio.h>
#include <rpc2/rpc2.h>
#include <stdint.h>

#ifndef _SE_
#define _SE_

#ifdef __cplusplus
extern "C" {
#endif

/* Types of side effects: use these in the RPC2_Bind() call and in filling SE
 * descriptors */
#define OMITSE 9999 /* in MultiRPC for omitting side effects on some conns */
#define SMARTFTP 1189
#define TCPFTP 31155

enum WhichWay
{
    CLIENTTOSERVER = 93,
    SERVERTOCLIENT = 87
};
enum FileInfoTag
{
    FILEBYNAME  = 33,
    FILEBYINODE = 58,
    FILEBYFD    = 67,
    FILEINVM    = 74
};

/* File-identity sub-descriptors. Shared by both the SMARTFTP and the TCPFTP
 * descriptors, which reuse SFTP's request / parameter plumbing for the
 * identity of the file to transfer. */
struct FileInfoByName {
    long ProtectionBits; /* Unix mode bits to be set for created files */
    char LocalFileName[256];
}; /* standard Unix open() */

struct FileInfoByInode {
    long Device; /* device on which file resides */
    long Inode; /* inode number of file (inode MUST exist already) */
}; /* ITC inode-open */

struct FileInfoByFD {
    long fd; /* fd of already-open file (not automatically closed!) */
}; /* user gives already-open file */

struct FileInfoByAddr {
    /* Describes buffer allocated by user in VM.
     *  When file used as source:
     *  - user sets vmfile.SeqLen to actual file length.
     *  - SFTP ignores vmfile.MaxSeqLen
     *  When used as sink:
     *  - user sets vmfile.MaxSeqLen
     *  - SFTP sets vmfile.SeqLen to length of received file.
     *  - SFTP returns RPC2_SEFAIL3 if file bigger than MaxSeqLen.
     */
    RPC2_BoundedBS vmfile;
    long vmfilep; /* for internal use by SFTP as file pointer */
}; /* file resides in VM */

struct SFTP_Descriptor {
    enum WhichWay TransmissionDirection; /* IN */
    char hashmark; /* IN: 0 for non-verbose transfer */
    long SeekOffset; /* IN: >= 0; position to seek to before first read/write */
    long BytesTransferred; /* OUT: value after RPC2_CheckSideEffect() meaningful */
    long ByteQuota;
    /* IN: maximum number of data bytes  to be sent or received.
     *  A value of -1 implies infinity.
     *  Transfer is terminated and QuotaExceeded set if this limit would be
     *  exceeded.
     *  EnforceQuota in SFTP_Initializer must be specified as 1 at RPC
     *  initialization for the quota enforcement to take place.
     *  NOTE: (2/6/1994, Satya) The semantics is being slightly changed here to
     *  support partial file transfer; it used to be the case that hitting
     *  ByteQuota was an error reported as RPC2_SEFAIL1. But no one seems to be
     *  relying on this, so changing the error return to a success return seems
     *  fair game.
     */
    long QuotaExceeded; /* OUT: set to 1 if transfer terminated due to ByteQuota
                            limit 0 otherwise */
    enum FileInfoTag Tag; /* IN */
    union {
        struct FileInfoByName ByName; /* if (Tag == FILEBYNAME) */
        struct FileInfoByInode ByInode; /* if (Tag == FILEBYINODE) */
        struct FileInfoByFD ByFD; /* if (Tag == FILEBYFD) */
        struct FileInfoByAddr ByAddr; /* if (Tag == FILEINVM) */
    } FileInfo; /* everything is IN */
};

/* TCPFTP streams its payload over the codatunnel
 * daemon-to-daemon channel; unlike SMARTFTP, no data loop runs in the RPC2
 * process. It shares the very same file SE descriptor shape as SMARTFTP - the
 * SFTP_Descriptor above. The per-transfer correlation cookie and the terminal
 * daemon status are SE-internal bookkeeping held in the per-connection TCPFTP
 * entry (tcpftp1.c), not part of the descriptor: the wire carries only the
 * 8-byte cookie (ftp_proto.c), and each end resolves file identity, tag,
 * direction, offset and length from its own SFTP_Descriptor. */

enum SE_Status
{
    SE_NOTSTARTED = 33,
    SE_INPROGRESS = 24,
    SE_SUCCESS    = 57,
    SE_FAILURE    = 36
};

typedef struct SE_SideEffectDescriptor {
    enum SE_Status LocalStatus;
    enum SE_Status RemoteStatus;
    long Tag; /* only SMARTFTP, TCPFTP or OMITSE */
    union {
        /* nothing for OMITSE */
        struct SFTP_Descriptor SmartFTPD;
        struct SFTP_Descriptor TcpFTPD; /* same shape as SmartFTPD */
    } Value;

    /* this is a callback function, which is called whenever a block of
     * data is successfully transferred (sink side only for now). */
    void (*XferCB)(void *userp, unsigned int offset);
    void *userp;
} SE_Descriptor;

/* forward declarations so the prototypes below refer to file-scope structs,
 * not structs local to the parameter list */
struct HEntry;
struct SE_Definition {
    long SideEffectType; /* what kind of side effect am I? */
    long (*SE_Init)(void); /* on both client & server side */
    long (*SE_Bind1)(RPC2_Handle ConnHandle,
                     RPC2_CountedBS *ClientIdent); /* on client side */
    long (*SE_Bind2)(RPC2_Handle ConnHandle,
                     RPC2_Unsigned BindTime); /* on client side */
    long (*SE_Unbind)(RPC2_Handle ConnHandle); /* on client and server side */
    long (*SE_NewConnection)(RPC2_Handle ConnHandle,
                             RPC2_CountedBS *ClientIdent); /* on server side */
    long (*SE_MakeRPC1)(RPC2_Handle ConnHandle, SE_Descriptor *SDesc,
                        RPC2_PacketBuffer **RequestPtr); /* on client side */
    long (*SE_MakeRPC2)(RPC2_Handle ConnHandle, SE_Descriptor *SDesc,
                        RPC2_PacketBuffer *Reply); /* on client side */
    long (*SE_MultiRPC1)(int HowMany, RPC2_Handle ConnHandleList[],
                         SE_Descriptor SDescList[], RPC2_PacketBuffer *req[],
                         long retcode[]); /* on client side */
    long (*SE_MultiRPC2)(RPC2_Handle ConnHandle, SE_Descriptor *SDesc,
                         RPC2_PacketBuffer *Reply); /* on client side */
    long (*SE_CreateMgrp)(RPC2_Handle MgroupHandle); /* on client side */
    long (*SE_AddToMgrp)(RPC2_Handle MgroupHandle, RPC2_Handle ConnHandle,
                         RPC2_PacketBuffer **Request); /* on client side */
    long (*SE_InitMulticast)(RPC2_Handle MgroupHandle, RPC2_Handle ConnHandle,
                             RPC2_PacketBuffer *Request); /* on server side */
    long (*SE_DeleteMgrp)(RPC2_Handle MgroupHandle,
                          struct RPC2_addrinfo *ClientAddr,
                          long Role); /* on client and server side */
    long (*SE_GetRequest)(RPC2_Handle ConnHandle,
                          RPC2_PacketBuffer *Request); /* on server side */
    long (*SE_InitSideEffect)(RPC2_Handle ConnHandle,
                              SE_Descriptor *SDesc); /* on server side */
    long (*SE_CheckSideEffect)(RPC2_Handle ConnHandle, SE_Descriptor *SDesc,
                               long Flags); /* on server side */
    long (*SE_SendResponse)(RPC2_Handle ConnHandle,
                            RPC2_PacketBuffer **Reply); /* on server side */
    long (*SE_PrintSEDescriptor)(SE_Descriptor *SDesc,
                                 FILE *outFile); /* for debugging */
    long (*SE_SetDefaults)(void); /* for initialization */
    long (*SE_GetSideEffectTime)(RPC2_Handle ConnHandle, struct timeval *Time);
    long (*SE_GetHostInfo)(RPC2_Handle ConnHandle, struct HEntry **hPtr);
};

/* SE_common(): the transfer fields shared by both file SE modes live in the
 * SFTP_Descriptor, so a data site reads/writes them through one accessor
 * regardless of whether the connection is SMARTFTP or TCPFTP. Both union arms
 * are SFTP_Descriptor and alias the same bytes (offset 0), so the choice made
 * at bind time (sed.Tag) does not change the shape. */
static inline struct SFTP_Descriptor *SE_common(SE_Descriptor *s)
{
    if (s->Tag == TCPFTP)
        return &s->Value.TcpFTPD;
    return &s->Value.SmartFTPD;
}

typedef struct SFTPI {
    long PacketSize; /* bytes in data packet */
    long WindowSize; /* max number of outstanding unacknowledged packets */
    long RetryCount;
    long RetryInterval; /* in milliseconds */
    long SendAhead; /* number of packets to read and send ahead */
    long AckPoint; /* when to send ack */
    long EnforceQuota; /* 0 ==> don't */
    long DoPiggy; /* FALSE ==> don't piggyback small files */
    long DupThreshold; /* Duplicates allowed before spontaneous Ack is sent */
    long MaxPackets; /* Memory usage throttle; SFTP will not use more than
                        this many packets in total; -1 (default) says no limit;
                        Caveat user: packet starvation can cause mysterious
                        RPC2_SEFAIL2s */
    RPC2_PortIdent Port; /* initialization required on server side */
} SFTP_Initializer;

/*
Flag options in RPC2_CheckSEStatus(): OR these together as needed
*/
#define SE_AWAITLOCALSTATUS 1
#define SE_AWAITREMOTESTATUS 2

extern struct SE_Definition *SE_DefSpecs; /* array */
extern long SE_DefCount; /* how many are there? */
extern void SE_SetDefaults();
extern char *SE_ErrorMsg(long rc);

/*
  Statistics
*/

/* I'm not sure where these should go. -JJK */
struct sftpStats {
    unsigned long Total, /* Packets Sent (Received) */
        Starts, /* Starts Sent (Received) */
        Datas, /* Datas Sent (Received) */
        DataRetries, /* Data Retries Sent (Received) */
        Acks, /* Acks Sent (Received) */
        Naks, /* Naks Sent (Received) */
        Busies, /* Busies Sent (Received) */
        Bytes, /* Bytes Sent (Received) */
        Timeouts; /* Timeouts when Sending (Receiving) */
};

extern struct sftpStats sftp_Sent, sftp_MSent;
extern struct sftpStats sftp_Recvd, sftp_MRecvd;

#ifdef __cplusplus
}
#endif

#endif
