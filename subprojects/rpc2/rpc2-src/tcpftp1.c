/* BLURB lgpl

                            Coda File System
                               Release 8

           Copyright (c) 2026 Carnegie Mellon University
                   Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

#*/

/* TCPFTP side effect.
 *
 * Like SFTP, TCPFTP is a per-connection side effect: the app fills
 * SE_Descriptor.Value.TcpFTPD (an SFTP_Descriptor, identical in shape to the
 * SMARTFTP arm) and the base RPC2 machinery invokes the hooks below. The
 * difference from SMARTFTP is that no in-process read/write loop runs - the
 * local codatunneld does the I/O and ships the bytes to the peer's codatunneld
 * over the tunnel. The SE's only job is to register the local file with the
 * daemon (client in MakeRPC1, server in CheckSE), ferry the 8-byte correlation
 * cookie from client to server, and wait for the daemon's terminal status. That
 * cookie travels in the request via ftp_proto.c. */

#include <rpc2/rpc2.h>
#include <rpc2/tcpftp.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "rpc2.private.h"
#include "codatunnel.private.h" /* codatunnel_file_* control plane (internal) */

/* RPC2's per-connection SE-pointer slot is historically typed SFTP_Entry*, but
 * the value is an opaque char* (see RPC2_SetSEPointer). Cast through it. */
struct SFTP_Entry;

#define TCPFTP_MAGIC 0x54434650L /* "TCPF" */
#define TCPFTP_CLIENT 1
#define TCPFTP_SERVER 2

/* Role bits: mirror CT_SOURCE / CT_SINK from codatunnel/ctp.h. */
#define TCPFTP_ROLE_SOURCE 0
#define TCPFTP_ROLE_SINK 1

/* Per-connection SE state. */
struct TCPFTP_Entry {
    long Magic;
    long WhoAmI; /* TCPFTP_CLIENT or TCPFTP_SERVER */
    RPC2_PeerInfo PInfo; /* the tunnel peer (for the daemon register) */
    /* Cookie, sink spool fd, and got-block live in the shared offload state so
     * the file-registration helpers can be reused by the SFTP TCPFTP path. */
    struct TcpFtpState TcpFtp;
};

/*--------------------- Per-connection SE state ---------------------------*/

static struct TCPFTP_Entry *tcpftp_get(RPC2_Handle h)
{
    struct SFTP_Entry *raw;
    if (RPC2_GetSEPointer(h, &raw) != RPC2_SUCCESS)
        return NULL;
    return (struct TCPFTP_Entry *)raw;
}
static long tcpftp_set(RPC2_Handle h, struct TCPFTP_Entry *se)
{
    return RPC2_SetSEPointer(h, (struct SFTP_Entry *)se);
}
static struct TCPFTP_Entry *tcpftp_alloc(void)
{
    struct TCPFTP_Entry *se =
        (struct TCPFTP_Entry *)calloc(1, sizeof(struct TCPFTP_Entry));
    if (se != NULL) {
        se->Magic       = TCPFTP_MAGIC;
        se->TcpFtp.VmFd = -1; /* 0 is a live fd: keep the unbind guard safe */
    }
    return se;
}

/* The tunnel endpoint is the RPC2 connection's remote host. */
static const void *tcpftp_peer(const struct TCPFTP_Entry *se, socklen_t *len)
{
    if (se->PInfo.RemoteHost.Tag == RPC2_HOSTBYADDRINFO &&
        se->PInfo.RemoteHost.Value.AddrInfo != NULL &&
        se->PInfo.RemoteHost.Value.AddrInfo->ai_addr != NULL) {
        *len = (socklen_t)se->PInfo.RemoteHost.Value.AddrInfo->ai_addrlen;
        return se->PInfo.RemoteHost.Value.AddrInfo->ai_addr;
    }
    return NULL;
}

/* Grow a packet so `bytes` more can be appended. Mirrors sftp's
 * MakeBigEnough: never frees the old buffer (RPC2 may still hold a ref). */
