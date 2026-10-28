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
#include <rpc2/sftp.h> /* shared vfile piggyback primitives */
#include <rpc2/tcpftp.h>
#include <assert.h>
#include <errno.h>
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

/* codatunneld.private.h must precede rpc2.private.h: the latter defines a
 * FREE macro that would clobber the dest-state enumerator in the former. */
#include "codatunneld.private.h" /* TLOG (gated by CODATUNNEL_TLOG) */
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
    /* Piggyback state: the file may ride the RPC packets themselves instead of
     * the tunnel (small files). openfd/fd_offset follow the sftp_InitIO
     * convention for the vfile primitives (openfd == -1 when no fd is open);
     * PiggySDesc squirrels a file's bytes between GetRequest/CheckSE and
     * between CheckSE/SendResponse. PiggyNegotiated is the bind-time answer
     * to the TCPFTP_PIGGY_CAPABLE bit: only send piggy when the peer can
     * parse it. */
    long openfd;
    off_t fd_offset;
    SE_Descriptor *PiggySDesc;
    long PiggyNegotiated;
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
        se->openfd      = -1;
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
    {
        struct CEntry *ce   = rpc2_GetConn(ConnHandle);
        se->PiggyNegotiated = (ce != NULL && (ce->Flags & CE_TCPFTP_PIGGY)) !=
                              0;
    }
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
    {
        struct CEntry *ce   = rpc2_GetConn(ConnHandle);
        se->PiggyNegotiated = (ce != NULL && (ce->Flags & CE_TCPFTP_PIGGY)) !=
                              0;
    }
    tcpftp_set(ConnHandle, se);
    return RPC2_SUCCESS;
}

/* Defined below, with the other piggyback helpers. */
static void tcpftp_vfclose(struct TCPFTP_Entry *se);
static void tcpftp_free_piggy(struct TCPFTP_Entry *se);

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
        tcpftp_vfclose(se); /* a piggy path may have left an fd open */
        if (se->PiggySDesc != NULL)
            tcpftp_free_piggy(se);
        free(se);
    }
    return tcpftp_set(ConnHandle, NULL);
}

/* Compute the length a local registration reports to the daemon. For a source
 * it is the byte count to send - the file's actual size (or the VM SeqLen)
 * past the seek offset, capped at ByteQuota when positive. The <= 0 ==
 * unlimited convention matches classic SFTP's "> 0" quota tests, so an app
 * that leaves ByteQuota 0 (an unset SE) ships the whole file rather than zero
 * bytes. Registering the true end lets the source pump stop on a clean EOF
 * instead of pread()-ing past the file and reporting IOERR. A sink registers
 * 0, telling the daemon to accept however many bytes it is written (the local
 * buffer / MaxSeqLen bounds it). */
uint64_t tcpftp_reglen(const struct SFTP_Descriptor *d, int role, uint64_t size)
{
    uint64_t seek;
    uint64_t reglen;

    if (role != TCPFTP_ROLE_SOURCE)
        return 0;
    seek   = (d->SeekOffset > 0) ? (uint64_t)d->SeekOffset : 0;
    reglen = (size > seek) ? size - seek : 0;
    if (d->ByteQuota > 0 && (uint64_t)d->ByteQuota < reglen)
        reglen = (uint64_t)d->ByteQuota;
    return reglen;
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
    TLOG(
        "tcpftp_register_local tag=%d role=%s in_cookie=0x%llx seek=%lld "
        "quota=%ld\n",
        (int)d->Tag, role == TCPFTP_ROLE_SOURCE ? "source" : "sink",
        (unsigned long long)in_cookie, (long long)d->SeekOffset,
        (long)d->ByteQuota);

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
        if (pl < 0 || (size_t)pl >= sizeof(path)) {
            TLOG("tcpftp_register_local: tmpdir path too long\n");
            return RPC2_SEFAIL1;
        }
        fd = mkstemp(path);
        if (fd < 0) {
            TLOG("tcpftp_register_local: mkstemp failed: %s\n",
                 strerror(errno));
            return RPC2_SEFAIL1;
        }
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
                    TLOG(
                        "tcpftp_register_local: spool write failed at "
                        "%llu/%llu: %s\n",
                        (unsigned long long)off, (unsigned long long)seq,
                        strerror(errno));
                    close(fd); /* spool is unlinked; the kernel frees it */
                    return RPC2_SEFAIL1;
                }
                off += (uint64_t)n;
            }
        }
        break;
    }
    default:
        TLOG("tcpftp_register_local: unsupported tag %d\n", (int)d->Tag);
        return RPC2_SEFAIL1; /* FILEBYINODE (or anything else) is unsupported */
    }

    if (fd < 0) {
        TLOG("tcpftp_register_local: open failed: %s\n", strerror(errno));
        return RPC2_SEFAIL1;
    }

    {
        uint64_t size;
        if (invm) {
            size = (uint64_t)vm->vmfile.SeqLen;
        } else {
            struct stat sb;
            if (fstat(fd, &sb) != 0) {
                TLOG("tcpftp_register_local: fstat failed: %s\n",
                     strerror(errno));
                return RPC2_SEFAIL1;
            }
            size = (uint64_t)sb.st_size;
        }
        reglen = tcpftp_reglen(d, role, size);
    }
    /* codatunnel_file_register takes the supplied cookie through *cookie:
     * non-zero means "reuse as-is and mark this the later end" (the server
     * passes the client's cookie back through in_cookie); zero means generate
     * a fresh one (the client). Seed it so in_cookie is honoured. */
    *out_cookie = in_cookie;
    ok = (codatunnel_file_register(peer, plen, fd, (uint64_t)d->SeekOffset,
                                   reglen, role, out_cookie) == 0);
    if (!ok) {
        TLOG(
            "tcpftp_register_local: codatunnel_file_register failed "
            "reglen=%llu seek=%llu in_cookie=0x%llx\n",
            (unsigned long long)reglen,
            (unsigned long long)(uint64_t)d->SeekOffset,
            (unsigned long long)in_cookie);
        *out_cookie = 0;
    } else {
        TLOG(
            "tcpftp_register_local: registered tag=%d role=%s reglen=%llu "
            "cookie=0x%llx\n",
            (int)d->Tag, role == TCPFTP_ROLE_SOURCE ? "source" : "sink",
            (unsigned long long)reglen, (unsigned long long)*out_cookie);
    }
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

