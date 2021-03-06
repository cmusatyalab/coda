/* BLURB lgpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <sys/file.h>

#include <rpc2/se.h>
#include <rpc2/secure.h>

#include "cbuf.h"
#include "codatunnel/wrapper.h"
#include "rpc2.private.h"
#include "trace.h"

/* free lists for different size packets. */
static struct rpc2_LinkEntry *rpc2_PBSmallFreeList, *rpc2_PBMediumFreeList,
    *rpc2_PBLargeFreeList;

RPC2_HostIdent rpc2_bindhost = {
    .Tag = RPC2_DUMMYHOST,
};

long RPC2_Init(
    const char *VId, /* magic version string */
    RPC2_Options *Options, RPC2_PortIdent *Port, /* array of portal ids */
    long RetryCount, /* max number of retries before breaking conn*/
    struct timeval *KAInterval /* for keeping long RPC requests alive  */
)
{
    char *c;
    long i;
    PROCESS ctpid;
    struct RPC2_addrinfo *rpc2_localaddrs;
    long rc1   = RPC2_NOCONNECTION, rc2, rc;
    short port = 0;
    int verbose;
    char *env;

    rpc2_logfile   = stderr;
    rpc2_tracefile = stderr;

    rpc2_Enter();
    say(1, RPC2_DebugLevel, "RPC2_Init()\n");
    say(999, RPC2_DebugLevel, "Runtime system version: \"%s\"\n", RPC2_VERSION);

    if (strcmp(VId, RPC2_VERSION) != 0) {
        say(-1, RPC2_DebugLevel, "RPC2_Init(): Wrong RPC2 version\n");
        rpc2_Quit(RPC2_WRONGVERSION);
    }

    /* rpc2_InitConn returns 0 if we're already initialized */
    if (rpc2_InitConn() == 0)
        rpc2_Quit(RPC2_SUCCESS);

    if (Options && (Options->Flags & RPC2_OPTION_IPV6))
        rpc2_ipv6ready = 1;

    env = getenv("RPC2SEC_KEYSIZE");
    if (env)
        RPC2_Preferred_Keysize = atoi(env);
    if (RPC2_Preferred_Keysize > 64)
        RPC2_Preferred_Keysize /= 8;

    /* Do we accept only secure connections, default is yes. This can be
     * disabled by setting the RPC2SEC_ONLY to 0, false, no, (nada, forgetit) */
    env              = getenv("RPC2SEC_ONLY");
    RPC2_secure_only = !env || (env && memchr("0fFnN", *env, 5) == NULL);

    verbose = (Options && (Options->Flags & RPC2_OPTION_VERBOSE_INIT));
    secure_init(verbose);

    rpc2_InitMgrp();
    rpc2_InitHost();

    rpc2_localaddrs = rpc2_resolve(&rpc2_bindhost, Port);

    if (!rpc2_localaddrs) {
        say(-1, RPC2_DebugLevel,
            "RPC2_Init(): Couldn't get addrinfo for localhost!\n");
        rpc2_Quit(RPC2_FAIL);
    }

#ifdef PF_INET6
    rc1 = rpc2_CreateIPSocket(PF_INET6, &rpc2_v6RequestSocket, rpc2_localaddrs,
                              &port);
#endif
    rc2 = rpc2_CreateIPSocket(PF_INET, &rpc2_v4RequestSocket, rpc2_localaddrs,
                              &port);

    RPC2_freeaddrinfo(rpc2_localaddrs);

    /* rc should probably be the most 'positive' result of the two */
    rc = (rc1 > rc2) ? rc1 : rc2;
    if (rc < RPC2_ELIMIT) {
        say(-1, RPC2_DebugLevel, "RPC2_Init(): Couldn't create socket\n");
        rpc2_Quit(rc);
    }

    rpc2_LocalPort.Tag                  = RPC2_PORTBYINETNUMBER;
    rpc2_LocalPort.Value.InetPortNumber = port;

    if (Port)
        *Port = rpc2_LocalPort;

    /* Initialize retry parameters */
    if (rpc2_InitRetry(RetryCount, KAInterval) != 0) {
        say(-1, RPC2_DebugLevel,
            "RPC2_Init(): Failed to init retryintervals\n");
        rpc2_Quit(RPC2_FAIL);
    }

    IOMGR_Initialize();
    TM_Init(&rpc2_TimerQueue);

    /* Register rpc2 packet handler with rpc2_SocketListener before
     * initializing the sideeffects */
    SL_RegisterHandler(RPC2_PROTOVERSION, rpc2_HandlePacket);

    /* Call side effect initialization routines */
    for (i = 0; i < SE_DefCount; i++)
        if (SE_DefSpecs[i].SE_Init != NULL)
            if ((*SE_DefSpecs[i].SE_Init)() < RPC2_ELIMIT) {
                say(-1, RPC2_DebugLevel, "RPC2_Init(): Failed to init SE\n");
                rpc2_Quit(RPC2_SEFAIL2);
            }

    c = "SocketListener";
    LWP_CreateProcess(rpc2_SocketListener, 32768, LWP_NORMAL_PRIORITY, NULL, c,
                      &rpc2_SocketListenerPID);

    c = "ClockTick";
    LWP_CreateProcess(rpc2_ClockTick, 16384, LWP_NORMAL_PRIORITY, NULL, c,
                      &ctpid);

    LUA_init();

    if (rc != RPC2_SUCCESS)
        say(-1, RPC2_DebugLevel, "RPC2_Init(): Exiting with error\n");

    rpc2_Quit(rc);
}