static int tcpftp_makebigenough(RPC2_PacketBuffer **whichP, off_t bytes,
                                long maxSize)
{
    long freebytes, curlen;
    RPC2_PacketBuffer *pb;

    curlen =
        (*whichP)->Header.BodyLength + (long)sizeof(struct RPC2_PacketHeader);
    freebytes = (*whichP)->Prefix.BufferSize -
                (long)sizeof(struct RPC2_PacketBufferPrefix) - curlen;
    if (freebytes >= (long)bytes)
        return 0;
    if (curlen + (long)bytes > maxSize)
        return -1;

    assert(bytes <= INT_MAX); /* LFS */
    RPC2_AllocBuffer((bytes + (*whichP)->Header.BodyLength), &pb);
    memcpy(&pb->Header, &(*whichP)->Header, curlen);
    pb->Prefix.sa = (*whichP)->Prefix.sa;
    *whichP       = pb; /* DON'T free the old packet */
    return 0;
}

/* Append the (already-packed) param block to the tail of the request body. */
static int tcpftp_append_block(RPC2_PacketBuffer **whichP,
                               const unsigned char *blk, size_t blen)
{
    if (tcpftp_makebigenough(whichP, (off_t)blen, RPC2_MAXPACKETSIZE) < 0)
        return -1;

    /* SEDataOffset marks the start of the SE block; the block runs to the
     * end of the body. TCPFTP appends exactly one block per RPC and is the
     * only side effect on the connection, so setting it here is unambiguous. */
    (*whichP)->Header.SEDataOffset =
        (RPC2_Unsigned)(*whichP)->Header.BodyLength;
    memcpy((*whichP)->Body + (*whichP)->Header.BodyLength, blk, blen);
    (*whichP)->Header.BodyLength += blen;
    (*whichP)->Prefix.LengthOfPacket =
        (long)sizeof(struct RPC2_PacketHeader) + (*whichP)->Header.BodyLength;
    return 0;
}

/*--------------------- Hooks invoked by RPC2 ---------------------------*/

static long TCPFTP_Init(void)
{
    /* No global state and no custom packet type: the daemon owns the data. */
    return RPC2_SUCCESS;
}

static long TCPFTP_Bind1(RPC2_Handle ConnHandle, RPC2_CountedBS *ClientIdent)
{
    struct TCPFTP_Entry *se;
    (void)ClientIdent;
    se = tcpftp_alloc();
    if (se == NULL)
        return RPC2_SEFAIL1;
    se->WhoAmI = TCPFTP_CLIENT;
    tcpftp_set(ConnHandle, se);
    return RPC2_SUCCESS;
}

static long TCPFTP_Bind2(RPC2_Handle ConnHandle, RPC2_Unsigned BindTime)
{
    struct TCPFTP_Entry *se;
    (void)BindTime;
    se = tcpftp_get(ConnHandle);
    if (se == NULL)
        return RPC2_SEFAIL2;
    if (RPC2_GetPeerInfo(ConnHandle, &se->PInfo) != RPC2_SUCCESS)
        return RPC2_SEFAIL2;
    rpc2_simplifyHost(&se->PInfo.RemoteHost, &se->PInfo.RemotePort);
    return RPC2_SUCCESS;
}

static long TCPFTP_NewConn(RPC2_Handle ConnHandle, RPC2_CountedBS *ClientIdent)
{
    struct TCPFTP_Entry *se;
    (void)ClientIdent;
    se = tcpftp_alloc();
    if (se == NULL)
        return RPC2_SEFAIL1;
    se->WhoAmI = TCPFTP_SERVER;
    if (RPC2_GetPeerInfo(ConnHandle, &se->PInfo) != RPC2_SUCCESS) {
        free(se);
        return RPC2_SEFAIL2;
    }
    rpc2_simplifyHost(&se->PInfo.RemoteHost, &se->PInfo.RemotePort);
    tcpftp_set(ConnHandle, se);
    return RPC2_SUCCESS;
}