/* Open/position the local file for d's tag + role so the vfile piggyback
 * primitives can move its bytes, mirroring sftp_InitIO but keyed on an
 * explicit role instead of an SFTP_Entry. On success se->openfd is open in
 * the correct mode and positioned at SeekOffset (se->openfd == -1 for
 * FILEINVM, whose position lives in vmfilep) and se->fd_offset tracks the
 * current position for FILEBYFD (a shared fd). Returns 0 on success, -1 on
 * failure; FILEBYINODE and other tags are unsupported, as in
 * tcpftp_register_local. */
static int tcpftp_initio(struct TCPFTP_Entry *se, struct SFTP_Descriptor *d,
                         int role)
{
    long oflags;

    if (d->Tag == FILEINVM) {
        se->openfd = -1; /* no descriptor really */
        if (d->SeekOffset > 0) {
            if (d->SeekOffset > d->FileInfo.ByAddr.vmfile.SeqLen)
                return (-1); /* bogus seek */
            d->FileInfo.ByAddr.vmfilep = d->SeekOffset;
        } else
            d->FileInfo.ByAddr.vmfilep = 0;
        if (role == TCPFTP_ROLE_SINK)
            d->FileInfo.ByAddr.vmfile.SeqLen = 0;
        return (0);
    }

    if (role == TCPFTP_ROLE_SINK) {
        oflags = O_WRONLY;
        if (d->SeekOffset <= 0)
            oflags |= O_TRUNC;
        if (d->Tag == FILEBYNAME)
            oflags |= O_CREAT;
    } else
        oflags = O_RDONLY;

    switch (d->Tag) {
    case FILEBYNAME:
        se->openfd    = open(d->FileInfo.ByName.LocalFileName, oflags,
                             (mode_t)d->FileInfo.ByName.ProtectionBits);
        se->fd_offset = 0;
        break;

    case FILEBYFD:
        /* trust the user to have given a good fd! dup it so the daemon (which
         * uses the original) and the piggyback path each own their copy */
        se->openfd    = dup(d->FileInfo.ByFD.fd);
        se->fd_offset = lseek(se->openfd, 0, SEEK_CUR);
        break;

    default:
        return (-1); /* FILEBYINODE (or anything else) is unsupported */
    }
    if (se->openfd < 0)
        return (-1);

    if ((int32_t)d->SeekOffset >= 0) {
        se->fd_offset = (off_t)d->SeekOffset;
        (void)lseek(se->openfd, se->fd_offset, SEEK_SET);
    }
    return (0);
}

/* Close whatever tcpftp_initio opened; safe to call when nothing is open. */
static void tcpftp_vfclose(struct TCPFTP_Entry *se)
{
    if (se->openfd >= 0)
        close(se->openfd);
    se->openfd    = -1;
    se->fd_offset = 0;
}