/* set the IP Addr to bind to */
struct in_addr RPC2_setip(struct in_addr *ip)
{
    RPC2_HostIdent host;

    host.Tag = RPC2_HOSTBYINETADDR;
    memcpy(&host.Value.InetAddress, ip, sizeof(struct in_addr));

    RPC2_setbindaddr(&host);
    return *ip;
}

void RPC2_setbindaddr(RPC2_HostIdent *host)
{
    if (rpc2_bindhost.Tag == RPC2_HOSTBYADDRINFO)
        RPC2_freeaddrinfo(rpc2_bindhost.Value.AddrInfo);

    rpc2_bindhost.Tag = RPC2_DUMMYHOST;

    if (!host)
        return;

    memcpy(&rpc2_bindhost, host, sizeof(RPC2_HostIdent));
    if (host->Tag == RPC2_HOSTBYADDRINFO)
        rpc2_bindhost.Value.AddrInfo = RPC2_copyaddrinfo(host->Value.AddrInfo);
}

long RPC2_Export(IN RPC2_SubsysIdent *Subsys)
{
    long i, myid = 0;
    struct SubsysEntry *ss;

    rpc2_Enter();
    say(1, RPC2_DebugLevel, "RPC2_Export()\n");

    switch (Subsys->Tag) {
    case RPC2_SUBSYSBYID:
        myid = Subsys->Value.SubsysId;
        break;

    case RPC2_SUBSYSBYNAME:
        say(1, RPC2_DebugLevel, "RPC2_Export: obsolete SUBSYSBYNAME used!\n");
        assert(0);
        break;

    default:
        rpc2_Quit(RPC2_FAIL);
    }

    /* Verify this subsystem not already exported */
    ss = rpc2_LE2SS(rpc2_SSList);
    for (i = 0; i < rpc2_SSCount; i++) {
        if (ss->Id == myid)
            rpc2_Quit(RPC2_DUPLICATESERVER);

        ss = rpc2_LE2SS(ss->LE.Next);
    }

    /* Mark this subsystem as exported */
    ss     = rpc2_AllocSubsys();
    ss->Id = myid;
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_DeExport(IN RPC2_SubsysIdent *Subsys)
{
    long i, myid = 0;
    struct SubsysEntry *ss;

    rpc2_Enter();
    say(1, RPC2_DebugLevel, "RPC2_DeExport()\n");

    if (Subsys == NULL) { /* Terminate all subsystems */
        rpc2_SSList  = NULL; /* possible core leak */
        rpc2_SSCount = 0;
        rpc2_Quit(RPC2_SUCCESS);
    }

    /* Else terminate a specific subsystem */
    switch (Subsys->Tag) {
    case RPC2_SUBSYSBYID:
        myid = Subsys->Value.SubsysId;
        break;

    case RPC2_SUBSYSBYNAME:
        say(1, RPC2_DebugLevel, "RPC2_Export: obsolete SUBSYSBYNAME used!\n");
        assert(0);

        break;

    default:
        rpc2_Quit(RPC2_BADSERVER);
    }

    /* Verify this subsystem is indeed exported */
    ss = rpc2_LE2SS(rpc2_SSList);
    for (i = 0; i < rpc2_SSCount; i++) {
        if (ss->Id == myid)
            break;

        ss = rpc2_LE2SS(ss->LE.Next);
    }
    if (i >= rpc2_SSCount)
        rpc2_Quit(RPC2_BADSERVER);

    rpc2_FreeSubsys(&ss);
    rpc2_Quit(RPC2_SUCCESS);
}

static RPC2_PacketBuffer *Gimme(long size, struct rpc2_LinkEntry **flist,
                                long *count, long *creacount)
{
    RPC2_PacketBuffer *pb;

    if (*flist == NULL) {
        rpc2_Replenish(flist, count, size, creacount, OBJ_PACKETBUFFER);
        assert(*flist);
        rpc2_LE2PB(*flist)->Prefix.BufferSize = size;
    }

    pb = rpc2_LE2PB(
        rpc2_MoveEntry(flist, &rpc2_PBList, NULL, count, &rpc2_PBCount));
    assert(pb->Prefix.LE.Queue == &rpc2_PBList);
    return pb;
}

static RPC2_PacketBuffer *GetPacket(long psize)
{
    if (psize <= SMALLPACKET) {
        return Gimme(SMALLPACKET, &rpc2_PBSmallFreeList, &rpc2_PBSmallFreeCount,
                     &rpc2_PBSmallCreationCount);
    }
    if (psize <= MEDIUMPACKET) {
        return Gimme(MEDIUMPACKET, &rpc2_PBMediumFreeList,
                     &rpc2_PBMediumFreeCount, &rpc2_PBMediumCreationCount);
    }
    if (psize <= LARGEPACKET) {
        return Gimme(LARGEPACKET, &rpc2_PBLargeFreeList, &rpc2_PBLargeFreeCount,
                     &rpc2_PBLargeCreationCount);
    }
    return NULL;
}

/* Allocates a packet buffer whose body is at least MinBodySize bytes,
   and sets BuffPtr to point to it.  Returns RPC2_SUCCESS on success.
   Sets BodyLength field of allocated packet to MinBodySize: you can
   alter this if this is not what you intended */
long rpc2_AllocBuffer(IN long MinBodySize, OUT RPC2_PacketBuffer **BuffPtr,
                      IN const char *File, IN long Line)
{
    long thissize;

    rpc2_Enter();
    thissize = MinBodySize + sizeof(RPC2_PacketBuffer);
    if (thissize > RPC2_MAXPACKETSIZE)
        return (0);

    *BuffPtr = GetPacket(thissize);
    assert(*BuffPtr);
    (*BuffPtr)->Prefix.sa = NULL;

    memset(&(*BuffPtr)->Header, 0, sizeof(struct RPC2_PacketHeader));
    (*BuffPtr)->Header.BodyLength = MinBodySize;

#ifdef RPC2DEBUG
    strncpy((char *)(*BuffPtr)->Prefix.File, File, 12);
    (*BuffPtr)->Prefix.File[2] &= 0xffffff00;
    (*BuffPtr)->Prefix.Line = Line;
#endif
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_FreeBuffer(INOUT RPC2_PacketBuffer **BuffPtr)
{
    struct rpc2_LinkEntry **tolist = NULL;
    long *tocount                  = NULL;

    rpc2_Enter();
    assert(BuffPtr);
    if (!*BuffPtr)
        return (RPC2_SUCCESS);

    assert((*BuffPtr)->Prefix.LE.MagicNumber == OBJ_PACKETBUFFER);

    if ((*BuffPtr)->Prefix.PeerAddr) {
        RPC2_freeaddrinfo((*BuffPtr)->Prefix.PeerAddr);
        (*BuffPtr)->Prefix.PeerAddr = NULL;
    }

    switch ((int)(*BuffPtr)->Prefix.BufferSize) {
    case SMALLPACKET:
        tolist  = &rpc2_PBSmallFreeList;
        tocount = &rpc2_PBSmallFreeCount;
        break;

    case MEDIUMPACKET:
        tolist  = &rpc2_PBMediumFreeList;
        tocount = &rpc2_PBMediumFreeCount;
        break;

    case LARGEPACKET:
        tolist  = &rpc2_PBLargeFreeList;
        tocount = &rpc2_PBLargeFreeCount;
        break;

    default:
        assert(FALSE);
    }
    assert((*BuffPtr)->Prefix.LE.Queue == &rpc2_PBList);
    rpc2_MoveEntry(&rpc2_PBList, tolist, &(*BuffPtr)->Prefix.LE, &rpc2_PBCount,
                   tocount);
    *BuffPtr = NULL;
    rpc2_Quit(RPC2_SUCCESS);
}

char *RPC2_ErrorMsg(long rc)
/* Returns a pointer to a static string describing error rc.  Note that this
   routine violates the RPC2 tradition of stuffing an OUT parameter.
*/
{
    static char msgbuf[100];

    switch ((int)rc) {
    case RPC2_SUCCESS:
        return ("RPC2_SUCCESS");

    case RPC2_OLDVERSION:
        return ("RPC2_OLDVERSION (W)");
    case RPC2_INVALIDOPCODE:
        return ("RPC2_INVALIDOPCODE (W)");
    case RPC2_BADDATA:
        return ("RPC2_BADDATA (W)");
    case RPC2_NOGREEDY:
        return ("RPC2_NOGREEDY (W)");
    case RPC2_ABANDONED:
        return ("RPC2_ABANDONED (W)");

    case RPC2_CONNBUSY:
        return ("RPC2_CONNBUSY (E)");
    case RPC2_SEFAIL1:
        return ("RPC2_SEFAIL1 (E)");
    case RPC2_TOOLONG:
        return ("RPC2_TOOLONG (E)");
    case RPC2_NOMGROUP:
        return ("RPC2_NOMGROUP (E)");
    case RPC2_MGRPBUSY:
        return ("RPC2_MGRPBUSY (E)");
    case RPC2_NOTGROUPMEMBER:
        return ("RPC2_NOTGROUPMEMBER (E)");
    case RPC2_DUPLICATEMEMBER:
        return ("RPC2_DUPLICATEMEMBER (E)");
    case RPC2_BADMGROUP:
        return ("RPC2_BADMGROUP (E)");

    case RPC2_FAIL:
        return ("RPC2_FAIL (F)");
    case RPC2_NOCONNECTION:
        return ("RPC2_NOCONNECTION (F)");
    case RPC2_TIMEOUT:
        return ("RPC2_TIMEOUT (F)");
    case RPC2_NOBINDING:
        return ("RPC2_NOBINDING (F)");
    case RPC2_DUPLICATESERVER:
        return ("RPC2_DUPLICATESERVER (F)");
    case RPC2_NOTWORKER:
        return ("RPC2_NOTWORKER (F)");
    case RPC2_NOTCLIENT:
        return ("RPC2_NOTCLIENT (F)");
    case RPC2_WRONGVERSION:
        return ("RPC2_WRONGVERSION (F)");
    case RPC2_NOTAUTHENTICATED:
        return ("RPC2_NOTAUTHENTICATED (F)");
    case RPC2_CLOSECONNECTION:
        return ("RPC2_CLOSECONNECTION (F)");
    case RPC2_BADFILTER:
        return ("RPC2_BADFILTER (F)");
    case RPC2_LWPNOTINIT:
        return ("RPC2_LWPNOTINIT (F)");
    case RPC2_BADSERVER:
        return ("RPC2_BADSERVER (F)");
    case RPC2_SEFAIL2:
        return ("RPC2_SEFAIL2 (F)");
    case RPC2_SEFAIL3:
        return ("RPC2_SEFAIL3 (F)");
    case RPC2_SEFAIL4:
        return ("RPC2_SEFAIL4 (F)");
    case RPC2_DEAD:
        return ("RPC2_DEAD (F)");
    case RPC2_NAKED:
        return ("RPC2_NAKED (F)");

    default:
        (void)sprintf(msgbuf, "Unknown RPC2 return code %ld", rc);
        return (msgbuf);
    }
}

long RPC2_GetPrivatePointer(IN RPC2_Handle ConnHandle, OUT char **PrivatePtr)
{
    struct CEntry *ceaddr;

    rpc2_Enter();

    say(999, RPC2_DebugLevel, "RPC2_GetPrivatePointer()\n");

    ceaddr = rpc2_GetConn(ConnHandle);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);

    *PrivatePtr = ceaddr->PrivatePtr;
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_SetPrivatePointer(IN RPC2_Handle ConnHandle, IN char *PrivatePtr)
{
    struct CEntry *ceaddr;

    rpc2_Enter();
    say(999, RPC2_DebugLevel, "RPC2_SetPrivatePointer()\n");

    ceaddr = rpc2_GetConn(ConnHandle);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);
    ceaddr->PrivatePtr = PrivatePtr;
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_GetSEPointer(IN RPC2_Handle ConnHandle, OUT struct SFTP_Entry **SEPtr)
{
    struct CEntry *ceaddr;

    rpc2_Enter();
    say(999, RPC2_DebugLevel, "RPC2_GetSEPointer()\n");

    ceaddr = rpc2_GetConn(ConnHandle);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);
    *SEPtr = (struct SFTP_Entry *)ceaddr->SideEffectPtr;
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_SetSEPointer(IN RPC2_Handle ConnHandle, IN struct SFTP_Entry *SEPtr)
{
    struct CEntry *ceaddr;

    rpc2_Enter();
    say(999, RPC2_DebugLevel, "RPC2_SetSEPointer()\n");

    ceaddr = rpc2_GetConn(ConnHandle);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);
    ceaddr->SideEffectPtr = (char *)SEPtr;
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_GetPeerInfo(IN RPC2_Handle ConnHandle, OUT RPC2_PeerInfo *PeerInfo)
{
    struct CEntry *ceaddr;

    rpc2_Enter();
    say(999, RPC2_DebugLevel, "RPC2_GetPeerInfo()\n");

    ceaddr = rpc2_GetConn(ConnHandle);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);

    rpc2_splitaddrinfo(&PeerInfo->RemoteHost, &PeerInfo->RemotePort,
                       ceaddr->HostInfo->Addr);
    PeerInfo->RemoteSubsys.Tag            = RPC2_SUBSYSBYID;
    PeerInfo->RemoteSubsys.Value.SubsysId = ceaddr->SubsysId;
    PeerInfo->RemoteHandle                = ceaddr->PeerHandle;
    PeerInfo->SecurityLevel               = ceaddr->SecurityLevel;
    PeerInfo->EncryptionType              = ceaddr->EncryptionType;
    memcpy(PeerInfo->SessionKey, ceaddr->SessionKey, RPC2_KEYSIZE);
    PeerInfo->Uniquefier = ceaddr->PeerUnique;
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_DumpTrace(IN FILE *OutFile, IN long HowMany)
{ /* NOTE: not surrounded by rpc2_Enter() and rpc2_Quit() */

#ifdef RPC2DEBUG
    if (OutFile == NULL)
        OutFile = stdout; /* it's ok, call-by-value */
    CBUF_WalkBuff(rpc2_TraceBuffHeader, rpc2_PrintTraceElem, HowMany, OutFile);
    (void)fflush(OutFile);
#endif
    return (RPC2_SUCCESS);
}

long RPC2_InitTraceBuffer(IN long ecount)
{ /* NOTE: not surrounded by rpc2_Enter() and rpc2_Quit() */

#ifdef RPC2DEBUG
    if (rpc2_TraceBuffHeader)
        CBUF_Free(&rpc2_TraceBuffHeader);
    rpc2_TraceBuffHeader =
        CBUF_Init(sizeof(struct TraceElem), ecount, "RPC2 Trace Buffer");
    assert(rpc2_TraceBuffHeader != NULL);
#endif
    return (RPC2_SUCCESS);
}

long RPC2_DumpState(FILE *DumpFile, long Verbosity /* > 0 ==> full dump */)
{ /* NOTE: not surrounded by rpc2_Enter() and rpc2_Quit() */

#ifdef RPC2DEBUG
    time_t when = rpc2_time();
    char where[100];

    if (DumpFile == NULL)
        DumpFile = stdout; /* it's ok, call-by-value */
    gethostname(where, sizeof(where));
    fprintf(DumpFile, "\n\n\t\t\tRPC2 Runtime State on %s at %s\n", where,
            ctime(&when));
    fprintf(DumpFile,
            "rpc2_ConnCreationCount = %ld  rpc2_ConnCount = %ld  "
            "rpc2_ConnFreeCount = %ld\n",
            rpc2_ConnCreationCount, rpc2_ConnCount, rpc2_ConnFreeCount);
    fprintf(
        DumpFile,
        "rpc2_PBCount = %ld  rpc2_PBHoldCount = %ld  rpc2_PBFreezeCount = %ld\n",
        rpc2_PBCount, rpc2_PBHoldCount, rpc2_PBFreezeCount);
    fprintf(DumpFile,
            "rpc2_PBSmallFreeCount = %ld  rpc2_PBSmallCreationCount = %ld\n",
            rpc2_PBSmallFreeCount, rpc2_PBSmallCreationCount);
    fprintf(DumpFile,
            "rpc2_PBMediumFreeCount = %ld  rpc2_PBMediumCreationCount = %ld\n",
            rpc2_PBMediumFreeCount, rpc2_PBMediumCreationCount);
    fprintf(DumpFile,
            "rpc2_PBLargeFreeCount = %ld  rpc2_PBLargeCreationCount = %ld\n",
            rpc2_PBLargeFreeCount, rpc2_PBLargeCreationCount);

    fprintf(DumpFile,
            "rpc2_SLCreationCount = %ld rpc2_SLFreeCount = %ld  "
            "rpc2_ReqCount = %ld  rpc2_SLCount = %ld\n",
            rpc2_SLCreationCount, rpc2_SLFreeCount, rpc2_SLReqCount,
            rpc2_SLCount);
    fprintf(DumpFile,
            "rpc2_SSCreationCount = %ld  rpc2_SSCount = %ld  "
            "rpc2_SSFreeCount = %ld\n",
            rpc2_SSCreationCount, rpc2_SSCount, rpc2_SSFreeCount);
#endif
    return (RPC2_SUCCESS);
}

long RPC2_LamportTime()
/* Returns the Lamport time for this system.
   This is at least one greater than the value returned on the preceding call.
   Accepted incoming packets with Lamport times greater than the local Lamport
   clock cause the local clock to be set to one greater than the incoming
   packet's time.
   Each non-retry outgoing packet gets a Lamport timestamp via this call.
   NOTE: the Lamport time bears no resemblance to the actual time of day.
   We could fix this.
*/
{
    rpc2_Enter();
    rpc2_LamportClock += 1;
    rpc2_Quit(rpc2_LamportClock);
}

long RPC2_SetBindLimit(IN int bindLimit)
{
    rpc2_Enter();
    rpc2_BindLimit = bindLimit;
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_Enable(RPC2_Handle whichConn)
{
    struct CEntry *ceaddr;

    say(1, RPC2_DebugLevel, "RPC2_Enable()\n");

    rpc2_Enter();
    ceaddr = rpc2_GetConn(whichConn);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);
    if (!TestState(ceaddr, SERVER, S_AWAITENABLE))
        rpc2_Quit(RPC2_FAIL);
    SetState(ceaddr, S_AWAITREQUEST);
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_SetColor(RPC2_Handle Conn, RPC2_Integer Color)
{
    struct CEntry *ceaddr;

    say(1, RPC2_DebugLevel, "RPC2_SetColor()\n");

    rpc2_Enter();
    ceaddr = rpc2_GetConn(Conn);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);
    ceaddr->Color = Color;
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_GetColor(RPC2_Handle Conn, RPC2_Integer *Color)
{
    struct CEntry *ceaddr;

    say(1, RPC2_DebugLevel, "RPC2_GetColor()\n");

    rpc2_Enter();
    ceaddr = rpc2_GetConn(Conn);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);
    *Color = ceaddr->Color;
    rpc2_Quit(RPC2_SUCCESS);
}

long RPC2_GetPeerLiveness(IN RPC2_Handle ConnHandle, OUT struct timeval *Time,
                          OUT struct timeval *SETime)
{
    struct CEntry *ceaddr;
    long rc = RPC2_SUCCESS;

    rpc2_Enter();
    say(999, RPC2_DebugLevel, "RPC2_GetPeerLiveness()\n");

    Time->tv_sec = Time->tv_usec = 0;
    SETime->tv_sec = SETime->tv_usec = 0;

    ceaddr = rpc2_GetConn(ConnHandle);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);

    /* get live time for RPC2 connection */
    if (ceaddr->HostInfo)
        *Time = ceaddr->HostInfo->LastWord; /* structure assignment */

    /* get live time for side effect, if any */
    if (ceaddr->SEProcs != NULL &&
        ceaddr->SEProcs->SE_GetSideEffectTime != NULL) {
        rc = (*ceaddr->SEProcs->SE_GetSideEffectTime)(ConnHandle, SETime);
    }

    rpc2_Quit(rc);
}

/*
 * returns the RPC and side effect network logs for the
 * peer of the connection Conn.  Note that the logs
 * contain information from all connections to that
 * peer, not just the connection Conn.
 *
 * CAVEAT: the side effect logs may not be returned if
 * the side effect data structures have not been linked
 * into the host/portal data structures used for tracking
 * network activity.  The side effect linkage occurs when
 * side effect parameters are exchanged.  In SFTP, this
 * exchange may _not_ occur until the first RPC that passes
 * a side effect descriptor!
 */

long RPC2_GetNetInfo(IN RPC2_Handle Conn, INOUT RPC2_NetLog *RPCLog,
                     INOUT RPC2_NetLog *SELog)
{
    struct CEntry *ceaddr;

    say(1, RPC2_DebugLevel, "RPC2_GetNetInfo()\n");

    rpc2_Enter();

    if (RPCLog == NULL && SELog == NULL)
        rpc2_Quit(RPC2_FAIL);

    if (SELog)
        SELog->ValidEntries = 0;
    if (RPCLog)
        RPCLog->ValidEntries = 0;

    ceaddr = rpc2_GetConn(Conn);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);

    /* first get the latency obs from the RPC layer */
    if (RPCLog && ceaddr->HostInfo)
        rpc2_GetHostLog(ceaddr->HostInfo, RPCLog, RPC2_MEASUREMENT);

    /* then get bandwidth obs from side effect layer */
    /* need a side effect call for getting the host info ptr */
    if (SELog && ceaddr->SEProcs != NULL &&
        ceaddr->SEProcs->SE_GetHostInfo != NULL) {
        struct HEntry *he;
        long rc;

        if ((rc = (*ceaddr->SEProcs->SE_GetHostInfo)(Conn, &he)) !=
            RPC2_SUCCESS)
            rpc2_Quit(rc);

        if (he)
            rpc2_GetHostLog(he, SELog, SE_MEASUREMENT);
    }

    rpc2_Quit(RPC2_SUCCESS);
}

