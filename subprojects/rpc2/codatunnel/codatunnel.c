/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 2017-2026 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <uv.h>

#include <lwp/lwp.h>
#include <rpc2/codatunnel.h>

#include "codatunneld.private.h"
#include "cookie.h"
#include "codatunnel.private.h"

/* global flag below controls whether codatunnel is used */
static int codatunnel_enable_codatunnel = 0; /* non zero to enable tunneling */

/* fd in parent of open hfsocket */
static int codatunnel_vside_sockfd = -1; /* v2t: venus to tunnel */

int codatunnel_fork(int argc, char **argv, const char *tcp_bindaddr,
                    const char *udp_bindaddr, const char *bind_service,
                    int onlytcp, const char *sslcertdir)
{
    /*
     Create the Coda tunnel process.  Returns 0 on success, -1 on error.
     Invoked before RPC2_Init() by the Coda client or server.

     tcp_bindaddr is the IP address to be used to bind the TCP listen
     socket to.  This parameter should be NULL on the client as it is
     is expected to initiate new TCP connections.  On the server this
     can be set to "" (empty string) to bind to the wildcard address.

     udp_bindaddr is the IP address to be used to bind the UDP listen
     socket for communicating with legacy clients and servers that are
     not using codatunnel.  This can be set to "" (empty string) to
     bind to the wildcard address.

     bind_service is the port number to use; on the Coda client, this
     may be the value of the variable "masquerade_port" or NULL to
     bind to any available port.  On Coda servers, this is usually
     specified as "codasrv" which is specified as an IANA reserved
     port number in /etc/services.

     onlytcp is a flag.  If non-zero it suppresses use of UDP as fallback.
  */
    int rc, sockfd[2];

    DEBUG("codatunnel_fork(\"%s:%s\", \"%s:%s\", %d)\n", tcp_bindaddr,
          bind_service, udp_bindaddr, bind_service, onlytcp);

    /* codatunnel is enabled when the daemon process is forked */
    codatunnel_enable_codatunnel = 1;

    /* Create socketpair for host-facing UDP communication */
    rc = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockfd);
    if (rc < 0) {
        perror("codatunnel_fork: socketpair() failed: ");
        return -1;
    }

    DEBUG("hfsocket_fdpair after socketpair() is: [%d, %d]\n", sockfd[0],
          sockfd[1]);
    codatunnel_vside_sockfd = sockfd[0];

    /* fork, and then invoke codatunneld */
    rc = fork();
    if (rc < 0) {
        perror("codatunnel_fork: fork() failed: ");
        return -1;
    }

    if (rc > 0) { /* I am the parent. */
        DEBUG("Parent: fork succeeded, child pid is %d\n", rc);
        close(sockfd[1]);
        return 0; /* this is the only success return */
    }

    /* If I get here, I must be the newborn child */
    DEBUG("Child: codatunneld fork succeeded\n");
    close(sockfd[0]); /* codatunnel_vside_sockfd */

    /* uv_setup_args()/uv_set_process_title() rewrite the original argv
     * buffer in place to set the process title, corrupting any string
     * argument that still points into it.  sslcertdir is the only value a
     * caller may pass as an argv string, so take a private copy before
     * touching the argv buffer. */
    if (sslcertdir)
        sslcertdir = strdup(sslcertdir);

    /* if possible, rename child's command line for "ps ax" */
    if (argc) {
        uv_setup_args(argc, argv);
        uv_set_process_title("codatunneld");
    }

    /* launch the tunnel and never return */
    codatunneld(sockfd[1], tcp_bindaddr, udp_bindaddr, bind_service, onlytcp,
                sslcertdir);
    __builtin_unreachable(); /* should never reach here */
}

int codatunnel_socket()
{
    return codatunnel_vside_sockfd; /* already created by socketpair () */
}

void codatunnel_init0(const struct sockaddr *addr, socklen_t addrlen,
                      const char *peername)
{
    if (codatunnel_enable_codatunnel) {
        if (!peername)
            peername = "";

        codatunnel_sendto(codatunnel_vside_sockfd, peername,
                          strlen(peername) + 1, CODATUNNEL_ISINIT0_HINT, addr,
                          addrlen);
    }
}

/* --- daemon-driven file-transfer control plane ----------------------- */

int codatunnel_enabled(void)
{
    return codatunnel_enable_codatunnel ? 1 : 0;
}