static long TCPFTP_Unbind(RPC2_Handle ConnHandle)
{
    struct TCPFTP_Entry *se = tcpftp_get(ConnHandle);
    if (se != NULL) {
        /* Release an in-flight daemon slot; a completed registration is a
         * no-op, so this is idempotent. */
        if (se->TcpFtp.Cookie != 0)
            codatunnel_file_unreg(se->TcpFtp.Cookie);
        if (se->TcpFtp.VmFd >= 0) {
            /* In-VM spool never finalized (aborted): dropping our fd lets the
             * daemon's close free the (already-unlinked) inode. */
            close(se->TcpFtp.VmFd);
            se->TcpFtp.VmFd = -1;
        }
        free(se);
    }
    return tcpftp_set(ConnHandle, NULL);
}

/* Open/resolve the local file for the descriptor's tag + role and register it
 * with the daemon as the source or sink of the cross-daemon transfer, cleaning
 * up whatever the SE itself opened once the daemon holds its own SCM_RIGHTS
 * copy of the fd:
 *   - FILEBYNAME: open(name, mode) -> SE owns; closed here.
 *   - FILEBYFD:   use d->FileInfo.ByFD.fd -> caller owns; never closed here.
 *   - FILEINVM source: materialize d->FileInfo.ByAddr.vmfile.SeqBody into an
 *                      unlinked temp spool (unlinked right after mkstemp, so no
 *                      path survives); the daemon preads it; our copy is closed
 *                      here.
 *   - FILEINVM sink: an empty unlinked temp spool the daemon fills; kept open
 *                      in st->VmFd so the finalize hook can read it back.
 *   - FILEBYINODE (or any other tag): rejected (RPC2_SEFAIL1).
 * in_cookie is 0 (client, generate) or the client's cookie (server, reuse);
 * *out_cookie receives the resulting cookie. Returns 0 or an RPC2_SEFAIL* code;
 * on failure any spool fd is closed (its file, already unlinked, is freed by
 * the kernel) and *out_cookie left 0. */