/*
 * allows log entries to be added to the RPC or side effect
 * log for the peer of connection Conn.  This is useful
 * for depositing externally derived information about
 * conditions to a particular host.  The number of log
 * entries to be deposited is in NumEntries.  The number
 * of log entries actually deposted is returned in
 * ValidEntries.
 */
long RPC2_PutNetInfo(IN RPC2_Handle Conn, INOUT RPC2_NetLog *RPCLog,
                     INOUT RPC2_NetLog *SELog)
{
    struct CEntry *ceaddr;
    int i;

    say(1, RPC2_DebugLevel, "RPC2_PutNetInfo()\n");

    rpc2_Enter();

    if (RPCLog == NULL && SELog == NULL)
        rpc2_Quit(RPC2_FAIL);

    if (SELog)
        SELog->ValidEntries = 0;
    if (RPCLog)
        RPCLog->ValidEntries = 0;

    ceaddr = rpc2_GetConn(Conn);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);

    /* first the rpc layer */
    if (RPCLog && ceaddr->HostInfo)
        for (i = 0; i < RPCLog->NumEntries; i++) {
            if (!rpc2_AppendHostLog(ceaddr->HostInfo, &RPCLog->Entries[i],
                                    RPC2_MEASUREMENT))
                return (RPC2_FAIL);
            RPCLog->ValidEntries++;
        }

    /* side effect layer */
    if (SELog && ceaddr->SEProcs != NULL &&
        ceaddr->SEProcs->SE_GetHostInfo != NULL) {
        struct HEntry *he;
        long rc;

        if ((rc = (*ceaddr->SEProcs->SE_GetHostInfo)(Conn, &he)) !=
            RPC2_SUCCESS)
            rpc2_Quit(rc);

        if (he)
            for (i = 0; i < SELog->NumEntries; i++) {
                if (!rpc2_AppendHostLog(he, &SELog->Entries[i], SE_MEASUREMENT))
                    return (RPC2_FAIL);
                SELog->ValidEntries++;
            }
    }

    rpc2_Quit(RPC2_SUCCESS);
}