/* Shared wakeup event for codatunnel_file_wait(). LWP signals are
 * broadcast, so the FILE_DONE demux below signals this one address and
 * every process blocked on it wakes; each re-checks its own cookie in
 * the table, which is the source of truth for completion. */
static int file_wakeup;

/* Unique, nonzero cookie value. Collision with an already-allocated slot is
 * handled by the caller's retry around ct_cookie_add. */
static uint64_t next_cookie(void)
{
    static uint64_t seed;
    static int initialized;
    uint64_t c;

    if (!initialized) {
        uint32_t t  = (uint32_t)time(NULL);
        seed        = ((uint64_t)t << 32) | (uint64_t)getpid();
        initialized = 1;
    }
    do {
        seed += 0x9E3779B97F4A7C15ULL;
        c = seed;
    } while (c == 0);
    return c;
}

/* Send one control datagram (ctp_t header + body) to the local codatunneld
 * over the vside socket, exactly the framing codatunnel_sendto uses. */
static int vside_send_control(uint32_t opcode, const void *body, size_t bodylen,
                              int fd)
{
    ctp_t p;
    struct iovec iov[2];
    char cbuf[CMSG_SPACE(sizeof(int))];
    struct msghdr msg = {
        .msg_iov    = iov,
        .msg_iovlen = 2,
    };

    strcpy(p.magic, "magic01");
    memset(&p.addr, 0, sizeof(p.addr));
    p.addrlen  = 0;
    p.opcode   = opcode;
    p.is_retry = 0;
    p.msglen   = bodylen;

    iov[0].iov_base = &p;
    iov[0].iov_len  = sizeof(ctp_t);
    iov[1].iov_base = (void *)body;
    iov[1].iov_len  = bodylen;

    /* When fd >= 0 (CT_FILEREG): hand the file fd to the daemon in the SAME
     * SEQPACKET datagram as the FILEREG header, via SCM_RIGHTS. The daemon's
     * uv_pipe(ipc=1) harvests it with uv_accept while reading the FILEREG. */
    memset(cbuf, 0, sizeof(cbuf));
    msg.msg_control    = NULL;
    msg.msg_controllen = 0;
    if (fd >= 0) {
        struct cmsghdr *cm;
        msg.msg_control       = cbuf;
        msg.msg_controllen    = sizeof(cbuf);
        cm                    = CMSG_FIRSTHDR(&msg);
        cm->cmsg_level        = SOL_SOCKET;
        cm->cmsg_type         = SCM_RIGHTS;
        cm->cmsg_len          = CMSG_LEN(sizeof(int));
        *(int *)CMSG_DATA(cm) = fd;
    }

    return (sendmsg(codatunnel_vside_sockfd, &msg, 0) >= 0) ? 0 : -1;
}

int codatunnel_file_register(const struct sockaddr *peer, socklen_t addrlen,
                             int fd, uint64_t offset, uint64_t length, int role,
                             uint64_t *cookie)
{
    ct_filereg *reg;
    uint64_t in_cookie = (cookie && *cookie) ? *cookie : 0;
    uint64_t c;

    if (cookie)
        *cookie = 0;
    /* documented: *cookie is 0 on any failure path below */

    if (!codatunnel_enable_codatunnel || !peer || fd < 0 || !cookie)
        return -1;

    /* The cookie is the cross-daemon correlation key: the client pre-registers
     * with a fresh cookie and carries it in its RPC; the RPC-server side
     * (CheckSE) passes that same cookie back through *cookie. A non-zero
     * in-cookie is therefore both the "use this cookie as-is" instruction and
     * exactly the "I am the later end" marker. */
    if (in_cookie) {
        if (ct_cookie_add(in_cookie, NULL) != 0)
            return -1; /* supplied cookie already in use, or table full */
        c = in_cookie;
    } else {
        /* no supplied cookie: generate a fresh, unallocated one (0 reserved).
         * next_cookie() values are effectively unique, so a failed
         * ct_cookie_add() here means the fixed table is full; bound the retry
         * and fail instead of spinning the cooperative io thread forever
         * (which would wedge every RPC on the handle). */
        int tries;
        for (tries = 0; tries < 8; tries++) {
            c = next_cookie();
            if (ct_cookie_add(c, NULL) == 0)
                break;
        }
        if (tries >= 8) {
            DEBUG(" codatunnel_file_register: cookie table full\n");
            return -1;
        }
    }

    reg = (ct_filereg *)malloc(sizeof(ct_filereg));
    if (!reg) {
        ct_cookie_remove(c);
        return -1;
    }
    reg->cookie  = c;
    reg->role    = (uint32_t)role;
    reg->flags   = in_cookie ? CT_FILREG_DRIVER : 0;
    reg->peerlen = (uint16_t)addrlen;
    reg->offset  = offset;
    reg->length  = length;
    reg->pad     = 0;
    memcpy(&reg->peer, peer, addrlen);

    if (vside_send_control(CT_FILEREG, reg, sizeof(ct_filereg), fd) != 0) {
        ct_cookie_remove(c);
        free(reg);
        return -1;
    }
    free(reg);
    *cookie = c;
    return 0;
}