int tcpftp_register_local(struct TcpFtpState *st, const struct sockaddr *peer,
                          socklen_t plen, struct SFTP_Descriptor *d, int role,
                          uint64_t in_cookie, uint64_t *out_cookie)
{
    struct FileInfoByAddr *vm = &d->FileInfo.ByAddr;
    int fd, byfd, invm, ok;
    uint64_t reglen;

    st->VmFd    = -1;
    byfd        = (d->Tag == FILEBYFD);
    invm        = (d->Tag == FILEINVM);
    *out_cookie = 0;

    switch (d->Tag) {
    case FILEBYNAME:
        fd = (role == TCPFTP_ROLE_SOURCE) ?
                 open(d->FileInfo.ByName.LocalFileName, O_RDONLY) :
                 open(d->FileInfo.ByName.LocalFileName,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      (mode_t)d->FileInfo.ByName.ProtectionBits);
        break;
    case FILEBYFD:
        fd = (int)d->FileInfo.ByFD.fd;
        break;
    case FILEINVM: {
        char path[PATH_MAX];
        int pl = snprintf(path, sizeof(path), "%s/tcpftp-ftp.XXXXXX", P_tmpdir);
        uint64_t seq, off;
        if (pl < 0 || (size_t)pl >= sizeof(path))
            return RPC2_SEFAIL1;
        fd = mkstemp(path);
        if (fd < 0)
            return RPC2_SEFAIL1;
        /* Unlink immediately: both ends reach the data through their open fds
         * (ours + the daemon's SCM_RIGHTS copy), so no directory entry is ever
         * needed, and the inode is freed by the kernel when the last fd closes
         * - no deferred cleanup or stored path anywhere. */
        unlink(path);
        if (role == TCPFTP_ROLE_SOURCE) {
            /* Materialize the VM buffer into the spool so the daemon can
             * pread it. */
            const char *body = (const char *)vm->vmfile.SeqBody;
            seq              = vm->vmfile.SeqLen;
            off              = 0;
            while (off < seq) {
                ssize_t n = write(fd, body + off, (size_t)(seq - off));
                if (n <= 0) {
                    close(fd); /* spool is unlinked; the kernel frees it */
                    return RPC2_SEFAIL1;
                }
                off += (uint64_t)n;
            }
        }
        break;
    }
    default:
        return RPC2_SEFAIL1; /* FILEBYINODE (or anything else) is unsupported */
    }

    if (fd < 0)
        return RPC2_SEFAIL1;

    /* Register length: for a source it is the byte count to send - the file's
     * actual size (or the VM SeqLen) past the seek offset, capped at ByteQuota
     * (-1 == unlimited). Registering the true end lets the source pump stop on
     * a clean EOF instead of pread()-ing past the file and reporting IOERR. A
     * sink registers 0, telling the daemon to accept however many bytes it is
     * written (the local buffer / MaxSeqLen bounds it). */
    reglen = 0;
    if (role == TCPFTP_ROLE_SOURCE) {
        uint64_t size;
        if (invm) {
            size = (uint64_t)vm->vmfile.SeqLen;
        } else {
            struct stat sb;
            if (fstat(fd, &sb) != 0)
                return RPC2_SEFAIL1;
            size = (uint64_t)sb.st_size;
        }
        {
            uint64_t seek = (d->SeekOffset > 0) ? (uint64_t)d->SeekOffset : 0;
            reglen        = (size > seek) ? size - seek : 0;
            if (d->ByteQuota >= 0 && (uint64_t)d->ByteQuota < reglen)
                reglen = (uint64_t)d->ByteQuota;
        }
    }
    /* codatunnel_file_register takes the supplied cookie through *cookie:
     * non-zero means "reuse as-is and mark this the later end" (the server
     * passes the client's cookie back through in_cookie); zero means generate
     * a fresh one (the client). Seed it so in_cookie is honoured. */
    *out_cookie = in_cookie;
    ok = (codatunnel_file_register(peer, plen, fd, (uint64_t)d->SeekOffset,
                                   reglen, role, out_cookie) == 0);

    if (!ok)
        *out_cookie = 0;
    if (ok && invm && role == TCPFTP_ROLE_SINK) {
        st->VmFd = fd; /* held open; drained into the VM buffer at finalize */
    } else if (!byfd) {
        /* FILEBYNAME (ours) and FILEINVM: the daemon holds its own SCM_RIGHTS
         * copy, so release our fd. The spool needs no unlink - it was unlinked
         * at creation and the inode lives until the last fd closes. */
        close(fd);
    }
    return ok ? 0 : RPC2_SEFAIL1;
}

/* Read a populated in-VM sink spool back into d's buffer, capping at
 * MaxSeqLen and setting SeqLen. Returns 0, or RPC2_SEFAIL3 if the
 * received bytes exceed MaxSeqLen (the SFTP in-VM convention), or
 * RPC2_SEFAIL1 on a seek/read error.  */
static long tcpftp_invm_readback(struct SFTP_Descriptor *d, int fd)
{
    struct FileInfoByAddr *vm = &d->FileInfo.ByAddr;
    off_t sz                  = lseek(fd, 0, SEEK_END);

    vm->vmfile.SeqLen = 0;

    if (sz < 0 || lseek(fd, 0, SEEK_SET) < 0)
        return RPC2_SEFAIL1;

    if (sz > (off_t)vm->vmfile.MaxSeqLen)
        return RPC2_SEFAIL3; /* local buffer too small for the received file */

    if (sz > 0) {
        char *dst  = (char *)vm->vmfile.SeqBody;
        off_t done = 0;
        while (done < sz) {
            ssize_t n = read(fd, dst + done, (size_t)(sz - done));
            if (n <= 0)
                return RPC2_SEFAIL1;
            done += n;
        }
        vm->vmfile.SeqLen = (RPC2_Unsigned)done;
    }
    return 0;
}