/*
 * clears the RPC and side effect network logs for the
 * peer of the connection Conn.
 */
long RPC2_ClearNetInfo(IN RPC2_Handle Conn)
{
    struct CEntry *ceaddr;

    rpc2_Enter();
    say(999, RPC2_DebugLevel, "RPC2_ClearNetInfo()\n");

    ceaddr = rpc2_GetConn(Conn);
    if (ceaddr == NULL)
        rpc2_Quit(RPC2_NOCONNECTION);

    /* get live time for RPC2 connection */
    if (ceaddr->HostInfo)
        rpc2_ClearHostLog(ceaddr->HostInfo, RPC2_MEASUREMENT);

    if (ceaddr->SEProcs != NULL && ceaddr->SEProcs->SE_GetHostInfo != NULL) {
        struct HEntry *he;
        long rc;

        if ((rc = (*ceaddr->SEProcs->SE_GetHostInfo)(Conn, &he)) !=
            RPC2_SUCCESS)
            rpc2_Quit(rc);

        if (he)
            rpc2_ClearHostLog(he, SE_MEASUREMENT);
    }

    rpc2_Quit(RPC2_SUCCESS);
}

/* adding this arg in theory allows me to specify whether to
   create a v4 or v6 socket.  Simpler way to do this? */
long rpc2_CreateIPSocket(int af, int *svar, struct RPC2_addrinfo *addr,
                         short *Port)
{
    union {
        struct sockaddr s;
        struct sockaddr_storage sstorage;
        struct sockaddr_in sin;
#if defined(PF_INET6)
        struct sockaddr_in6 sin6;
#endif
    } bindaddr;
    socklen_t blen;
    int err = RPC2_FAIL;
    int flags, rc;
    unsigned short port = 0, *sa_port;

    /* If codatunnel is enabled, it is responsible for binding.
     * just return the local socketpair endpoint. */
    *svar = codatunnel_socket();
    if (*svar != -1) {
        if (Port)
            *Port = 0;
        return RPC2_SUCCESS;
    }

    if (Port && *Port != 0)
        port = *Port;

    for (; addr; addr = addr->ai_next) {
        if (af != PF_UNSPEC && af != addr->ai_family)
            continue;

        switch (addr->ai_family) {
        case PF_INET:
            sa_port = &((struct sockaddr_in *)addr->ai_addr)->sin_port;
            break;
#if defined(PF_INET6)
        case PF_INET6:
            sa_port = &((struct sockaddr_in6 *)addr->ai_addr)->sin6_port;
            break;
#endif
        default:
            sa_port = NULL;
        }
        /* if the sockaddr doesn't have a port set, but we previously bound
         * successfully to a specific port (most likely with another protocol
         * or on another interface), then try to bind to the same port for this
         * address. If the bind fails then the OS probably maps 6to4. Or we are
         * colliding with some other application, but there is no way to tell
         * the difference. */
        if (sa_port && *sa_port == 0 && port != 0)
            *sa_port = port;

        /* Allocate socket */
        *svar = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (*svar < 0) {
            err = RPC2_FAIL;
            continue;
        }

        /* make sure the socket is non-blocking, corrupt udp checksums can
         * cause a packet drop by recvmsg and it would end up blocking
         * (possibly indefinitely). */
        flags = fcntl(*svar, F_GETFL, 0);
        fcntl(*svar, F_SETFL, flags | O_NONBLOCK);

        /* Now bind the socket */
        if (bind(*svar, addr->ai_addr, addr->ai_addrlen) < 0) {
            err = (errno == EADDRINUSE) ? RPC2_DUPLICATESERVER : RPC2_BADSERVER;
            close(*svar);
            *svar = -1;
            continue;
        }

        /* Retrieve fully resolved socket address so we can check which port we
         * actually got bound to */
        blen = sizeof(bindaddr);
        rc   = getsockname(*svar, (struct sockaddr *)&bindaddr, &blen);
        if (rc < 0) {
            err = RPC2_FAIL;
            close(*svar);
            *svar = -1;
            continue;
        }

        switch (addr->ai_family) {
        case PF_INET:
            port = bindaddr.sin.sin_port;
            break;
#if defined(PF_INET6)
        case PF_INET6:
            port = bindaddr.sin6.sin6_port;
            break;
#endif
        default:
            break;
        }
        if (Port)
            *Port = port;

        err = RPC2_SUCCESS;
        break;
    }
    return err;
}