void codatunnel_file_unreg(uint64_t cookie)
{
    uint64_t c = cookie;

    if (!codatunnel_enable_codatunnel || cookie == 0)
        return;
    vside_send_control(CT_FILEUNREG, &c, sizeof(c), -1);
    ct_cookie_remove(cookie);
}

int codatunnel_file_wait(uint64_t cookie, int timeout_ticks, int nowait,
                         uint64_t *nbytes)
{
    uint32_t s   = -1;
    uint64_t got = 0;
    int status   = -1;

    /* The timeout is not re-implemented here; the SFTP side-effect already
     * runs under an RPC2 deadline, which bounds the real wait. */
    (void)timeout_ticks;
    if (!codatunnel_enable_codatunnel)
        return -1;
    /* file_wait and the FILE_DONE demux below run in the same cooperative
     * LWP context (the handle's io thread): there is no yield between the
     * table check and the block, so the shared signal cannot be lost in
     * that gap. Waking spuriously on another cookie's FILE_DONE is
     * harmless - the table check below decides for our cookie alone. */
    for (;;) {
        int r = ct_cookie_get(cookie, &s, &got);
        if (r == 0) {
            if (nbytes)
                *nbytes = got;
            TLOG("TCPFTP file_wait cookie=%lu RETURN status=%u nbytes=%lu\n",
                 (unsigned long)cookie, (unsigned)s, (unsigned long)got);
            status = s; /* done; status in SFTP space (0 == ok) */
            break;
        }
        if (r != CT_COOKIE_PENDING)
            break; /* registration removed while we waited */
        if (nowait) {
            /* No terminal status yet (the peer never started the pump, e.g.
             * it rejected the RPC before CheckSE): do not block the
             * cooperative io thread. Fall out with the not-done status. */
            TLOG("TCPFTP file_wait cookie=%lu INCOMPLETE (nowait)\n",
                 (unsigned long)cookie);
            break; /* status stays -1 (no terminal status) */
        }
        TLOG("TCPFTP file_wait cookie=%lu BLOCK\n", (unsigned long)cookie);
        LWP_WaitProcess(&file_wakeup);
    }
    codatunnel_file_unreg(cookie);
    return status;
}

ssize_t codatunnel_sendto(int sockfd, const void *buf, size_t len, int flags,
                          const struct sockaddr *addr, socklen_t addrlen)
{
    int rc;
    ctp_t p;
    struct iovec iov[2];
    struct msghdr msg = {
        .msg_iov    = iov,
        .msg_iovlen = 2,
    };

    if (!codatunnel_enable_codatunnel) {
        return sendto(sockfd, buf, len, flags & ~CODATUNNEL_HINTS, addr,
                      addrlen);
    }

    /* construct the codatunnel packet */
    strncpy(p.magic, "magic01", 8);
    memcpy(&p.addr, addr, addrlen);
    p.addrlen  = addrlen;
    p.is_retry = (flags & CODATUNNEL_ISRETRY_HINT) ? 1 : 0;
    p.opcode   = (flags & CODATUNNEL_ISINIT0_HINT) ? CT_INIT0 : CT_PKT;
    p.msglen   = len;

    iov[0].iov_base = &p;
    iov[0].iov_len  = sizeof(ctp_t);
    iov[1].iov_base = (void *)buf;
    iov[1].iov_len  = len;

    // DEBUG("sending packet to codatunneld size=%ld\n", sizeof(ctp_t) + len);

    /* then send it to codatunneld */
    rc = sendmsg(sockfd, &msg, 0);

    if (rc < 0)
        return rc;

    /* adjust returned value for size of packet header */
    rc -= sizeof(ctp_t);
    if (rc < 0) {
        errno = ENOBUFS;
        return -1;
    }
    return rc;
}