/* Wait for the daemon's terminal status, then, for an in-VM sink, drain the
 * spool back into the local buffer. A failed transfer drops the spool without
 * publishing partial bytes. Returns the terminal status (0 == ok). */
long tcpftp_finalize(struct TcpFtpState *st, struct SFTP_Descriptor *d,
                     uint64_t cookie, int nowait)
{
    uint64_t nbytes = 0;
    int status =
        codatunnel_file_wait(cookie, 0, nowait, &nbytes); /* wait + release */
    d->BytesTransferred = (long)nbytes;

    if (st->VmFd >= 0) {
        int fd   = st->VmFd;
        st->VmFd = -1;
        if (status == 0) {
            status = (int)tcpftp_invm_readback(d, fd); /* seeks, reads */
        }

        /* Drop the spool (no partial bytes); the file is already unlinked,
         * so closing our fd lets the daemon's close free the inode. */
        close(fd);
    }
    return status;
}

/*
 * Client side. Resolve the local file (by tag/role) and register it with the
 * daemon, generating the correlation cookie; append the cookie block to the
 * request. Everything but the cookie is resolved locally, so it stays off the
 * wire. The daemon does the actual I/O.
 */
static long TCPFTP_MakeRPC1(RPC2_Handle ConnHandle, SE_Descriptor *SDesc,
                            RPC2_PacketBuffer **RequestPtr)
{
    struct TCPFTP_Entry *se;
    struct SFTP_Descriptor *d = &SDesc->Value.TcpFTPD;
    const void *peer;
    unsigned char blk[16];
    uint64_t cookie = 0;
    socklen_t plen;
    size_t blen = 0;
    int role;

    se = tcpftp_get(ConnHandle);
    if (se == NULL)
        return RPC2_SEFAIL2;
    if (se->WhoAmI != TCPFTP_CLIENT)
        return RPC2_SEFAIL2;

    peer = tcpftp_peer(se, &plen);
    if (peer == NULL)
        return RPC2_SEFAIL1;
    role = (d->TransmissionDirection == CLIENTTOSERVER) ? TCPFTP_ROLE_SOURCE :
                                                          TCPFTP_ROLE_SINK;
    if (tcpftp_register_local(&se->TcpFtp, (const struct sockaddr *)peer, plen,
                              d, role, 0, &cookie) != 0)
        return RPC2_SEFAIL1;
    se->TcpFtp.Cookie   = cookie;
    se->TcpFtp.GotBlock = 0;

    if (tcpftp_pack_param_block(&cookie, blk, sizeof(blk), &blen) != 0 ||
        tcpftp_append_block(RequestPtr, blk, blen) != 0) {
        codatunnel_file_unreg(cookie); /* releases the daemon's fd copy */
        if (se->TcpFtp.VmFd >= 0) {
            close(se->TcpFtp
                      .VmFd); /* spool unlinked at creation; kernel frees it */
            se->TcpFtp.VmFd = -1;
        }
        se->TcpFtp.Cookie = 0;
        return RPC2_SEFAIL4;
    }

    SDesc->LocalStatus = SE_SUCCESS; /* non-execution == success */
    return RPC2_SUCCESS;
}

/*
 * Client side. Wait for the daemon's terminal status, then release the slot.
 */