unsigned int rpc2_MakeTimeStamp()
/* makes a longword time stamp in 1 msec units since rpc2_InitTime  */
{
    struct timeval now;
    unsigned int ts;

    /* use the approximate version b/c gettimeofday is called often */
    /* but for now we take the safe route */
    FT_GetTimeOfDay(&now, (struct timezone *)0);

    TVTOTS(&now, ts);

    return (ts);
}

/* Retransmission timer stuff */
void rpc2_UpdateRTT(RPC2_PacketBuffer *pb, struct CEntry *ceaddr)
{
    unsigned int obs;
    RPC2_NetLogEntry entry;

    if (!pb->Header.TimeStamp)
        return;

    TVTOTS(&pb->Prefix.RecvStamp, obs);
    say(15, RPC2_DebugLevel, "updatertt %u %u\n", obs, pb->Header.TimeStamp);
    obs = TSDELTA(obs, pb->Header.TimeStamp);
    RPC2_UpdateEstimates(ceaddr->HostInfo, obs, pb->Prefix.LengthOfPacket,
                         ceaddr->reqsize);

    /*
     * Requests can be sent and received in the same tick.
     * (though this is unlikely in the 1ms/tick case)
     * Adding in service time on the server complicates things --
     * the clock may tick on the server (service time > 0) but not
     * on the client. Coerce this case to 1.
     */
    if ((long)obs <= 0)
        obs = 1000;
    obs /= 1000;

    /* log the round-trip time observation in the host log */
    entry.Tag                  = RPC2_MEASURED_NLE;
    entry.Value.Measured.Bytes = ceaddr->reqsize + pb->Prefix.LengthOfPacket;
    entry.Value.Measured.ElapsedTime = obs;
    entry.Value.Measured.Conn        = ceaddr->UniqueCID;
    (void)rpc2_AppendHostLog(ceaddr->HostInfo, &entry, RPC2_MEASUREMENT);
}