/* The vside is a socketpair; a 0-byte (EOF) read means the daemon closed its
 * end (clean shutdown, or a crash, which the kernel turns into an EOF). No
 * FILEDONE will arrive now, so release every blocked codatunnel_file_wait()
 * with a terminal status, otherwise those processes spin in LWP_WaitProcess
 * forever. The table is only touched from this one cooperative LWP context, so
 * no lock is needed; codatunnel_file_wait() removes its own cookie on wake. */
struct ct_failall_ctx {
    int any;
};

static void ct_fail_all_visitor(uint64_t cookie, void *waiter, void *arg)
{
    struct ct_failall_ctx *ctx = (struct ct_failall_ctx *)arg;

    (void)waiter;
    if (ct_cookie_deliver(cookie, CT_STATUS_TIMEOUT, 0) == 0)
        ctx->any = 1;
}

static void ct_fail_all_pending(void)
{
    struct ct_failall_ctx ctx = { 0 };

    TLOG("TCPFTP vside closed: failing all pending file registrations\n");
    ct_cookie_foreach(ct_fail_all_visitor, &ctx);
    if (ctx.any)
        LWP_SignalProcess(&file_wakeup);
}

ssize_t codatunnel_recvfrom(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *from, socklen_t *fromlen)
{
    int rc;
    ctp_t p;
    struct iovec iov[2];
    struct msghdr msg = {
        .msg_iov    = iov,
        .msg_iovlen = 2,
    };

    if (!codatunnel_enable_codatunnel) {
        return recvfrom(sockfd, buf, len, flags, from, fromlen);
    }

    iov[0].iov_base = &p;
    iov[0].iov_len  = sizeof(ctp_t);
    iov[1].iov_base = buf;
    iov[1].iov_len  = len;

    /* get the next datagram from codatunneld */
    rc = recvmsg(sockfd, &msg, 0);

    DEBUG("received packet from codatunneld size=%d\n", rc);

    if (rc < 0)
        return rc; /* error */

    if (rc == 0) {
        /* daemon gone (clean shutdown or crash): release any blocked
         * file_wait before RPC2's socketlistener tears the tunnel down. */
        ct_fail_all_pending();
        errno = EBADF;
        return -1;
    }

    /* make sure we received enough data to read the packet header */
    if (rc < (ssize_t)sizeof(ctp_t)) {
        DEBUG("did not receive enough for packet header\n");
        errno = EBADF;
        return -1;
    }

    DEBUG("is_retry = %u  opcode = %u  msglen = %u\n", p.is_retry, p.opcode,
          p.msglen);

    if (p.opcode == CT_FILEDONE) {
        /* Terminal state for a file registration pushed down by the
         * daemon; demux it into the cookie table and keep reading so the
         * SocketListener's cadence of "give me one RPC2 packet" is
         * unchanged. */
        if ((size_t)(rc - (ssize_t)sizeof(ctp_t)) >= sizeof(ct_filedone)) {
            ct_filedone fd;
            int d;
            memcpy(&fd, buf, sizeof(ct_filedone));
            d = ct_cookie_deliver(fd.cookie, fd.status, fd.nbytes);
            TLOG(
                "TCPFTP FILEDONE(demux) cookie=%lu status=%u nbytes=%lu "
                "deliver=%d\n",
                (unsigned long)fd.cookie, (unsigned)fd.status,
                (unsigned long)fd.nbytes, d);
            if (d == 0)
                LWP_SignalProcess(&file_wakeup);
        }
        // returning 0 will make it look like a short read
        // secure_recvfrom will drop the packet and sets errno to ENOMEM
        // rpc2_RecvPacket will go back to the socketlistener loop
        return 0;
    }

    /* CT_PKT (and any legacy word): the normal case; return the payload */

    /* were the buffers we passed large enough? */
    if ((msg.msg_flags & MSG_TRUNC) || (*fromlen < p.addrlen)) {
        errno = ENOSPC;
        return -1;
    }

    /* copy peer address */
    memcpy(from, &p.addr, p.addrlen);
    *fromlen = p.addrlen;

    rc -= sizeof(ctp_t);
    assert(rc >= 0);
    // assert(rc == p.msglen); /* I think this should hold true -JH */
    return rc;
}