static long TCPFTP_MakeRPC2(RPC2_Handle ConnHandle, SE_Descriptor *SDesc,
                            RPC2_PacketBuffer *Reply)
{
    struct TCPFTP_Entry *se;
    struct SFTP_Descriptor *d = &SDesc->Value.TcpFTPD;
    long status;
    (void)Reply;

    se = tcpftp_get(ConnHandle);
    if (se == NULL)
        return RPC2_SEFAIL2;
    if (se->WhoAmI != TCPFTP_CLIENT)
        return RPC2_SEFAIL2;

    SDesc->LocalStatus  = SE_SUCCESS;
    SDesc->RemoteStatus = SE_SUCCESS;
    if (se->TcpFtp.Cookie == 0)
        return RPC2_SUCCESS; /* nothing registered (unsupported form) */

    status             = tcpftp_finalize(&se->TcpFtp, d, se->TcpFtp.Cookie, 1);
    se->TcpFtp.Cookie  = 0;
    SDesc->LocalStatus = (status == 0) ? SE_SUCCESS : SE_FAILURE;
    return RPC2_SUCCESS;
}

/*
 * Server side. Pluck the client's param block off the request and stash it so
 * CheckSE can register the local (sink/source) file with the right cookie.
 */
static long TCPFTP_GetRequest(RPC2_Handle ConnHandle,
                              RPC2_PacketBuffer *Request)
{
    struct TCPFTP_Entry *se;
    size_t avail;

    se = tcpftp_get(ConnHandle);
    if (se == NULL)
        return RPC2_SEFAIL2;
    if (se->WhoAmI != TCPFTP_SERVER)
        return RPC2_SEFAIL2;

    se->TcpFtp.GotBlock = 0;
    se->TcpFtp.Cookie   = 0;
    if (Request->Header.SEDataOffset > Request->Header.BodyLength)
        return RPC2_SEFAIL2; /* malformed: would underflow avail below */
    avail = (size_t)Request->Header.BodyLength -
            (size_t)Request->Header.SEDataOffset;
    if (avail > 0 &&
        tcpftp_unpack_param_block(
            (const unsigned char *)&Request->Body[Request->Header.SEDataOffset],
            avail, &se->TcpFtp.Cookie) != 0)
        return RPC2_SEFAIL2;
    if (avail > 0)
        se->TcpFtp.GotBlock = 1;
    return RPC2_SUCCESS;
}

static long TCPFTP_InitSE(RPC2_Handle ConnHandle, SE_Descriptor *SDesc)
{
    struct TCPFTP_Entry *se = tcpftp_get(ConnHandle);
    if (se == NULL)
        return RPC2_SEFAIL2;
    if (se->WhoAmI != TCPFTP_SERVER)
        return RPC2_SEFAIL2;
    SDesc->LocalStatus  = SE_NOTSTARTED;
    SDesc->RemoteStatus = SE_NOTSTARTED;
    return RPC2_SUCCESS;
}

/*
 * Server side. Resolve the local (sink or source) file for this connection's
 * descriptor and register it with the daemon using the cookie the client
 * carried on the wire (so the two daemons pair on it), then wait for
 * completion and, for an in-VM sink, drain the spool back into the buffer.
 */
static long TCPFTP_CheckSE(RPC2_Handle ConnHandle, SE_Descriptor *SDesc,
                           long Flags)
{
    struct TCPFTP_Entry *se;
    struct SFTP_Descriptor *d = &SDesc->Value.TcpFTPD;
    const void *peer;
    uint64_t cookie;
    socklen_t plen;
    int role, status;
    long rc;

    if (Flags == 0)
        return RPC2_SUCCESS;
    se = tcpftp_get(ConnHandle);
    if (se == NULL)
        return RPC2_SEFAIL2;
    if (se->WhoAmI != TCPFTP_SERVER)
        return RPC2_SEFAIL2;

    if (se->TcpFtp.GotBlock == 0)
        return RPC2_SEFAIL1; /* no cookie arrived on the wire */

    peer = tcpftp_peer(se, &plen);
    if (peer == NULL)
        return RPC2_SEFAIL1;
    role   = (d->TransmissionDirection == CLIENTTOSERVER) ? TCPFTP_ROLE_SINK :
                                                            TCPFTP_ROLE_SOURCE;
    cookie = se->TcpFtp.Cookie;
    rc = tcpftp_register_local(&se->TcpFtp, (const struct sockaddr *)peer, plen,
                               d, role, cookie, &cookie);
    if (rc != 0)
        return rc; /* already an RPC2_SEFAIL* code */
    se->TcpFtp.Cookie = cookie;

    status              = tcpftp_finalize(&se->TcpFtp, d, cookie, 0);
    se->TcpFtp.Cookie   = 0;
    SDesc->LocalStatus  = (status == 0) ? SE_SUCCESS : SE_FAILURE;
    SDesc->RemoteStatus = SE_SUCCESS;
    return (status == 0) ? RPC2_SUCCESS : RPC2_SEFAIL1;
}