/* Squirrel a file's bytes in a detached in-VM descriptor (the same trick as
 * sftp_AllocPiggySDesc, keyed on the TcpFTPD arm, which is identical in
 * shape). len == 0 mallocs a 1-byte file since 0-length mallocs choke. */
static void tcpftp_alloc_piggy(struct TCPFTP_Entry *se, off_t len,
                               enum WhichWay direction)
{
    struct FileInfoByAddr *p;

    assert(se->PiggySDesc == NULL); /* can't already exist */
    se->PiggySDesc = (SE_Descriptor *)malloc(sizeof(SE_Descriptor));
    assert(se->PiggySDesc); /* malloc failure is fatal */
    memset(se->PiggySDesc, 0, sizeof(SE_Descriptor));
    se->PiggySDesc->Value.TcpFTPD.Tag                   = FILEINVM;
    se->PiggySDesc->Value.TcpFTPD.TransmissionDirection = direction;

    p = &se->PiggySDesc->Value.TcpFTPD.FileInfo.ByAddr;
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

static void tcpftp_free_piggy(struct TCPFTP_Entry *se)
{
    struct FileInfoByAddr *p;

    assert(se->PiggySDesc); /* better not be NULL! */
    p = &se->PiggySDesc->Value.TcpFTPD.FileInfo.ByAddr;
    assert(p->vmfile.SeqBody); /* better not be NULL! */
    free(p->vmfile.SeqBody);
    free(se->PiggySDesc);
    se->PiggySDesc = NULL;
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
    /* A source file may ride the request itself instead of the tunnel (push
     * piggy); a miss falls through to the registration below. The peer must
     * have negotiated the format (PiggyNegotiated): a pre-piggy peer treats
     * the body as an 8-byte cookie block and rejects it. */
    if (role == TCPFTP_ROLE_SOURCE && se->PiggyNegotiated &&
        d->Tag != FILEBYINODE) {
        off_t maxbytes;
        off_t len = -1;

        if (tcpftp_initio(se, d, role) == 0) {
            maxbytes = RPC2_MAXPACKETSIZE - sizeof(struct RPC2_PacketHeader) -
                       (*RequestPtr)->Header.BodyLength;
            len = sftp_vfile_piggy(d, se->openfd, &se->fd_offset, (long)role,
                                   RequestPtr, maxbytes, RPC2_MAXPACKETSIZE);
        }
        if (len == -1) {
            tcpftp_vfclose(se);
            return RPC2_SEFAIL4;
        }
        if (len == -2) {
            tcpftp_vfclose(se); /* too big: use the daemon */
        } else {
            se->TcpFtp.Cookie   = 0;
            se->TcpFtp.GotBlock = 0;
            SDesc->LocalStatus  = SE_SUCCESS;
            sftp_Progress(SDesc, len);
            tcpftp_vfclose(se);
            return RPC2_SUCCESS;
        }
    }

    if (role == TCPFTP_ROLE_SINK && se->PiggyNegotiated &&
        d->Tag != FILEBYINODE)
        /* Open the sink now, best effort, so MakeRPC2 can write a
         * piggybacked reply. */
        (void)tcpftp_initio(se, d, role);

    if (tcpftp_register_local(&se->TcpFtp, (const struct sockaddr *)peer, plen,
                              d, role, 0, &cookie) != 0) {
        tcpftp_vfclose(se);
        return RPC2_SEFAIL1;
    }
    se->TcpFtp.Cookie   = cookie;
    se->TcpFtp.GotBlock = 0;
    /* The daemon owns the I/O, so report per-chunk progress back through the
     * SE descriptor's progress callback, mirroring SFTP's XferCB. No-op when
     * the app left XferCB NULL. */
    codatunnel_file_progress(cookie, SDesc->XferCB, SDesc->userp,
                             (uint64_t)d->SeekOffset);

    if (tcpftp_pack_param_block(&cookie, blk, sizeof(blk), &blen) != 0 ||
        tcpftp_append_block(RequestPtr, blk, blen) != 0) {
        codatunnel_file_unreg(cookie); /* releases the daemon's fd copy */
        if (se->TcpFtp.VmFd >= 0) {
            close(se->TcpFtp
                      .VmFd); /* spool unlinked at creation; kernel frees it */
            se->TcpFtp.VmFd = -1;
        }
        tcpftp_vfclose(se);
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

    se = tcpftp_get(ConnHandle);
    if (se == NULL)
        return RPC2_SEFAIL2;
    if (se->WhoAmI != TCPFTP_CLIENT)
        return RPC2_SEFAIL2;

    SDesc->LocalStatus  = SE_SUCCESS;
    SDesc->RemoteStatus = SE_SUCCESS;
    if (se->TcpFtp.Cookie == 0) {
        tcpftp_vfclose(se); /* nothing registered (push piggy) */
        return RPC2_SUCCESS;
    }

    if (Reply != NULL && (Reply->Header.SEFlags & TCPFTP_PIGGY) &&
        d->TransmissionDirection == SERVERTOCLIENT) {
        /* Fetch piggy: the file rode the reply. Write it to the local sink
         * and release the daemon registration, which moved nothing. No
         * tcpftp_finalize: this SE handler runs on the io thread that
         * demuxes the daemon's CT_FILEDONE, so waiting would deadlock. */
        off_t len;
        long rc;

        codatunnel_file_unreg(se->TcpFtp.Cookie);
        se->TcpFtp.Cookie = 0;
        if (se->TcpFtp.VmFd >= 0) {
            close(se->TcpFtp.VmFd); /* empty spool; kernel frees it */
            se->TcpFtp.VmFd = -1;
        }
        if (Reply->Header.SEDataOffset > Reply->Header.BodyLength) {
            tcpftp_vfclose(se);
            SDesc->LocalStatus = SE_FAILURE;
            return RPC2_SEFAIL2; /* malformed: file region underflows */
        }
        len = Reply->Header.BodyLength - Reply->Header.SEDataOffset;
        if (se->openfd < 0) {
            /* MakeRPC1's best-effort open failed; the daemon's own open
             * (its registration) succeeded, so ours should too. */
            if (tcpftp_initio(se, d, TCPFTP_ROLE_SINK) != 0) {
                SDesc->LocalStatus = SE_FAILURE;
                return RPC2_SEFAIL4;
            }
        }
        rc = sftp_vfile_write(
            d, se->openfd, &se->fd_offset,
            (char *)&Reply->Body[Reply->Header.BodyLength - len], (int)len);
        tcpftp_vfclose(se);
        if (rc < 0) {
            SDesc->LocalStatus = SE_FAILURE;
            return rc; /* RPC2_SEFAIL3 or RPC2_SEFAIL4 */
        }
        Reply->Header.BodyLength = Reply->Header.SEDataOffset;
        sftp_Progress(SDesc, len);
        return RPC2_SUCCESS;
    }

    status            = tcpftp_finalize(&se->TcpFtp, d, se->TcpFtp.Cookie, 1);
    se->TcpFtp.Cookie = 0;
    tcpftp_vfclose(se); /* the daemon did the I/O; drop our best-effort fd */
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
    if (Request->Header.SEFlags & TCPFTP_PIGGY) {
        /* Push piggy: the client read its source file into the body region,
         * so the daemon is not involved at all. Squirrel the bytes for
         * CheckSE and shorten the body so the SE machinery sees nothing
         * left. A zero-length body is legitimate (an empty source file). */
        struct FileInfoByAddr *p;

        tcpftp_alloc_piggy(se, (off_t)avail, CLIENTTOSERVER);
        p = &se->PiggySDesc->Value.TcpFTPD.FileInfo.ByAddr;
        if (avail > 0)
            memcpy((char *)p->vmfile.SeqBody,
                   &Request->Body[Request->Header.SEDataOffset], avail);
        Request->Header.BodyLength = Request->Header.SEDataOffset;
        return RPC2_SUCCESS;
    }
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

    role = (d->TransmissionDirection == CLIENTTOSERVER) ? TCPFTP_ROLE_SINK :
                                                          TCPFTP_ROLE_SOURCE;

    if (se->TcpFtp.GotBlock == 0) {
        /* No cookie arrived: either the client pushed a piggybacked file
         * (GetRequest squirreled it away), or it sent nothing at all. */
        struct FileInfoByAddr *p;
        off_t filelen;
        int rc2;

        if (se->PiggySDesc == NULL)
            return RPC2_SEFAIL1; /* no cookie and no piggy: malformed */
        p       = &se->PiggySDesc->Value.TcpFTPD.FileInfo.ByAddr;
        filelen = p->vmfile.SeqLen;
        if (tcpftp_initio(se, d, role) != 0)
            rc2 = RPC2_SEFAIL1;
        else
            rc2 = sftp_vfile_write(d, se->openfd, &se->fd_offset,
                                   (char *)p->vmfile.SeqBody, (int)filelen);
        tcpftp_vfclose(se);
        tcpftp_free_piggy(se);
        SDesc->RemoteStatus = SE_SUCCESS;
        if (rc2 < 0) {
            SDesc->LocalStatus = SE_FAILURE;
            return rc2;
        }
        d->BytesTransferred = filelen;
        SDesc->LocalStatus  = SE_SUCCESS;
        return RPC2_SUCCESS;
    }

    /* Fetch piggy: the file may ride the RPC reply instead of the tunnel,
     * which skips both daemon registrations. The client must have negotiated
     * the format (PiggyNegotiated): a pre-piggy client cannot write a
     * piggybacked reply. Any miss falls through to the registration below,
     * which opens the file itself. */
    if (role == TCPFTP_ROLE_SOURCE && se->PiggyNegotiated &&
        d->Tag != FILEBYINODE && (Flags & SE_AWAITREMOTESTATUS) == 0) {
        off_t size, filelen, maxbytes;
        int rc2;

        if (tcpftp_initio(se, d, role) == 0) {
            size = sftp_vfile_size(d, se->openfd);
            if (size >= 0) {
                filelen = (off_t)tcpftp_reglen(d, role, (uint64_t)size);
                maxbytes =
                    RPC2_MAXPACKETSIZE - sizeof(struct RPC2_PacketHeader);
                if (filelen <= maxbytes) {
                    struct FileInfoByAddr *bp;

                    tcpftp_alloc_piggy(se, filelen, SERVERTOCLIENT);
                    bp  = &se->PiggySDesc->Value.TcpFTPD.FileInfo.ByAddr;
                    rc2 = sftp_vfile_read(d, se->openfd, &se->fd_offset,
                                          (char *)bp->vmfile.SeqBody, filelen);
                    if (rc2 < 0) {
                        tcpftp_free_piggy(se);
                        tcpftp_vfclose(se);
                    } else {
                        sftp_Progress(SDesc, filelen);
                        SDesc->LocalStatus  = SE_SUCCESS;
                        SDesc->RemoteStatus = SE_SUCCESS;
                        se->TcpFtp.Cookie   = 0; /* nothing registered */
                        tcpftp_vfclose(se);
                        return RPC2_SUCCESS;
                    }
                }
            }
        }
        tcpftp_vfclose(se); /* miss: the daemon opens its own fd */
    }

    peer = tcpftp_peer(se, &plen);
    if (peer == NULL)
        return RPC2_SEFAIL1;
    cookie = se->TcpFtp.Cookie;
    rc = tcpftp_register_local(&se->TcpFtp, (const struct sockaddr *)peer, plen,
                               d, role, cookie, &cookie);
    if (rc != 0)
        return rc; /* already an RPC2_SEFAIL* code */
    se->TcpFtp.Cookie = cookie;
    /* Per-chunk progress for the daemon-driven transfer, as above. */
    codatunnel_file_progress(cookie, SDesc->XferCB, SDesc->userp,
                             (uint64_t)d->SeekOffset);

    status              = tcpftp_finalize(&se->TcpFtp, d, cookie, 0);
    se->TcpFtp.Cookie   = 0;
    SDesc->LocalStatus  = (status == 0) ? SE_SUCCESS : SE_FAILURE;
    SDesc->RemoteStatus = SE_SUCCESS;
    return (status == 0) ? RPC2_SUCCESS : RPC2_SEFAIL1;
}

static long TCPFTP_SendResponse(RPC2_Handle ConnHandle,
                                RPC2_PacketBuffer **Reply)
{
    struct TCPFTP_Entry *se;

    se = tcpftp_get(ConnHandle);
    if (se == NULL)
        return RPC2_SEFAIL2;
    if (se->WhoAmI != TCPFTP_SERVER)
        return RPC2_SEFAIL2;
    if (se->PiggySDesc) {
        if (se->PiggySDesc->Value.TcpFTPD.TransmissionDirection ==
            SERVERTOCLIENT) {
            /* Fetch piggy: append the squirreled bytes to the reply.
             * sftp_AddPiggy sets the TCPFTP_PIGGY bit (== SFTP_PIGGY) and
             * the SEDataOffset itself; there is no daemon fallback, so a
             * packet that is too small is a plain failure. */
            if (sftp_AddPiggy(
                    Reply,
                    (char *)se->PiggySDesc->Value.TcpFTPD.FileInfo.ByAddr.vmfile
                        .SeqBody,
                    se->PiggySDesc->Value.TcpFTPD.FileInfo.ByAddr.vmfile.SeqLen,
                    RPC2_MAXPACKETSIZE) != 0)
                return RPC2_SEFAIL4;
        }
        tcpftp_free_piggy(se); /* always get rid of the file; covers
                                 CLIENTTOSERVER leftovers from aborted RPCs */
    }
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