static long TCPFTP_SendResponse(RPC2_Handle ConnHandle,
                                RPC2_PacketBuffer **Reply)
{
    /* No SE data rides the reply: the bytes went daemon-to-daemon. */
    (void)ConnHandle;
    (void)Reply;
    return RPC2_SUCCESS;
}

static long TCPFTP_PrintSED(SE_Descriptor *SDesc, FILE *outFile)
{
    struct SFTP_Descriptor *d = &SDesc->Value.TcpFTPD;
    fprintf(outFile,
            "TCPFTP dir=%d tag=%d offset=%llu len=%llu local=%d remote=%d\n",
            (int)d->TransmissionDirection, (int)d->Tag,
            (unsigned long long)d->SeekOffset, (unsigned long long)d->ByteQuota,
            (int)SDesc->LocalStatus, (int)SDesc->RemoteStatus);
    return RPC2_SUCCESS;
}

static long TCPFTP_GetTime(RPC2_Handle ConnHandle, struct timeval *Time)
{
    (void)ConnHandle;
    (void)Time;
    return RPC2_SUCCESS;
}

static long TCPFTP_GetHostInfo(RPC2_Handle ConnHandle, struct HEntry **hPtr)
{
    (void)ConnHandle;
    (void)hPtr;
    return RPC2_SUCCESS;
}

/*--------------------- SE table registration ---------------------------*/

long TCPFTP_Activate(void)
{
    struct SE_Definition *sed;
    long size;

    /* No-op if we're already in the table (SFTP_Activate ran, so we're next). */
    if (SE_DefSpecs != NULL && SE_DefCount > 0 &&
        SE_DefSpecs[SE_DefCount - 1].SideEffectType == TCPFTP)
        return TCPFTP;

    SE_DefCount++;
    size = sizeof(struct SE_Definition) * SE_DefCount;
    if (SE_DefSpecs == NULL)
        SE_DefSpecs = (struct SE_Definition *)malloc(size);
    else
        SE_DefSpecs = (struct SE_Definition *)realloc(SE_DefSpecs, size);
    if (SE_DefSpecs == NULL)
        return RPC2_SEFAIL1;

    sed = &SE_DefSpecs[SE_DefCount - 1];
    memset(sed, 0, sizeof(*sed));
    sed->SideEffectType       = TCPFTP;
    sed->SE_Init              = TCPFTP_Init;
    sed->SE_Bind1             = TCPFTP_Bind1;
    sed->SE_Bind2             = TCPFTP_Bind2;
    sed->SE_Unbind            = TCPFTP_Unbind;
    sed->SE_NewConnection     = TCPFTP_NewConn;
    sed->SE_MakeRPC1          = TCPFTP_MakeRPC1;
    sed->SE_MakeRPC2          = TCPFTP_MakeRPC2;
    sed->SE_GetRequest        = TCPFTP_GetRequest;
    sed->SE_InitSideEffect    = TCPFTP_InitSE;
    sed->SE_CheckSideEffect   = TCPFTP_CheckSE;
    sed->SE_SendResponse      = TCPFTP_SendResponse;
    sed->SE_PrintSEDescriptor = TCPFTP_PrintSED;
    sed->SE_GetSideEffectTime = TCPFTP_GetTime;
    sed->SE_GetHostInfo       = TCPFTP_GetHostInfo;
    return TCPFTP;
}
