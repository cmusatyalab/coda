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

/*
   Daemon that does the relaying of packets to/from net and localhost.
   Created via fork() by Venus or codasrv.

   Uses single Unix domain socket to talk to Venus or codasrv on
   localhost, and one TCP-tunneled socket (with TLS end-to-end
   security) to talk to each distinct remote Coda server or client.
   Also has one UDP socket for backward compatibility with legacy
   servers and clients.

   This code layers UDP socket primitives on top of TCP connections.
   Maintains a single TCP connection for each (host, port) pair.
   All UDP packets to/from that (host, port) pair are sent/recvd on this
   connection.
   All RPC2 connections to/from that (host, port) pair are multiplexed
   on this connection.
   Minimal changes to rest of the RPC2 code.
   Discards all packets with "RETRY" bit set.  (Is this still true? Satya 12/23/2019)

   Possible negative consequences:
   (a) serializes all transmissions to each (host,port) pair
       (but no guarantee that such serialization wasn't happening before)
   (b) SFTP becomes a stop and wait protocol for each 8-packet window
       (since RETRY flag triggered sendahead)

   (Satya, 2017-01-04)

   Encapsulation rules: Is ctp_t packet present as prefix to UDP packet?
   (1) Venus/CodaSrv to/from codatunnel daemon:  yes; ctp_t fields in host order
   (2) codatunnel daemon to/from network via udpsocket:  no
   (3) codatunnel daemon to/from network via tcpsocket: yes; ctp_t fields in
   network order

   The addition of TLS security uses the uv layer as the transport for the
   push and pull functions of the TLS engine. Use of TLS is not an option.
   It is implemented on all the TCP connections.  So the only two choices are
   TLS-encapsulated TCP tunnel  or legacy UDP.    (Satya 2019-12-23)
*/

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <uv.h>
#include <gnutls/gnutls.h>

#include "codatunneld.private.h"
#include "cookie.h"

/* Global variables within codatunnel daemon */
static int codatunnel_I_am_server = 0; /* only clients initiate;
                                          only servers accept */
static int codatunnel_onlytcp     = 0; /* whether to use UDP fallback;
                                      default is yes */

static uv_loop_t *codatunnel_main_loop;
static uv_pipe_t codatunnel; /* facing Venus or CodaSrv (ipc=1: SCM_RIGHTS) */
static uv_udp_t udpsocket; /* facing the network */
static uv_tcp_t tcplistener; /* facing the network, only on servers */

static uv_async_t async_forward;
static uv_mutex_t async_forward_mutex;

/* TCP keepalive on the tunnel connections. The idle period deliberately
 * matches Coda's existing ~60s RPC2 keepalive (the client's RPC2_timeout,
 * codasrv's "timeout"); the tunnel is a direct TCP/TLS connection and we do
 * not need to keep NAT/masquerade entries alive, so a 1s ping is
 * unnecessary. libuv leaves the probe cadence to the kernel (9 probes, 10s
 * apart on Linux), so a silently dead peer is found ~2.5 min after the last
 * traffic and the resulting channel teardown fails all in-flight transfers.
 */
#define CT_TCP_KEEPALIVE_IDLE 60 /* seconds of idleness before first probe */

/* File-transfer registration record. The app hands the daemon an already-open
 * fd (SCM_RIGHTS on the FILEREG datagram) and the daemon owns it until the
 * registration is released; one record per cookie lives in the shared cookie
 * table (cookie.c) so both daemons of one transfer track the same
 * registration.
 *
 * Lifetime is owned by exactly one thread and all free/close happens on the uv
 * loop (serialized with FILEUNREG so it cannot race the worker). refs>0 means
 * a source pump worker is currently operating the record and is itself
 * responsible for requesting the free; while that is the case an
 * app-initiated FILEUNREG only sets cancel.
 */
struct ct_filerec {
    int fd;
    uint32_t role; /* CT_SOURCE / CT_SINK */
    uint64_t offset;
    uint64_t next; /* expected offset of the next chunk */
    uint64_t length;
    struct sockaddr_storage peer;
    uint16_t peerlen;
    uv_mutex_t lock; /* guards refs / cancel below */
    int refs; /* in-flight operators (the source pump) */
    int cancel; /* app requested release while refs>0 */
    int is_driver; /* CT_FILREG_DRIVER: the push-driving end of a transfer */
};

static int ct_peer_matches_dest(const struct ct_filerec *rec, const dest_t *d)
{
    return rec->peerlen == d->destlen &&
           memcmp(&rec->peer, &d->destaddr, rec->peerlen) == 0;
}

/* Worker->loop handoff. The TLS peel-off worker reads inbound daemon->daemon
 * records but must not touch the cookie table (that lives on the event loop),
 * so it enqueues each one here and the loop runs it. This is the single
 * thread-crossing point in the data path, and it is what keeps all rec/cookie
 * access -- and the whole registration lifecycle -- serialized on the loop. */
typedef struct ct_async_req {
    struct ct_async_req *next;
    uint64_t cookie;
    int op; /* CT_ASYNC_* (see defines below) */
    dest_t *d; /* channel the record (or its tear-down) refers to */
    uint64_t offset; /* DATA: absolute file offset of the payload */
    uint32_t status; /* ERROR: SFTP status carried by the peer */
    char *data; /* DATA: the owner record buffer (freed in the loop) */
    uint32_t datalen;
} ct_async_req_t;
#define CT_ASYNC_READY 0
#define CT_ASYNC_DATA 1
#define CT_ASYNC_EOF 2
#define CT_ASYNC_ERROR 3
#define CT_ASYNC_REQUEST 4
/* fail all transfers riding on r->d (cookie unused) */
#define CT_ASYNC_FAILDEST 5

static uv_async_t file_async;
static uv_mutex_t file_async_mutex;
static ct_async_req_t *file_async_q; /* FIFO head, guarded by file_async_mutex */
static ct_async_req_t
    *file_async_tail; /* FIFO tail, NULL when the queue is empty */

/* directory containing CA and server certificates */
static const char *sslcert_dir;

/* Useful data structures for callbacks; these minicbs do little real work
 * and are mainly used to free malloc'ed data structures after transmission */
typedef struct minicb_udp_req {
    uv_udp_send_t req;
    struct minicb_udp_req *qnext;
    ctp_t ctp;
    uv_buf_t msg;
} minicb_udp_req_t; /* used to be udp_send_req_t */

/* The vside ("codatunnel") is a uv_pipe (ipc=1) rather than a uv_udp because
 * receiving the file fds the app hands off via SCM_RIGHTS requires uv_accept,
 * which only operates on UV_FILE-capable IPC pipes. This req mirrors
 * minicb_udp_req_t for the three daemon->app writes that cross the vside. */
typedef struct minicb_pipe_req {
    uv_write_t req;
    struct minicb_pipe_req *qnext;
    ctp_t ctp;
    uv_buf_t msg;
} minicb_pipe_req_t;

#define MTR_MAXBUFS 6
typedef struct minicb_tcp_req {
    uv_write_t req;
    dest_t *dest;
    struct minicb_tcp_req *qnext;
    uv_sem_t write_done;
    ssize_t write_status;
    uv_buf_t msg[MTR_MAXBUFS];
    unsigned int msglen;
} minicb_tcp_req_t; /* used to be tcp_send_req_t */

typedef struct send_to_tls_req {
    struct send_to_tls_req *qnext;
    uv_work_t req;
    dest_t *dest;
    uv_buf_t buf;
    size_t len;
    uv_async_t *done; /* optional; re-armed when this record hits TCP */
} send_to_tls_req_t;

/* forward refs for workhorse functions; many are cb functions */
static void recv_codatunnel_cb(uv_stream_t *, ssize_t, const uv_buf_t *);
static void send_to_udp_dest(ssize_t, const uv_buf_t *,
                             const struct sockaddr *saddr, socklen_t slen);
static void send_to_tcp_dest(dest_t *, ssize_t, const uv_buf_t *, uv_async_t *);
static void _send_to_tls_done(uv_work_t *req, int status);
static void try_creating_tcp_connection(dest_t *);
static void recv_tcp_cb(uv_stream_t *, ssize_t, const uv_buf_t *);
static void tcp_connect_cb(uv_connect_t *, int);
static void async_free_dest(dest_t *);
static void recv_udpsocket_cb(uv_udp_t *, ssize_t, const uv_buf_t *,
                              const struct sockaddr *, unsigned);
static void tcp_newconnection_cb(uv_stream_t *, int);
static void peeloff_and_decrypt(uv_work_t *w);
static void cleanup_work(uv_work_t *w, int status);

/* Global that holds TLS-related stuff such as where to find server
   certificates (on client) and private key (on server). gnutls.h
   defines this as a pointer to a private structure.  It is malloced
   and filled shortly after codatunneld() is forked.  It is then used
   in all the gnutls-related calls for handshake, etc. */
static gnutls_certificate_credentials_t x509_cred;
static uv_rwlock_t credential_load_lock; /* protect x509_creds during handshake */

/* Whether to verify identity of peer via X509 certificate
   Only the client side of RPC2 bothers to verify identify of server side
*/
typedef enum
{
    IGNORE,
    VERIFY
} peercheck_t;

#if GNUTLS_VERSION_NUMBER < 0x030500
typedef unsigned int gnutls_init_flags_t;
#endif

/* For use with uv_queue_work() in async TLS calls */
typedef struct {
    uv_work_t work;
    dest_t *d;
    gnutls_init_flags_t tlsflags;
    peercheck_t certverify;
} async_tls_parms_t;

static socklen_t sockaddr_len(const struct sockaddr *addr)
{
    if (addr->sa_family == AF_INET)
        return sizeof(struct sockaddr_in);

    if (addr->sa_family == AF_INET6)
        return sizeof(struct sockaddr_in6);

    return sizeof(struct sockaddr_storage);
}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    *buf = uv_buf_init(calloc(1, suggested_size), suggested_size);

    /* gracefully handle allocation failures on libuv < 1.10.0 */
    if (buf->base == NULL)
        buf->len = 0;
}

/* All the minicb()s are gathered here in one place */

static void minicb_udp(uv_udp_send_t *arg, int status)
/* used to be udp_sent_cb() */
{
    DEBUG("minicb_udp(%p, %p, %d)\n", arg, arg->data, status);
    DEBUG("arg.handle.send_queue_count = %lu\n", arg->handle->send_queue_count);
    free(arg->data);
    free(arg);
}

static void minicb_tcp(uv_write_t *arg, int status)
/* used to be tcp_send_req() */
{
    minicb_tcp_req_t *req = (minicb_tcp_req_t *)arg;

    DEBUG("minicb_tcp(%p, %p, %d)\n", arg, arg->data, status);

    req->write_status = status;
    uv_sem_post(&req->write_done);
}

/* completion for a daemon->app vside write; mirrors minicb_udp */
static void minicb_pipe(uv_write_t *arg, int status)
{
    DEBUG("minicb_pipe(%p, %p, %d)\n", arg, arg->data, status);
    free(arg->data);
    free(arg);
}

/* called when we're about to free dest_t, dequeue any pending writes */
void drain_outbound_queues(dest_t *d)
{
    send_to_tls_req_t *cur, *next = NULL;
    minicb_tcp_req_t *mtr;

    /* drain tls_send_queue (to send_to_tls_dest) */
    uv_mutex_lock(&d->tls_send_mutex);
    cur               = d->tls_send_queue;
    d->tls_send_queue = NULL;
    if (cur) { /* first entry was queued for a worker thread */
        next       = cur->qnext;
        cur->qnext = NULL;
        uv_cancel((uv_req_t *)&cur->req);
    }
    while (next != NULL) { /* the rest was not queued yet */
        cur        = next;
        next       = cur->qnext;
        cur->qnext = NULL;

        free(cur->buf.base);
        free(cur);
    }
    uv_mutex_unlock(&d->tls_send_mutex);

    /* drain outbound_queue (from send_to_tls_dest) */
    uv_mutex_lock(&d->outbound_mutex);
    while (d->outbound_queue) {
        mtr               = d->outbound_queue;
        d->outbound_queue = mtr->qnext;
        minicb_tcp(&mtr->req, -EPIPE);
    }
    uv_mutex_unlock(&d->outbound_mutex);
}

/* File-transfer control plane. The app hands the daemon an open fd for a
 * local file (CT_FILEREG, the fd riding in the same datagram via SCM_RIGHTS);
 * the daemon owns the fd until the registration is released and keeps one
 * record per cookie in the shared file table (cookie.c), keyed by the app's
 * cookie so both daemons of one transfer track the same registration.
 * Terminal status is pushed back over the vside as CT_FILEDONE; the status
 * field is in the SFTP error space (CT_STATUS_*, ctp.h). */

static uint32_t ct_errno_to_status(int err)
{
    if (err == ENOENT || err == ENOTDIR)
        return CT_STATUS_NOENT;
    if (err == EACCES || err == EPERM)
        return CT_STATUS_ACCES;
    if (err == ENOSPC || err == EDQUOT)
        return CT_STATUS_NOSPC;
    return CT_STATUS_IOERR;
}

/* Close callback for a heap accept handle (ct_accept_fd). */
static void ct_fdacc_close_cb(uv_handle_t *h)
{
    free(h);
}

/* Harvest the SCM_RIGHTS fd(s) pending on the vside pipe. The app hands each
 * file fd in the same SEQPACKET datagram as its FILEREG (one fd per
 * registration), so at most one UV_FILE is pending on a CT_FILEREG.
 * uv_accept moves the fd into a pipe handle; we dup it -- the record's
 * pread/pwrite and close own that dup -- and close the accept handle, which
 * releases libuv's copy of the original. Returns the dup'd fd, or -1 if none
 * could be harvested. Runs on the loop only. */
static int ct_accept_fd(uv_pipe_t *vside)
{
    int first = -1;

    while (uv_pipe_pending_count(vside) > 0) {
        if (uv_pipe_pending_type(vside) != UV_FILE)
            break; /* leave a non-file pending handle alone */
        uv_pipe_t *acc = malloc(sizeof(*acc));
        if (!acc) {
            ERROR("malloc() failed\n");
            break;
        }
        uv_pipe_init(codatunnel_main_loop, acc, 1);
        if (uv_accept((uv_stream_t *)vside, (uv_stream_t *)acc) != 0) {
            uv_close((uv_handle_t *)acc, ct_fdacc_close_cb);
            continue;
        }
        uv_os_fd_t f;
        uv_fileno((uv_handle_t *)acc, &f);
        int d = (int)dup((int)f);
        uv_close((uv_handle_t *)acc,
                 ct_fdacc_close_cb); /* closes f; d remains */
        if (d < 0)
            continue;
        if (first < 0)
            first = d;
        else
            close(d); /* unexpected extra fd; drop it */
    }
    return first;
}

/* Push a terminal status to the app over the vside (host byte order, the
 * same framing the vside socketpair already uses). The buffer is carried by
 * a minicb so libuv frees it when the send completes. */
static void ct_push_filedone(uint64_t cookie, uint32_t status, uint64_t nbytes)
{
    minicb_pipe_req_t *req;
    char *pkt;
    ctp_t *p;
    ct_filedone *fd;
    int rc;

    TLOG("TCPFTP FILEDONE cookie=%lu status=%u nbytes=%lu\n",
         (unsigned long)cookie, (unsigned)status, (unsigned long)nbytes);

    req = malloc(sizeof(*req));
    pkt = malloc(sizeof(ctp_t) + sizeof(ct_filedone));
    if (!req || !pkt) {
        ERROR("malloc() failed\n");
        free(req);
        free(pkt);
        return;
    }
    req->qnext = NULL;

    p  = (ctp_t *)pkt;
    fd = (ct_filedone *)(pkt + sizeof(ctp_t));
    strncpy(p->magic, "magic01", sizeof(p->magic));
    memset(&p->addr, 0, sizeof(p->addr));
    p->opcode   = CT_FILEDONE;
    p->is_retry = 0;
    p->msglen   = sizeof(ct_filedone);
    p->addrlen  = 0;
    fd->cookie  = cookie;
    fd->nbytes  = nbytes;
    fd->status  = status;
    fd->pad     = 0;

    req->msg      = uv_buf_init(pkt, sizeof(ctp_t) + sizeof(ct_filedone));
    req->req.data = pkt;
    rc = uv_write(&req->req, (uv_stream_t *)&codatunnel, &req->msg, 1,
                  minicb_pipe);
    if (rc) {
        ERROR("uv_write(): rc = %d\n", rc);
        minicb_pipe(&req->req, rc);
    }
}

/* ---- source pump + registration lifetime ------------------------------ */

/* The body of a daemon->daemon record travels over the TLS hop in network
 * byte order (the ctp_t header does its own htonl in send_to_tcp_dest).
 * glibc's per-arch ntohl/htonl only cover 32 bits; these handle the 8-byte
 * cookie/offset fields without relying on platform htonll/ntohll. */
static uint64_t ct_hton64(uint64_t v)
{
    uint32_t hi = htonl((uint32_t)(v >> 32));
    uint32_t lo = htonl((uint32_t)v);
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t ct_ntoh64(uint64_t v)
{
    uint32_t hi = ntohl((uint32_t)(v >> 32));
    uint32_t lo = ntohl((uint32_t)v);
    return ((uint64_t)hi << 32) | lo;
}

/* One source pump per in-flight transfer, driven entirely on the event loop.
 * It works one chunk at a time: it reads a chunk from rec->fd and fires it at
 * the channel with a completion hook (p->arm); when that chunk's bytes hit TCP
 * the send worker posts p->arm, and we read the next chunk. Nothing in the
 * pump ever blocks the loop, so the outbound-write path -- which is what
 * actually completes our own TLS sends -- stays live (a blocking wait would
 * deadlock the daemon: a gnutls send only returns once the loop has done the
 * TCP write, and the loop can't do the TCP write while it is in our wait). At
 * most one chunk is ever in flight, which is the transfer's backpressure. */
typedef struct ct_pump {
    uv_async_t arm; /* re-arm on each chunk's TCP completion */
    struct ct_filerec *rec; /* refs held for the whole run */
    dest_t *d; /* channel to send on */
    uint64_t cookie;
    uint64_t begin; /* rec->offset: where this transfer's bytes start */
    uint64_t off; /* next offset to read/emit */
    uint64_t length;
    int eof_sent; /* terminal EOF has been queued; waiting on its TCP completion */
    char *scratch; /* CT_CHUNKMAX read buffer */
} ct_pump_t;

static void ct_pump_arm_cb(uv_async_t *arm);
static void ct_pump_arm_close_cb(uv_handle_t *handle);
static void ct_pump_send_next(ct_pump_t *p);
static void ct_pump_finish(ct_pump_t *p, uint32_t status);

/* Push a filled-in req onto the FIFO and wake the loop. */
static void file_async_push(ct_async_req_t *r)
{
    uv_mutex_lock(&file_async_mutex);
    r->next = NULL;
    if (file_async_tail)
        file_async_tail->next = r;
    else
        file_async_q = r;
    file_async_tail = r;
    uv_mutex_unlock(&file_async_mutex);
    uv_async_send(&file_async);
}

/* Enqueue a small (payload-free) event for the loop. Thread-safe: it is
 * called from the TLS peel-off worker thread; the FIFO (guarded by
 * file_async_mutex) is the thread-crossing point, the uv_async the wake-up,
 * and file_async_cb on the loop drains it. */
static void file_async_enqueue(uint64_t cookie, int op, dest_t *d)
{
    ct_async_req_t *r = malloc(sizeof(*r));
    if (!r) {
        ERROR("malloc() failed\n");
        return;
    }
    memset(r, 0, sizeof(*r));
    r->cookie = cookie;
    r->op     = op;
    r->d      = d;
    file_async_push(r);
}

/* Enqueue a CT_TRANSFER_DATA record. recbuf (the owner) is handed to the loop,
 * which pwrites the payload out of it and frees it. */
static void file_async_enqueue_data(uint64_t cookie, uint64_t offset, dest_t *d,
                                    char *recbuf, uint32_t reclen)
{
    ct_async_req_t *r = malloc(sizeof(*r));
    if (!r) {
        ERROR("malloc() failed\n");
        free(recbuf);
        return;
    }
    memset(r, 0, sizeof(*r));
    r->cookie  = cookie;
    r->op      = CT_ASYNC_DATA;
    r->d       = d;
    r->offset  = offset;
    r->data    = recbuf;
    r->datalen = reclen;
    file_async_push(r);
}

/* Enqueue a CT_TRANSFER_ERROR carrying the peer's SFTP status. */
static void file_async_enqueue_error(uint64_t cookie, uint32_t status,
                                     dest_t *d)
{
    ct_async_req_t *r = malloc(sizeof(*r));
    if (!r) {
        ERROR("malloc() failed\n");
        return;
    }
    memset(r, 0, sizeof(*r));
    r->cookie = cookie;
    r->op     = CT_ASYNC_ERROR;
    r->d      = d;
    r->status = status;
    file_async_push(r);
}

/* Free a registration record. Runs on the uv loop only. If no worker is
 * operating the record (refs==0) it is torn down now; otherwise the worker
 * owns the free and will re-queue this through file_async when it finishes. */
static void ct_rec_free_now(uint64_t cookie)
{
    struct ct_filerec *rec = ct_cookie_waiter(cookie);
    if (!rec)
        return; /* already released */
    uv_mutex_lock(&rec->lock);
    if (rec->refs == 0) {
        ct_cookie_remove(cookie);
        close(rec->fd);
        uv_mutex_unlock(&rec->lock);
        uv_mutex_destroy(&rec->lock);
        free(rec);
    } else {
        uv_mutex_unlock(&rec->lock);
    }
}

/* Small daemon->daemon control packets (READY/EOF/ERROR), each its own ctp_t
 * record, sent over d fire-and-forget. Ordering is preserved by the
 * per-destination TLS send queue, so EOF always follows the last DATA. */
static void ct_send_small(dest_t *d, uint32_t opcode, const void *body,
                          size_t len, uv_async_t *done)
{
    size_t total = sizeof(ctp_t) + len;
    char *pkt    = malloc(total);
    if (!pkt) {
        ERROR("malloc() failed\n");
        return;
    }
    ctp_t *h = (ctp_t *)pkt;
    memset(pkt, 0, total);
    strncpy(h->magic, CT_MAGIC, sizeof(h->magic));
    h->opcode = opcode;
    h->msglen = (uint32_t)len;
    if (body)
        memcpy(pkt + sizeof(ctp_t), body, len);
    uv_buf_t buft = uv_buf_init(pkt, total);
    send_to_tcp_dest(d, total, &buft, done);
}

static void ct_send_ready(dest_t *d, uint64_t cookie)
{
    ct_transfer_ready t;
    t.cookie = ct_hton64(cookie);
    ct_send_small(d, CT_TRANSFER_READY, &t, sizeof(t), NULL);
}

static void ct_send_transfer_request(dest_t *d, uint64_t cookie,
                                     uint64_t offset, uint64_t len)
{
    ct_transfer_request t;
    t.cookie = ct_hton64(cookie);
    t.offset = ct_hton64(offset);
    t.len    = ct_hton64(len);
    ct_send_small(d, CT_TRANSFER_REQUEST, &t, sizeof(t), NULL);
}

static void ct_send_transfer_eof(dest_t *d, uint64_t cookie, uv_async_t *done)
{
    ct_transfer_eof t;
    t.cookie = ct_hton64(cookie);
    ct_send_small(d, CT_TRANSFER_EOF, &t, sizeof(t), done);
}

static void ct_send_transfer_error(dest_t *d, uint64_t cookie, uint32_t status)
{
    ct_transfer_error t;
    t.cookie = ct_hton64(cookie);
    t.status = htonl(status);
    ct_send_small(d, CT_TRANSFER_ERROR, &t, sizeof(t), NULL);
}

/* Read the next chunk off rec->fd and fire it at the channel. Runs on the loop.
 * On success the transport re-arms us through p->arm once the chunk's bytes hit
 * TCP, which pulls the next chunk; when the file is exhausted we finish. Exactly
 * one chunk is in flight at a time -- the transfer's backpressure -- because
 * the next chunk is only read after the previous one has landed. */
static void ct_pump_send_next(ct_pump_t *p)
{
    if (p->off >= p->length) {
        if (!p->eof_sent) {
            /* The EOF must be confirmed on TCP before we report success: the
             * app tears the channel down as soon as it sees FILEDONE, and an
             * EOF still queued in the send worker would be dropped with the
             * channel. Queue it with p->arm so the pump only finishes once the
             * EOF's bytes actually hit TCP. */
            p->eof_sent = 1;
            TLOG("TCPFTP PUMP EOF cookie=%lu total=%lu\n",
                 (unsigned long)p->cookie, (unsigned long)(p->off - p->begin));
            ct_send_transfer_eof(p->d, p->cookie, &p->arm);
        }
        return;
    }
    size_t want = (size_t)(p->length - p->off);
    /* cap to what this channel's negotiated TLS record max can carry in one
     * fragment (and to the scratch size): never split a record across records */
    size_t cap = p->d->max_data_payload;
    if (cap > CT_CHUNKMAX)
        cap = CT_CHUNKMAX;
    if (want > cap)
        want = cap;
    ssize_t n = pread(p->rec->fd, p->scratch, want, p->off);
    if (n <= 0) {
        uint32_t st = (n < 0) ? ct_errno_to_status(errno) : CT_STATUS_IOERR;
        if (p->d->state == TCPACTIVE)
            ct_send_transfer_error(p->d, p->cookie, st);
        ct_pump_finish(p, st);
        return;
    }
    size_t total = sizeof(ctp_t) + sizeof(ct_transfer_data) + (size_t)n;
    char *pkt    = malloc(total);
    if (!pkt) {
        if (p->d->state == TCPACTIVE)
            ct_send_transfer_error(p->d, p->cookie, CT_STATUS_IOERR);
        ct_pump_finish(p, CT_STATUS_IOERR);
        return;
    }
    ctp_t *h             = (ctp_t *)pkt;
    ct_transfer_data *td = (ct_transfer_data *)(pkt + sizeof(ctp_t));
    memset(pkt, 0, total);
    strncpy(h->magic, CT_MAGIC, sizeof(h->magic));
    h->opcode = CT_TRANSFER_DATA;
    h->msglen = (uint32_t)(sizeof(ct_transfer_data) + (uint32_t)n);
    memcpy(pkt + sizeof(ctp_t) + sizeof(ct_transfer_data), p->scratch, n);
    td->cookie = ct_hton64(p->cookie);
    td->offset = ct_hton64(p->off);
    p->off += (uint64_t)n;
    p->rec->next  = p->off;
    uv_buf_t buft = uv_buf_init(pkt, total);
    TLOG("TCPFTP PUMP DATA cookie=%lu off=%lu len=%lu\n",
         (unsigned long)p->cookie, (unsigned long)(p->off - (uint64_t)n),
         (unsigned long)n);
    /* transport owns the buffer; p->arm re-arms the pump when this hits TCP */
    send_to_tcp_dest(p->d, total, &buft, &p->arm);
}

/* A chunk landed on TCP: the send worker posted p->arm (thread-safe
 * uv_async_send while the loop drains it). Runs on the loop. Cancel / dropped
 * channel are checked here; otherwise the next chunk goes out. */
static void ct_pump_arm_cb(uv_async_t *arm)
{
    ct_pump_t *p = (ct_pump_t *)arm->data;
    if (p->rec->cancel || p->d->state != TCPACTIVE) {
        if (p->d->state == TCPACTIVE)
            ct_send_transfer_error(p->d, p->cookie, CT_STATUS_TIMEOUT);
        ct_pump_finish(p, CT_STATUS_TIMEOUT);
        return;
    }
    /* The send that re-armed us is on TCP. If that was the terminal EOF
     * (everything emitted), the transfer is truly complete: report success now
     * that the EOF is guaranteed to reach the peer before any teardown. */
    if (p->off >= p->length && p->eof_sent) {
        ct_pump_finish(p, CT_STATUS_SUCCESS);
        return;
    }
    ct_pump_send_next(p);
}

/* Terminal state on every exit path: report to the app, hand the record back
 * (refs->0, freed loop-side so it cannot race FILEUNREG), and tear down the
 * pump (its struct is released when the arm handle closes). */
static void ct_pump_finish(ct_pump_t *p, uint32_t status)
{
    TLOG(
        "TCPFTP PUMP FINISH cookie=%lu status=%u off=%lu len=%lu eof_sent=%d\n",
        (unsigned long)p->cookie, status, (unsigned long)p->off,
        (unsigned long)p->length, p->eof_sent);
    ct_push_filedone(p->cookie, status, p->off - p->begin);
    free(p->scratch);
    p->rec->refs = 0; /* loop-side; the pump was the last operator */
    ct_rec_free_now(p->cookie);
    uv_close((uv_handle_t *)&p->arm, ct_pump_arm_close_cb);
}

static void ct_pump_arm_close_cb(uv_handle_t *handle)
{
    ct_pump_t *p = (ct_pump_t *)handle->data;
    free(p);
}

/* Start a source pump. Runs on the uv loop only: FILEREG starts the "driver"
 * end here (send_ready=1, it is the side that tells the peer to start), and
 * ct_transfer_ready_arrived starts the "earlier" end (send_ready=0). The pump
 * is allocated, its re-arm handle opened, the first chunk is emitted, and the
 * completion loop (ct_pump_arm_cb) then carries the rest. */
static void ct_pump_do_start(uint64_t cookie, int send_ready)
{
    struct ct_filerec *rec = ct_cookie_waiter(cookie);
    if (!rec || rec->role != CT_SOURCE)
        return; /* already released, or not a source on this side */
    if (rec->refs > 0)
        return; /* already streaming */
    rec->refs   = 1; /* the pump owns this record until it finishes */
    rec->cancel = 0;

    dest_t *d = getdest(&rec->peer, rec->peerlen);
    if (!d || d->state != TCPACTIVE) {
        DEBUG("pump start: channel for cookie %lu not active\n",
              (unsigned long)cookie);
        rec->refs = 0;
        ct_push_filedone(cookie, CT_STATUS_IOERR, 0);
        ct_rec_free_now(cookie);
        return;
    }
    ct_pump_t *p = malloc(sizeof(*p));
    if (!p) {
        rec->refs = 0;
        ct_push_filedone(cookie, CT_STATUS_IOERR, 0);
        ct_rec_free_now(cookie);
        return;
    }
    p->rec      = rec;
    p->d        = d;
    p->cookie   = cookie;
    p->begin    = rec->offset;
    p->off      = rec->offset;
    p->length   = rec->length;
    p->eof_sent = 0;
    p->scratch  = malloc(CT_CHUNKMAX);
    if (!p->scratch) {
        free(p);
        rec->refs = 0;
        ct_push_filedone(cookie, CT_STATUS_IOERR, 0);
        ct_rec_free_now(cookie);
        return;
    }
    p->arm.data = p;
    uv_async_init(codatunnel_main_loop, &p->arm, ct_pump_arm_cb);
    TLOG("TCPFTP PUMP START cookie=%lu off=%lu len=%lu ready=%d\n",
         (unsigned long)cookie, (unsigned long)p->begin,
         (unsigned long)p->length, send_ready);
    if (send_ready)
        ct_send_ready(d, cookie);
    /* chunk 0, or an immediate EOF/finish if the file is already empty */
    ct_pump_send_next(p);
}

/* Inbound CT_TRANSFER_READY from the peer. Runs on the uv loop so it never
 * races the cookie table. The "earlier" SOURCE is now a passive responder to
 * CT_TRANSFER_REQUEST; a READY only ever wakes the fetch SINK, which has
 * nothing to do here. An unknown cookie tears the channel. */
static void ct_transfer_request_arrived(const ct_async_req_t *r);
static void ct_transfer_ready_arrived(uint64_t cookie, dest_t *d)
{
    struct ct_filerec *rec = ct_cookie_waiter(cookie);
    if (!rec) {
        ERROR("TRANSFER_READY for unknown cookie %lu; dropping connection\n",
              (unsigned long)cookie);
        async_free_dest(d);
        return;
    }
    /* The earlier-end SOURCE is now a passive responder to
     * CT_TRANSFER_REQUEST; a READY only ever wakes the fetch SINK, which has
     * nothing to do here. */
}

/* Inbound CT_TRANSFER_DATA (runs on the loop so the record lookup is
 * serialized with the registration lifecycle). Write the payload into the
 * sink's file at the wire offset and advance next; a torn stream
 * (unknown record, offset gap / overlap, or pwrite failure) sends a
 * CT_TRANSFER_ERROR to the peer, closes the transfer with the local app,
 * and destroys the channel. */
static void ct_transfer_data_arrived(const ct_async_req_t *r)
{
    struct ct_filerec *rec = ct_cookie_waiter(r->cookie);
    if (!rec || rec->role != CT_SINK) {
        ERROR("TRANSFER_DATA: no sink record for cookie %lu\n",
              (unsigned long)r->cookie);
        async_free_dest(r->d);
        return;
    }
    if (r->offset != rec->next) {
        ERROR("TRANSFER_DATA: cookie %lu offset %lu != expected %lu\n",
              (unsigned long)r->cookie, (unsigned long)r->offset,
              (unsigned long)rec->next);
        if (r->d->state == TCPACTIVE)
            ct_send_transfer_error(r->d, r->cookie, CT_STATUS_IOERR);
        ct_push_filedone(r->cookie, CT_STATUS_IOERR, rec->next - rec->offset);
        ct_rec_free_now(r->cookie);
        async_free_dest(r->d);
        return;
    }
    const char *payload = r->data + sizeof(ctp_t) + sizeof(ct_transfer_data);
    size_t n            = r->datalen - sizeof(ctp_t) - sizeof(ct_transfer_data);
    ssize_t w           = pwrite(rec->fd, payload, n, (off_t)r->offset);
    if (w < 0) {
        uint32_t status = ct_errno_to_status(errno);
        ERROR("TRANSFER_DATA: pwrite(cookie %lu, offset %lu): %s\n",
              (unsigned long)r->cookie, (unsigned long)r->offset,
              strerror(errno));
        if (r->d->state == TCPACTIVE)
            ct_send_transfer_error(r->d, r->cookie, status);
        ct_push_filedone(r->cookie, status, rec->next - rec->offset);
        ct_rec_free_now(r->cookie);
        async_free_dest(r->d);
        return;
    }
    if ((size_t)w != n) {
        /* a short write on a regular file: the disk could not take the chunk */
        ERROR("TRANSFER_DATA: short pwrite(cookie %lu) %ld != %zu\n",
              (unsigned long)r->cookie, (long)w, n);
        if (r->d->state == TCPACTIVE)
            ct_send_transfer_error(r->d, r->cookie, CT_STATUS_IOERR);
        ct_push_filedone(r->cookie, CT_STATUS_IOERR, rec->next - rec->offset);
        ct_rec_free_now(r->cookie);
        async_free_dest(r->d);
        return;
    }
    rec->next = r->offset + n;

    /* Sink-driven pull: after a chunk lands, ask for the next one. Only the
     * DRIVER (server-side, push) sink drives; the fetch client sink stays
     * passive. The EOF that ends the stream comes from the source, never a
     * short chunk. */
    if (rec->role == CT_SINK && rec->is_driver && r->d->state == TCPACTIVE)
        ct_send_transfer_request(r->d, r->cookie, rec->next, CT_CHUNKMAX);
}

/* Inbound CT_TRANSFER_EOF or CT_TRANSFER_ERROR (loop-side): finalize a sink
 * record for the local app. A peer-originated error is the legitimate
 * terminal state (the source's pump failed), so the channel stays up for
 * other traffic. */
static void ct_transfer_finish_arrived(const ct_async_req_t *r, uint32_t status)
{
    struct ct_filerec *rec = ct_cookie_waiter(r->cookie);
    if (!rec || rec->role != CT_SINK) {
        ERROR("TRANSFER_{EOF,ERROR}: no sink record for cookie %lu\n",
              (unsigned long)r->cookie);
        async_free_dest(r->d);
        return;
    }
    uint64_t written = rec->next - rec->offset;
    if (status == CT_STATUS_SUCCESS && rec->length != 0 &&
        written != rec->length) {
        ERROR("TRANSFER_EOF: cookie %lu wrote %lu, registered %lu\n",
              (unsigned long)r->cookie, (unsigned long)written,
              (unsigned long)rec->length);
        status = CT_STATUS_IOERR;
    }
    ct_push_filedone(r->cookie, status, written);
    ct_rec_free_now(r->cookie);
}

static void file_async_cb(uv_async_t *arg)
{
    for (;;) {
        ct_async_req_t *r;
        uv_mutex_lock(&file_async_mutex);
        r = file_async_q;
        if (r) {
            file_async_q = r->next;
            if (!file_async_q)
                file_async_tail = NULL;
        }
        uv_mutex_unlock(&file_async_mutex);
        if (!r)
            break;
        dest_t *d = r->d;
        switch (r->op) {
        case CT_ASYNC_READY:
            ct_transfer_ready_arrived(r->cookie, d);
            break;
        case CT_ASYNC_REQUEST:
            ct_transfer_request_arrived(r);
            break;
        case CT_ASYNC_FAILDEST:
            ct_fail_dest_transfers(d);
            /* setuptls() (a worker) enqueued this op after a handshake
             * failure. The tcphandle belongs to codatunnel_main_loop, so the
             * worker may not uv_read_stop/uv_close it; tear it down here on
             * the loop. Skip if free_dest() already started closing it. */
            if (d->tcphandle && d->state != TCPCLOSING) {
                uv_read_stop((uv_stream_t *)d->tcphandle);
                uv_close((uv_handle_t *)d->tcphandle, free_tcphandle);
                d->tcphandle = NULL;
            }
            break;
        case CT_ASYNC_DATA:
            ct_transfer_data_arrived(r);
            break;
        case CT_ASYNC_EOF:
            ct_transfer_finish_arrived(r, CT_STATUS_SUCCESS);
            break;
        case CT_ASYNC_ERROR:
            ct_transfer_finish_arrived(r, r->status);
            break;
        default:
            ERROR("file_async: unknown op %d\n", r->op);
            break;
        }
        if (r->data)
            free(r->data); /* the CT_TRANSFER_DATA owner buffer */
        free(r);
        /* The peeloff worker breaks on the first CT_TRANSFER record it hands
         * off, leaving the rest of the TLS queue (including any CT_PKT behind
         * it) unconsumed; the only thing that drains it otherwise is a fresh
         * recv on this dest, so a trailing RPC reply can sit here until some
         * unrelated packet happens to arrive. If the dest is still alive and
         * bytes remain queued, spawn another worker to keep draining.
         * async_free_dest() only flips state to TLSERROR (it does not free),
         * so d is still valid here; the TCPACTIVE check skips the teardown
         * path, and uvcount>0 is what bounds the re-drain to real work. */
        if (d->state == TCPACTIVE) {
            uv_mutex_lock(&d->uvcount_mutex);
            int uc        = d->uvcount;
            int needdrain = (uc > 0);
            uv_mutex_unlock(&d->uvcount_mutex);
            if (needdrain) {
                TLOG("CT_REDRRAIN d=%p uvcount=%d\n", d, uc);
                uv_work_t *w = malloc(sizeof(*w));
                if (w) {
                    w->data = d;
                    uv_queue_work(codatunnel_main_loop, w, peeloff_and_decrypt,
                                  cleanup_work);
                }
            }
        }
    }
}

static void ct_filereg_handler(int fd, const char *body, size_t bodylen)
{
    const ct_filereg *reg = (const ct_filereg *)body;
    struct ct_filerec *rec;

    if (reg->cookie == 0 || bodylen < sizeof(ct_filereg))
        goto drop; /* malformed */
    if (reg->role > CT_SINK || reg->peerlen < 2 ||
        reg->peerlen > sizeof(reg->peer))
        goto drop; /* role invalid, or peer not a real sockaddr */

    if (fd < 0) {
        /* no fd arrived (the app must hand one in the same datagram) */
        DEBUG("FILEREG: no fd via SCM_RIGHTS, dropping\n");
        ct_push_filedone(reg->cookie, CT_STATUS_IOERR, 0);
        return;
    }

    rec = malloc(sizeof(*rec));
    if (!rec) {
        ERROR("malloc() failed\n");
        close(fd);
        ct_push_filedone(reg->cookie, CT_STATUS_IOERR, 0);
        return;
    }
    rec->fd     = fd;
    rec->role   = reg->role;
    rec->offset = reg->offset;
    rec->next   = reg->offset;
    rec->length = reg->length;
    memcpy(&rec->peer, &reg->peer, reg->peerlen);
    rec->peerlen = reg->peerlen;
    uv_mutex_init(&rec->lock);
    rec->refs      = 0;
    rec->cancel    = 0;
    rec->is_driver = (reg->flags & CT_FILREG_DRIVER) != 0;

    if (ct_cookie_add(reg->cookie, rec) != 0) {
        /* cookie already registered (or table full): the first
           registration still owns it; drop this one, no FILE_DONE yet */
        DEBUG("FILEREG: cookie %lu already registered, dropping\n",
              (unsigned long)reg->cookie);
        close(fd);
        uv_mutex_destroy(&rec->lock);
        free(rec);
        return;
    }

    TLOG(
        "TCPFTP FILEREG cookie=%lu role=%s is_driver=%d offset=%lu length=%lu\n",
        (unsigned long)reg->cookie,
        (reg->role == CT_SOURCE) ? "source" : "sink",
        (reg->flags & CT_FILREG_DRIVER) != 0, (unsigned long)reg->offset,
        (unsigned long)reg->length);

    /* The "driver" end (always the RPC server's CheckSE side) sends
     * CT_TRANSFER_READY. A driver SOURCE starts the pump immediately; a driver
     * SINK opens, sends the first REQUEST, and waits for its chunks. An
     * EARLIER SOURCE waits for the peer's REQUESTs; an EARLIER SINK waits
     * for the first chunk. */
    if (reg->flags & CT_FILREG_DRIVER) {
        dest_t *d = getdest(&rec->peer, rec->peerlen);
        if (d && d->state == TCPACTIVE) {
            if (reg->role == CT_SOURCE) {
                ct_pump_do_start(reg->cookie, 1);
            } else {
                /* Sink-driven pull: request the first chunk. */
                ct_send_transfer_request(d, reg->cookie, rec->offset,
                                         CT_CHUNKMAX);
            }
        }
        /* A channel that is not yet TCPACTIVE cannot be signalled from here;
         * the READY / pump start is deferred and issued from
         * ct_redrive_one() when the channel commits TCPACTIVE. */
    }
    return;
drop:
    DEBUG("FILEREG: malformed packet dropped\n");
}

/* A channel died. The app's request/transfer for every record whose peer was
 * that channel can never complete (the other side is gone): a dead channel
 * means no REQUEST/DATA/EOF can ever complete a pending SOURCE or SINK record
 * that rides it. Fail them all with a terminal FILEDONE instead of orphaning
 * them in the cookie table. Runs on the uv loop only (the cookie table lives
 * there). */
struct ct_fail_ctx {
    dest_t *d;
};

static void ct_fail_one(uint64_t cookie, void *waiter, void *arg)
{
    struct ct_fail_ctx *ctx = (struct ct_fail_ctx *)arg;
    struct ct_filerec *r    = (struct ct_filerec *)waiter;
    if (r && ct_peer_matches_dest(r, ctx->d)) {
        ct_push_filedone(cookie, CT_STATUS_TIMEOUT, r->next - r->offset);
        ct_rec_free_now(cookie);
    }
}

void ct_fail_dest_transfers(dest_t *d)
{
    struct ct_fail_ctx ctx = { .d = d };
    ct_cookie_foreach(ct_fail_one, &ctx);
}

static void ct_fileunreg_handler(uint64_t cookie)
{
    struct ct_filerec *rec = ct_cookie_waiter(cookie);
    if (!rec)
        return; /* never registered, or already released */
    uv_mutex_lock(&rec->lock);
    if (rec->refs == 0) {
        /* idle: free now (the uv loop owns all idle free/close) */
        ct_cookie_remove(cookie);
        close(rec->fd);
        uv_mutex_unlock(&rec->lock);
        uv_mutex_destroy(&rec->lock);
        free(rec);
    } else {
        /* a source pump is operating it (the app cancelled mid-transfer);
         * tell it to stop and it will request the free through file_async */
        rec->cancel = 1;
        uv_mutex_unlock(&rec->lock);
    }
}

static void recv_codatunnel_cb(uv_stream_t *handle, ssize_t nread,
                               const uv_buf_t *buf)
{
    uv_pipe_t *vside = (uv_pipe_t *)handle;

    DEBUG("packet received from codatunnel nread=%ld buf=%p\n", nread,
          buf ? buf->base : NULL);

    if (nread == UV_EOF) {
        /* app side closed the vside */
        DEBUG("codatunnel closed (EOF)\n");
        uv_stop(codatunnel_main_loop);
        uv_close((uv_handle_t *)handle, NULL);
        return;
    }

    if (nread < 0) {
        /* We shouldn't see read errors on the codatunnel socketpair. -JH */
        /* if we close the socketpair endpoint, we might just as well stop */
        uv_stop(codatunnel_main_loop);
        uv_close((uv_handle_t *)handle, NULL);
        goto exit_drop;
    }
    if (nread < sizeof(ctp_t)) {
        DEBUG("short packet received from codatunnel\n");
        goto exit_drop;
    }

    /* We have a legit packet; it was already been read into buf before this
      * upcall was invoked by libuv */

    ctp_t *p = (ctp_t *)buf->base;

    TLOG("CT_OUT_VSIDE op=%u nread=%ld\n", (unsigned)p->opcode, nread);

    if (nread != (sizeof(ctp_t) + p->msglen)) {
        DEBUG("incomplete packet received from codatunnel\n");
        goto exit_drop;
    }

    dest_t *d = getdest(&p->addr, p->addrlen);

    /* Try to establish a new TCP connection for future use;
     * do this only once per INIT0 to avoid TCP SYN flood;
     * Only clients should attempt this, because of NAT firewalls */
    if (p->opcode == CT_INIT0 && !codatunnel_I_am_server) {
        const char *msgbody = buf->base + sizeof(ctp_t);
        if (!d) { /* new destination */
            const char *peername = strndup(msgbody, p->msglen);

            DEBUG("createdest(%s)\n", peername ? peername : "<undefined>");
            d = createdest(&p->addr, p->addrlen, peername);
        }
        /* make sure we're still validating the right peer name? */
        else if (!d->fqdn || strncmp(d->fqdn, msgbody, p->msglen) != 0) {
            ERROR("INIT0 hostname mismatch on remotedest %s\n",
                  d->fqdn ? d->fqdn : "<undefined>");
        }

        if (d->state == ALLOCATED) {
            d->state = TCPATTEMPTING;
            try_creating_tcp_connection(d);
        }
    }

    /* we never actually send an INIT0 across the wire, it is just to notify
     * codatunneld of our intended destination.. */
    if (p->opcode == CT_INIT0)
        goto exit_drop;

    /*
     * The file-transfer control datagrams belong to the daemon and must
     * never traverse the wire as CT_PKT.  FILEREG/FILEUNREG are consumed
     * locally (registration state, see the ct_filereg/ct_fileunreg
     * handlers); either way the packet is dropped here rather than
     * forwarded to the RPC2 destination.
     */
    if (p->opcode == CT_FILEREG) {
        const char *body = (const char *)buf->base + sizeof(ctp_t);
        /* the app handed the file fd in the same datagram via SCM_RIGHTS */
        int fd = ct_accept_fd(vside);
        ct_filereg_handler(fd, body, p->msglen);
        goto exit_drop;
    }
    if (p->opcode == CT_FILEUNREG) {
        if (p->msglen >= sizeof(uint64_t)) {
            uint64_t cookie;
            const char *body = (const char *)buf->base + sizeof(ctp_t);
            memcpy(&cookie, body, sizeof(cookie));
            ct_fileunreg_handler(cookie);
        }
        goto exit_drop;
    }

    /* what do we do with packet p for destination d? */

    if (d && (d->state == TCPACTIVE)) {
        /* Changed this to always send retries so we get RPC2_BUSY as a keep
         * alive on long running operations -JH */
        if (0) { // p->is_retry) {
            /* drop retry packet;
               only exception is when nothing has yet been sent on new TCP
               connection; the state may have become TCPACTIVE after most
               recent retry;  those earlier retries via UDP may all have
               been lost (e.g., because of firewall settings at dest);
               make sure you send at least this one; all future
               retries will be dropped;  no harm if earlier retries got through
               (Satya, 1/20/2018)
            */
            goto exit_drop;
        } else {
            send_to_tcp_dest(d, nread, buf, NULL);
            /* free buf in cascaded cb */
            return;
        }
    }
    /* Possible states for destination d: ALLOCATED, TCPATTEMPTING, and
       TLSHANDSHAKE. In all of these cases we'll fall back to UDP as long as
       we have not yet had a gnutls certificate validation error.

       UDP fallback: always forward UDP packets if TCPACTIVE is not true; RPC2
       duplicate elimination at higher level will drop as needed for
       at-most-once semantics; if TCPACTIVE happens later for this
       destination, early packets will be sent by UDP, but later ones
       by TCP; nothing special needs to be done to track these or
       avoid race conditions; the higher level processing in RPC2 will
       ensure at-most-once semantics regardless of how the packet
       traveled (Satya, 1/20/2018)
    */
    else if (!codatunnel_onlytcp && !(d && d->certvalidation_failed)) {
        send_to_udp_dest(nread, buf, NULL, 0);
        /* free buf only in cascaded cb */
        return;
    }

exit_drop:
    free(buf->base); /* packet dropped, no cascaded cb */
}

static void send_to_udp_dest(ssize_t nread, const uv_buf_t *buf,
                             const struct sockaddr *addr, unsigned flags)
{
    /* Somewhat complicated structure is to avoid data copy of payload */

    ctp_t *p = (ctp_t *)buf->base;
    minicb_udp_req_t *req;
    uv_buf_t msg;
    int rc;

    req = malloc(sizeof(*req));
    if (!req) {
        /* unable to allocate, free buffer and let RPC2 retry */
        ERROR("malloc() failed\n");
        free(buf->base);
        return;
    }

    /* data to send is what follows the codatunnel packet header */
    msg = uv_buf_init(buf->base + sizeof(ctp_t), nread - sizeof(ctp_t));

    /* make sure the buffer is released when the send completes */
    req->req.data = buf->base;

    /* forward packet to the remote host */
    rc = uv_udp_send(&req->req, &udpsocket, &msg, 1,
                     (struct sockaddr *)&p->addr, minicb_udp);
    DEBUG("udpsocket.send_queue_count = %lu\n", udpsocket.send_queue_count);
    if (rc) {
        /* unable to forward packet to udp destination.
         * free buffers and continue, the RPC2 layer will retry. */
        ERROR("uv_udp_send(): rc = %d\n", rc);
        minicb_udp(&req->req, rc);
    }
}

/* mark connection as an error and wake up main loop to call free_dest */
/* this has to be used in worker threads */
static void async_free_dest(dest_t *d)
{
    uv_mutex_lock(&d->uvcount_mutex);
    d->state = TLSERROR;
    uv_mutex_unlock(&d->uvcount_mutex);

    /* wake up main loop so it can call free_dest */
    uv_async_send(&d->wakeup);
}

/* running on worker thread, should only use limited set of libuv functions */
static void send_to_tls_dest(uv_work_t *req)
{
    send_to_tls_req_t *w = req->data;
    dest_t *d            = w->dest;
    ssize_t rc;

    /* A source pump (see ct_pump) re-arms on this record's completion via
     * w->done; fire it on every exit of this worker so a failed record can't
     * stall the pump. (The one exit that does not fire it:
     * drain_outbound_queues() freeing a still-queued record on channel
     * teardown.) */

resend:
    if (d->state != TCPACTIVE || !d->my_tls_session) {
        ERROR("about to send packet, but we have no active tls session\n");
        if (w->done)
            uv_async_send(w->done);
        return;
    }

    /* gnutls is not thread-safe: serialize gnutls_record_send() against
     * gnutls_record_recv() on the peeling-off worker thread */
    uv_mutex_lock(&d->tls_session_mutex);
    DEBUG("About to call gnutls_record_send()\n");
    rc = gnutls_record_send(d->my_tls_session, w->buf.base, w->len);
    DEBUG("Just returned from gnutls_record_send()\n");
    /* actual sending of bytes happens in upcall of above  */
    uv_mutex_unlock(&d->tls_session_mutex);
    TLOG("CT_OUT_TLS rc=%ld len=%lu\n", rc, w->len);

    if (rc == GNUTLS_E_INTERRUPTED || rc == GNUTLS_E_AGAIN)
        goto resend;

    /* Everything went well, this chunk is on TCP; re-arm the pump if any. */
    if (rc == w->len) {
        if (w->done)
            uv_async_send(w->done);
        return;
    }

    /* something went wrong */
    if (rc < 0) {
        ERROR("gnutls_record_send(%s): rc = %ld (%s)\n", d->fqdn ? d->fqdn : "",
              rc, gnutls_strerror(rc));
    } else if (rc != w->len) {
        ERROR("gnutls_record_send(%s): short write %ld, expected %lu\n",
              d->fqdn ? d->fqdn : "", rc, w->len);
    }
    if (w->done)
        uv_async_send(w->done);
    async_free_dest(d);
}

static void _send_to_tls_done(uv_work_t *req, int status)
{
    send_to_tls_req_t *w = req->data;
    dest_t *d            = w->dest;

    uv_mutex_lock(&d->tls_send_mutex);

    d->tls_send_queue = w->qnext;
    free(w->buf.base);
    free(w);

    /* if there is stuff queued, fire off the next job */
    if (d->tls_send_queue != NULL) {
        send_to_tls_req_t *w = d->tls_send_queue;
        uv_queue_work(codatunnel_main_loop, &w->req, send_to_tls_dest,
                      _send_to_tls_done);
    }

    uv_mutex_unlock(&d->tls_send_mutex);
}

/* To accommodate TLS, send_to_tcp_dest() has been split;
   top half invokes TLS; upcall from TLS engine invokes bottom half which
   does the actual sending on TCP */
static void send_to_tcp_dest(dest_t *d, ssize_t nread, const uv_buf_t *buf,
                             uv_async_t *done)
{
    DEBUG("send_to_tcp_dest(%p, %ld, %p)\n", d, nread, buf);

    /* Convert ctp_t fields to network order, before encryption */
    ctp_t *p = (ctp_t *)buf->base;
    DEBUG("is_retry = %u  opcode = %u  msglen = %u\n", p->is_retry, p->opcode,
          p->msglen);
    /* The opcode word (former is_init0) now carries file-control opcodes in
       addition to CT_PKT/CT_INIT0; like is_retry and msglen it travels in
       network byte order across the TLS hop. Only CT_PKT reaches this point
       today, so the wire bytes are unchanged from before the rename. */
    p->opcode   = htonl(p->opcode);
    p->is_retry = htonl(p->is_retry);
    p->msglen   = htonl(p->msglen);
    /* ignoring addr and addrlen; will be clobbered by dest_t->destaddr and
     * dest_t->destlen on the other side of the tunnel */

    /* We assume that TLS is in use; code does not work without TLS
       Using asserts for now; perhaps we need to be less brutal? */

    assert(d->my_tls_session);
    assert(d->state == TCPACTIVE); /* never reach here in TLSHANDSHAKE */

    /* Do gnutls operations on separate thread to avoid blocking
     * due to TLS */
    send_to_tls_req_t *w = malloc(sizeof(send_to_tls_req_t));
    w->dest              = d;
    w->buf               = uv_buf_init(buf->base, buf->len);
    w->len               = nread;
    w->req.data          = w;
    w->qnext             = NULL;
    w->done              = done;

    uv_mutex_lock(&d->tls_send_mutex);
    send_to_tls_req_t **q = (send_to_tls_req_t **)&d->tls_send_queue;

    if (*q == NULL) /* if there was nothing queued, kick off a worker */
        uv_queue_work(codatunnel_main_loop, &w->req, send_to_tls_dest,
                      _send_to_tls_done);

    /* append to end of queue */
    while (*q != NULL)
        q = &((*q)->qnext);
    *q = w;
    uv_mutex_unlock(&d->tls_send_mutex);
}

/* Upcall handler for TLS to send encrypted packet using uv as transport;
   returns bytes sent*/
static ssize_t vec_push_func(gnutls_transport_ptr_t gtp, const giovec_t *iov,
                             int iovcnt)
{
    dest_t *d = (dest_t *)gtp;
    minicb_tcp_req_t mtr;
    unsigned int i;
    ssize_t bytecount = 0;

    DEBUG("vec_push_func(%p, %p, %d)\n", gtp, iov, iovcnt);
    if (iovcnt <= 0)
        return 0;

    assert(iovcnt <= MTR_MAXBUFS);
    for (i = 0; i < iovcnt; i++) {
        mtr.msg[i] = uv_buf_init(iov[i].iov_base, iov[i].iov_len);
        bytecount += iov[i].iov_len;
    }
    mtr.msglen       = iovcnt;
    mtr.dest         = d;
    mtr.qnext        = NULL;
    mtr.write_status = -EINTR;
    uv_sem_init(&mtr.write_done, 0);

    /* queue mtr */
    uv_mutex_lock(&d->outbound_mutex);
    minicb_tcp_req_t **p = &d->outbound_queue;
    while (*p != NULL)
        p = &((*p)->qnext);
    *p = &mtr;
    uv_mutex_unlock(&d->outbound_mutex);

    /* kick off sender */
    DEBUG("waking outbound_worker\n");
    uv_async_send(&d->wakeup);
    uv_sem_wait(&mtr.write_done);

    uv_sem_destroy(&mtr.write_done);
    return (mtr.write_status == 0) ? bytecount : mtr.write_status;
}

void outbound_worker_cb(uv_async_t *async)
{
    minicb_tcp_req_t *mtr;
    dest_t *d = async->data;
    int rc;

    while (1) {
        /* in case an error occurred, we can safely tear down here */
        if (d->state == TLSERROR) {
            free_dest(d);
            return;
        }

        uv_mutex_lock(&d->outbound_mutex);
        mtr               = d->outbound_queue;
        d->outbound_queue = mtr ? mtr->qnext : NULL;
        uv_mutex_unlock(&d->outbound_mutex);

        if (!mtr) /* queue empty, nothing left to do */
            return;

        /* forward packet to the remote host */
        DEBUG("Going to do uv_write(%p, %p, ...)\n", mtr, d->tcphandle);
        rc = uv_write(&mtr->req, (uv_stream_t *)d->tcphandle, mtr->msg,
                      mtr->msglen, minicb_tcp);
        DEBUG("Just completed uv_write() --> %d\n", rc);
        if (rc) {
            /* unable to send on tcp connection, pass back failure */
            ERROR("uv_write(): rc = %d\n", rc);
            minicb_tcp(&mtr->req, rc);
            async_free_dest(d);
        }
    }
}

static void cleanup_work(uv_work_t *w, int status)
{
    free(w);
}

/* running on worker thread, should only use limited set of libuv functions */
static void peeloff_and_decrypt(uv_work_t *w)
{
    /* Asynchronous worker invoked via uv_queue_work();
       Try to peel off bytes, decrypt them and hand them off;
       We don't know yet if we have all the bytes of even one gnutls record.
       We rely on gnutls to reassemble, and then decrypt the record
       (reasembly used to be our job, pre-tls).
    */

    DEBUG("peeloff_and_decrypt()\n");

    dest_t *d = (dest_t *)(w->data);
    TLOG("CT_IN_WORK d=%p\n", d);

    /* Assemble at most one gnutls_record at a time */
    uv_mutex_lock(&d->tls_receive_record_mutex);

    while (d->state == TCPACTIVE) {
        if (!d->decrypted_record) {
            DEBUG("Allocating d->decrypted_record\n");
            d->decrypted_record = malloc(CT_MAX_RECORD);

            if (!d->decrypted_record) {
                ERROR("malloc() failed\n");
                async_free_dest(d);
                break;
            }
        }
        /* else partially assembled TLS record already exists; just extend it */

        /* Try to peel off a complete encrypted record.
         * gnutls is not thread-safe: serialize against
         * gnutls_record_send() on the send worker thread */
        uv_mutex_lock(&d->tls_session_mutex);
        DEBUG("About to call gnutls_record_recv()\n");
        ssize_t rc = gnutls_record_recv(d->my_tls_session, d->decrypted_record,
                                        CT_MAX_RECORD);
        DEBUG("Just returned from gnutls_record_recv(), rc = %ld\n", rc);
        uv_mutex_unlock(&d->tls_session_mutex);

        if (rc == GNUTLS_E_INTERRUPTED) {
            DEBUG("gnutls_record_recv() --> GNUTLS_E_INTERRUPTED\n");
            continue; /* as if nothing had happened */
        }

        if (rc == GNUTLS_E_AGAIN) {
            /* eat_uvbytes() ran out of bytes;
               need to continue when more bytes are received in next uv upcall;
               leave current d->decrypted_record undisturbed for continuation
            */
            break;
        }
        if (rc <= 0) { /* something went wrong */
            /* when the other side closes the connection right after the
             * handshake, we see this error. No need to log it. */
            if (rc != GNUTLS_E_PREMATURE_TERMINATION)
                ERROR("gnutls_record_recv(%s): rc = %ld (%s)\n",
                      d->fqdn ? d->fqdn : "", rc, gnutls_strerror(rc));
            async_free_dest(d);
            break;
        }
        if (rc > (ssize_t)CT_MAX_RECORD) {
            /* Buffer caps rc at CT_MAX_RECORD, so this can't fire; kept as a
               guard in case the buffer size and CT_MAX_RECORD drift apart. */
            ERROR("Monster packet: gnutls_record_recv(%s) --> %ld\n",
                  d->fqdn ? d->fqdn : "", rc);
            async_free_dest(d);
            break;
        }

        /* Yay!  We have a complete gnutls record of length rc;
           Hand it off to codasrv/Venus;
           Then carry on with this loop */

        DEBUG("Yay!  we have a complete gnutls record of length %ld\n", rc);

        ctp_t *packet = (ctp_t *)d->decrypted_record;
        if (rc < sizeof(ctp_t) || strncmp(packet->magic, "magic01", 8) != 0) {
            DEBUG("unexpected packet header received, dropping connection\n");
            async_free_dest(d);
            break;
        }

        /* Replace recipient address with sender's address, so that
           recvfrom() can provide the "from" address. */
        memcpy(&packet->addr, &d->destaddr, d->destlen);
        packet->addrlen = d->destlen;
        packet->msglen  = rc - sizeof(ctp_t);
        packet->opcode  = ntohl(packet->opcode);

        /*
         * Inbound opcodes. CT_PKT (the encapsulated RPC2 packet) is relayed
         * to the local app over the vside below. The CT_TRANSFER_* opcodes
         * (5-8) are daemon-to-daemon: they must never reach the app, whose
         * RPC2 layer would treat them as a bogus packet. Only the header is
         * valid here; the daemon-side handling of the transfer opcodes lives
         * on the event loop, because this worker thread must not touch the
         * cookie table. We therefore decode just enough to forward the packet
         * to the loop (via file_async), which owns all rec/cookie lifetimes.
         * An opcode the daemon does not recognize tears the connection down.
         */
        if (packet->opcode == CT_TRANSFER_READY ||
            packet->opcode == CT_TRANSFER_REQUEST ||
            packet->opcode == CT_TRANSFER_DATA ||
            packet->opcode == CT_TRANSFER_EOF ||
            packet->opcode == CT_TRANSFER_ERROR) {
            /* daemon->daemon transfer control/data (5-9). Decode just the
              * cookie (network order in the body) and hand the record to the
              * loop via file_async; the loop owns the cookie table and does
              * the I/O, and may tear d down. REQUEST also decodes offset
              * (+8) and len (+16). For DATA the decrypted record buffer is
              * handed over (the loop frees it); for the others it is dropped
              * here. */
            uint64_t cookie;
            memcpy(&cookie, d->decrypted_record + sizeof(ctp_t),
                   sizeof(cookie));
            cookie = ct_ntoh64(cookie);

            if (packet->opcode == CT_TRANSFER_DATA) {
                if (packet->msglen < sizeof(ct_transfer_data)) {
                    ERROR("peeloff: bad DATA msglen %u; dropping it\n",
                          packet->msglen);
                    async_free_dest(d);
                    break;
                }
                uint64_t offset;
                memcpy(&offset,
                       d->decrypted_record + sizeof(ctp_t) + sizeof(uint64_t),
                       sizeof(offset));
                file_async_enqueue_data(cookie, ct_ntoh64(offset), d,
                                        d->decrypted_record, (uint32_t)rc);
                d->decrypted_record = NULL; /* loop owns + frees the buffer */
                break;
            }
            if (packet->opcode == CT_TRANSFER_REQUEST) {
                if (packet->msglen < sizeof(ct_transfer_request)) {
                    ERROR("peeloff: bad REQUEST msglen %u; dropping it\n",
                          packet->msglen);
                    async_free_dest(d);
                    break;
                }
                uint64_t offset, len;
                memcpy(&offset,
                       d->decrypted_record + sizeof(ctp_t) + sizeof(uint64_t),
                       sizeof(offset));
                memcpy(&len,
                       d->decrypted_record + sizeof(ctp_t) +
                           2 * sizeof(uint64_t),
                       sizeof(len));
                ct_async_req_t *r = malloc(sizeof(*r));
                if (!r) {
                    async_free_dest(d);
                    break;
                }
                memset(r, 0, sizeof(*r));
                r->cookie  = cookie;
                r->op      = CT_ASYNC_REQUEST;
                r->d       = d;
                r->offset  = ct_ntoh64(offset);
                r->datalen = (uint32_t)ct_ntoh64(len);
                file_async_push(r);
                break;
            }
            if (packet->opcode == CT_TRANSFER_EOF) {
                if (packet->msglen != sizeof(ct_transfer_eof)) {
                    ERROR("peeloff: bad EOF msglen %u; dropping it\n",
                          packet->msglen);
                    async_free_dest(d);
                    break;
                }
                file_async_enqueue(cookie, CT_ASYNC_EOF, d);
                break;
            }
            if (packet->opcode == CT_TRANSFER_ERROR) {
                if (packet->msglen < sizeof(ct_transfer_error)) {
                    ERROR("peeloff: bad ERROR msglen %u; dropping it\n",
                          packet->msglen);
                    async_free_dest(d);
                    break;
                }
                uint32_t status;
                memcpy(&status,
                       d->decrypted_record + sizeof(ctp_t) + sizeof(uint64_t),
                       sizeof(status));
                file_async_enqueue_error(cookie, ntohl(status), d);
                break;
            }
            file_async_enqueue(cookie, CT_ASYNC_READY, d);
            break; /* the loop will (or will not) free the dest */
        }

        if (packet->opcode != CT_PKT) {
            /* anything the daemon does not recognize tears the channel */
            ERROR("peeloff: unrecognizable opcode %u; dropping channel\n",
                  packet->opcode);
            async_free_dest(d);
            break;
        }

        /* Prepare to send  */
        minicb_pipe_req_t *req = malloc(sizeof(*req));
        if (!req) {
            /* unable to allocate, free buffers and trigger a disconnection
             * because we have no other way to force a retry. */
            ERROR("malloc() failed\n");
            async_free_dest(d);
            break;
        }

        /* queue packet to forward to codatunnel */
        req->msg      = uv_buf_init(d->decrypted_record, rc); /* to send */
        req->req.data = d->decrypted_record;
        req->qnext    = NULL;

        /* make sure we allocate a new decryption buffer */
        d->decrypted_record = NULL;

        /* append packet to queue of pending packets */
        uv_mutex_lock(&async_forward_mutex);
        minicb_pipe_req_t **q = (minicb_pipe_req_t **)&async_forward.data;
        int qlen              = 0;
        while (*q != NULL) {
            qlen++;
            q = &(*q)->qnext;
        }
        *q = req;
        qlen++; /* include the one we just added */
        uv_mutex_unlock(&async_forward_mutex);

        TLOG("CT_PKT_ENQ d=%p op=%u qlen=%d\n", d, (unsigned)packet->opcode,
             qlen);

        /* signal mainloop to send this packet */
        uv_async_send(&async_forward);
    }
    {
        int _uc, _ps;
        uv_mutex_lock(&d->uvcount_mutex);
        _uc = d->uvcount;
        _ps = d->read_paused;
        uv_mutex_unlock(&d->uvcount_mutex);
        TLOG("CT_WORK_EXIT d=%p uvcount=%d paused=%d\n", d, _uc, _ps);
    }
    uv_mutex_unlock(&d->tls_receive_record_mutex);
}

/* Re-arm driver registrations that the FILEREG handler dropped because the
 * channel was not yet TCPACTIVE. The per-dest redrive async is posted from
 * setuptls() (a worker) and serviced on the loop; each channel has its own
 * handle, so concurrent channel-ups never share one. A live record here was
 * never started: any peer traffic for it (a chunk, EOF, or the first REQUEST
 * answer) would have errored and freed it. A record that already streamed has
 * refs != 0 and is skipped. */
struct ct_redrive_ctx {
    dest_t *d;
};

static void ct_redrive_one(uint64_t cookie, void *waiter, void *arg)
{
    struct ct_redrive_ctx *ctx = (struct ct_redrive_ctx *)arg;
    struct ct_filerec *r       = (struct ct_filerec *)waiter;
    if (!r || !r->is_driver || r->cancel || r->refs != 0 ||
        !ct_peer_matches_dest(r, ctx->d))
        return;
    if (r->role == CT_SOURCE) {
        TLOG("TCPFTP RE-DRIVE pump cookie=%lu (channel up)\n",
             (unsigned long)cookie);
        ct_pump_do_start(cookie, 1);
    } else {
        TLOG("TCPFTP RE-DRIVE pull cookie=%lu (channel up)\n",
             (unsigned long)cookie);
        ct_send_transfer_request(ctx->d, cookie, r->offset, CT_CHUNKMAX);
    }
}

/* Loop-side: d->redrive is posted from setuptls() (a worker) once the channel
 * commits TCPACTIVE. Runs on the loop because ct_pump_do_start() starts a pump
 * (uv_async_init) and may emit the first chunk / FILEDONE (uv_write), all of
 * which are loop-only. */
void ct_redrive_cb(uv_async_t *async)
{
    struct ct_redrive_ctx ctx = { .d = (dest_t *)async->data };
    ct_cookie_foreach(ct_redrive_one, &ctx);
}

/* running on worker thread, should only use limited set of libuv functions */
static void setuptls(uv_work_t *w)
{
    /*
       setuptls() is done in a separate thread in the uv thread pool,
       via uv_queue_work(),  because of blocking in handshake;
       w->data is really of type (async_tls_parms_t *ap);

       Encapsulate all the set up of TLS for destination ap->d;
       ap->certverify  indicates whether setup includes verification of peer
       identity. Coda clients always insist on verifying server's identity;
       Coda servers don't care about Coda client identity;
       However, Coda servers connect to each other for resolution, etc.  and
       in those cases, both sides verify the other's identity.
    */

    int rc;

    async_tls_parms_t *ap        = (async_tls_parms_t *)w->data;
    dest_t *d                    = ap->d;
    gnutls_init_flags_t tlsflags = ap->tlsflags;
    peercheck_t certverify       = ap->certverify;

    /* make sure credentials are not destroyed/reloaded during the handshake */
    uv_rwlock_rdlock(&credential_load_lock);

    /* local TLS setup errors, fall back on UDP connection */
#define GNUTLSERROR(op, retcode)                                           \
    do {                                                                   \
        ERROR("%s(%s) --> %d (%s)\n", op, d->fqdn ? d->fqdn : "", retcode, \
              gnutls_strerror(retcode));                                   \
        if (d->state != TCPCLOSING) {                                      \
            if (certverify == IGNORE) {                                    \
                async_free_dest(d);                                        \
            } else {                                                       \
                uv_mutex_lock(&d->uvcount_mutex);                          \
                d->state = ALLOCATED;                                      \
                uv_mutex_unlock(&d->uvcount_mutex);                        \
                file_async_enqueue(0, CT_ASYNC_FAILDEST, d);               \
            }                                                              \
        }                                                                  \
        uv_rwlock_rdunlock(&credential_load_lock);                         \
        return;                                                            \
    } while (0)

    rc = gnutls_init(&d->my_tls_session, tlsflags);
    if (rc != GNUTLS_E_SUCCESS)
        GNUTLSERROR("gnutls_init", rc);

    rc = gnutls_set_default_priority(d->my_tls_session);
    if (rc != GNUTLS_E_SUCCESS)
        GNUTLSERROR("gnutls_set_default_priority", rc);

    /* per-session data and methods; methods all return void */
    gnutls_handshake_set_timeout(d->my_tls_session,
                                 GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
    gnutls_transport_set_ptr(d->my_tls_session, d);
    gnutls_transport_set_vec_push_function(d->my_tls_session, vec_push_func);
    gnutls_transport_set_pull_function(d->my_tls_session, eat_uvbytes);
    gnutls_transport_set_pull_timeout_function(d->my_tls_session, poll_uvbytes);

    rc = gnutls_credentials_set(d->my_tls_session, GNUTLS_CRD_CERTIFICATE,
                                x509_cred);
    if (rc != GNUTLS_E_SUCCESS)
        GNUTLSERROR("gnutls_credentials_set", rc);
    DEBUG("gnutls_credentials_set() successful\n");

    if (certverify == IGNORE ||
        d->fqdn == NULL) { /* don't bother checking peer identity */
        gnutls_certificate_server_set_request(d->my_tls_session,
                                              GNUTLS_CERT_IGNORE);
    } else { /* I am a client; verify server identify */
#if GNUTLS_VERSION_NUMBER >= 0x030406
        gnutls_session_set_verify_cert(d->my_tls_session, d->fqdn, 0);
#else
        /* TODO verify the peer's certificate by setting a callback with
         * gnutls_certificate_set_verify_function and then using
         * gnutls_certificate_verify_peers3 from it. */
#endif
    }

    /* Any errors after this point make the remote we're connecting to
     * suspicious, so we should probably not fall back on UDP on errors */

    /* Everything has been setup; do the TLS handshake */
    DEBUG("About to do gnutls_handshake(%s)\n", d->fqdn ? d->fqdn : "");
eagain:
    rc = gnutls_handshake(d->my_tls_session);

    if (rc == GNUTLS_E_INTERRUPTED || rc == GNUTLS_E_AGAIN)
    /* || rc == GNUTLS_E_WARNING_ALERT_RECEIVED */
    {
        DEBUG("gnutls_handshake(%s) got non-fatal error, trying again\n",
              d->fqdn ? d->fqdn : "");

        /* avoid busy looping while waiting for a network response */
        if (gnutls_record_get_direction(d->my_tls_session) == 0)
            poll_uvbytes(d, 10);

        goto eagain;
    }

#if GNUTLS_VERSION_NUMBER >= 0x030406
    if (rc == GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR) {
        int vrc = gnutls_session_get_verify_cert_status(d->my_tls_session);
        ERROR("gnutls_session_get_verify_cert_status() --> %d (%s)\n", vrc,
              gnutls_strerror(vrc));
        d->certvalidation_failed = 1;
    }
#endif

    if (rc != GNUTLS_E_SUCCESS)
        GNUTLSERROR("gnutls_handshake", rc);
#undef GNUTLSERROR

    DEBUG("gnutls_handshake(%s) successful\n", d->fqdn ? d->fqdn : "");
    d->certvalidation_failed = 0;
    /* Capture this session's negotiated max record size now, while we still own
     * the gnutls thread; the send/recv workers only start after TCPACTIVE. A
     * full-chunk record is ctp_t + ct_transfer_data + payload, so the payload
     * cap is the record max minus those two headers. */
    d->max_data_payload = gnutls_record_get_max_size(d->my_tls_session);
    if (d->max_data_payload > sizeof(ctp_t) + sizeof(ct_transfer_data))
        d->max_data_payload -= sizeof(ctp_t) + sizeof(ct_transfer_data);
    else
        d->max_data_payload = 0;
    d->state = TCPACTIVE; /* commit point for encrypted TCP tunnel */
    uv_rwlock_rdunlock(&credential_load_lock);

    /* Re-arm driver registrations that landed before this channel was active
     * (their FILEREG was signalled but not started). setuptls() runs on a
     * worker, but starting a pump is loop-only, so post the thread-safe
     * redrive async and let the loop run it. */
    uv_async_send(&d->redrive);

    /* Process any received data (or EOF) in case it arrived before we
     * finished processing the handshake. */
    w->data = d; /* reuse uv_work_t struct, cleanup_work is the same. */
    peeloff_and_decrypt(w);
}

static void tcp_connect_cb(uv_connect_t *req, int status)
{
    int i, rc;

    DEBUG("tcp_connect_cb(%p, %d)\n", req, status);
    dest_t *d = req->data;
    free(req); /* no further use */

    if (status != 0) { /* connection unsuccessful */
        d->state = ALLOCATED;
        free(d->tcphandle);
        d->tcphandle = NULL;
        /* a deferred FILEREG on this channel will not be redriven (we stay
         * ALLOCATED, not TCPACTIVE); hand it a terminal status or its
         * app-side file_wait blocks forever. Runs on the loop, so the direct
         * uv_write in ct_push_filedone is safe. */
        ct_fail_dest_transfers(d);
        return;
    }

    /* TCP connection successful */
    DEBUG("tcp_connect_cb(%p, %d) --> %p\n", d, status, d->tcphandle);
    d->tcphandle->data = d; /* point back, for use in upcalls */
    d->uvcount         = 0;
    d->uvoffset        = 0;
    for (i = 0; i < UVBUFLIMIT; i++) {
        d->enqarray[i].b.base   = NULL;
        d->enqarray[i].b.len    = 0;
        d->enqarray[i].numbytes = 0;
    }
    d->decrypted_record = NULL;
    d->state            = TLSHANDSHAKE;

    /* disable Nagle */
    uv_tcp_nodelay(d->tcphandle, 1);
    uv_tcp_keepalive(d->tcphandle, 1, CT_TCP_KEEPALIVE_IDLE);

    rc = uv_read_start((uv_stream_t *)d->tcphandle, alloc_cb, recv_tcp_cb);
    if (rc)
        DEBUG("uv_read_start() --> %d\n", rc);

    /* Prepare and launch TLS setup;
      ap can't be a local variable because their lifetime is
      longer than this function; */

    async_tls_parms_t *ap = malloc(sizeof(async_tls_parms_t));

    ap->d = d;
    if (codatunnel_I_am_server) {
        DEBUG("codatunnel_I_am_server\n");
        ap->tlsflags = (GNUTLS_SERVER | GNUTLS_NONBLOCK);
    } else {
        DEBUG("codatunnel_I_am_client\n");
        ap->tlsflags = (GNUTLS_CLIENT | GNUTLS_NONBLOCK);
    }

    ap->certverify = VERIFY;
    ap->work.data  = ap;

    DEBUG("about to call uv_queue_work()\n");
    rc = uv_queue_work(codatunnel_main_loop, &ap->work, setuptls, cleanup_work);
    DEBUG("after call to uv_queue_work()  -> %d\n", rc);
}

static void try_creating_tcp_connection(dest_t *d)
{
    uv_connect_t *req;

    DEBUG("try_creating_tcp_connection(%p)\n", d);
    d->tcphandle = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(codatunnel_main_loop, d->tcphandle);

    req = malloc(sizeof(uv_connect_t));
    assert(req != NULL);

    req->data = d; /* so we can identify dest in upcall */
    int rc = uv_tcp_connect(req, d->tcphandle, (struct sockaddr *)&d->destaddr,
                            tcp_connect_cb);
    if (rc)
        DEBUG("uv_tcp_connect --> %d\n", rc);
}

/* Poked by eat_uvbytes() (thread pool) once the recv queue has drained to
   zero while the read was paused. Runs on the event loop so it may call
   uv_read_start. */
void resume_read_cb(uv_async_t *async)
{
    dest_t *d  = async->data;
    int resume = 0;

    int paused, uc;
    uv_mutex_lock(&d->uvcount_mutex);
    if (d->state == TCPACTIVE && d->read_paused && d->uvcount == 0 &&
        d->tcphandle) {
        d->read_paused = 0;
        resume         = 1;
    }
    paused = d->read_paused;
    uc     = d->uvcount;
    uv_mutex_unlock(&d->uvcount_mutex);

    TLOG("CT_RD_RESUME_CB d=%p state=%s paused=%d uvcount=%d resuming=%d\n", d,
         tcpstatename(d->state), paused, uc, resume);

    if (resume)
        uv_read_start((uv_stream_t *)d->tcphandle, alloc_cb, recv_tcp_cb);
}

static void recv_tcp_cb(uv_stream_t *tcphandle, ssize_t nread,
                        const uv_buf_t *buf)
{
    DEBUG("recv_tcp_cb (%p, %ld, %p)\n", tcphandle, nread, buf);

    DEBUG("buf->base = %p  buf->len = %lu\n", buf->base, buf->len);
    /* hexdump ("buf->base", buf->base, 64);  */

    dest_t *d = tcphandle->data;
    DEBUG("d = %p\n", d);

    if (nread < 0 && nread != UV_EOF) {
        DEBUG("recv_tcp_cb() --> %s\n", uv_strerror(nread));
        free(buf->base);
        free_dest(d);
        return;
    }

    if (nread == 0) {
        /* similar to EAGAIN or EWOULDBLOCK according to libuv manual */
        DEBUG("recv_tcp_cb() --> no-op\n");
        free(buf->base);
        return;
    }

    /* else nread > 0: we have successfully received some bytes
     * or nread == UV_EOF: the other side closed the connection
       note that any freeing of buf happens inside enq_uvbuf() or later */
    enq_element(d, buf, nread); /* append to list of bufs for this dest */

    if (d->state == TLSHANDSHAKE) {
        DEBUG(
            "recv_tcp_cb() just called enq_element() in TLSHANDSHAKE state \n");
        /* rely on internal gnutls_handshake() state machine to trigger
	   call to eat_uvbytes(); no way to force this */
        return;
    }

    if (d->state != TCPACTIVE) {
        /* used to be assert(d->state == TCPACTIVE) */
        DEBUG("Dest state is %s rather than TCPACTIVE; giving up\n",
              tcpstatename(d->state));
        return;
    }

    /* Do peeling off and decrypting on async thread to
       avoid blocking due to TLS */
    TLOG("CT_IN_RECV d=%p nread=%ld\n", d, (long)nread);
    uv_work_t *w = malloc(sizeof(uv_work_t));
    w->data      = d;
    uv_queue_work(codatunnel_main_loop, w, peeloff_and_decrypt, cleanup_work);
}

void async_send_codatunnel(uv_async_t *async)
{
    minicb_pipe_req_t *req;
    int rc;

    /* pop request off the queue */
    while (1) {
        uv_mutex_lock(&async_forward_mutex);
        req                = async_forward.data;
        async_forward.data = req ? req->qnext : NULL;
        uv_mutex_unlock(&async_forward_mutex);

        if (!req) /* queue emptied, nothing to do */
            return;

        {
            ctp_t *p = (ctp_t *)req->msg.base;
            TLOG("CT_FWD_VSIDE op=%u len=%u\n", (unsigned)p->opcode,
                 (unsigned)req->msg.len);
        }
        /* forward packet to venus/codasrv */
        rc = uv_write(&req->req, (uv_stream_t *)&codatunnel, &req->msg, 1,
                      minicb_pipe);
        if (rc) {
            /* unable to forward packet from tcp connection to venus/codasrv */
            ERROR("uv_write(): rc = %d\n", rc);
            minicb_pipe(&req->req, rc);
        }
    }
}

static void recv_udpsocket_cb(uv_udp_t *udpsocket, ssize_t nread,
                              const uv_buf_t *buf, const struct sockaddr *addr,
                              unsigned flags)
{
    minicb_pipe_req_t *req;
    uv_buf_t msg[2];
    int rc;

    DEBUG("packet received from udpsocket nread=%ld buf=%p addr=%p flags=%u\n",
          nread, buf ? buf->base : NULL, addr, flags);

    if (nread == UV_ENOBUFS)
        return;

    if (nread < 0) {
        /* I believe recoverable errors should be handled by libuv. -JH */
        /* if we close the udp listen socket, we might just as well stop */
        uv_stop(codatunnel_main_loop);
        uv_close((uv_handle_t *)udpsocket, NULL);
        free(buf->base);
        return;
    }

    if (nread == 0) {
        free(buf->base);
        return;
    }

    req = malloc(sizeof(*req));
    if (!req) {
        /* unable to allocate, free buffers and continue, the other side will
         * assume the packet was dropped and retry in a bit */
        ERROR("malloc() failed\n");
        free(buf->base);
        return;
    }

    msg[0]           = uv_buf_init((char *)&req->ctp, sizeof(ctp_t));
    req->ctp.addrlen = sockaddr_len(addr);
    memcpy(&req->ctp.addr, addr, req->ctp.addrlen);
    req->ctp.msglen   = nread;
    req->ctp.is_retry = req->ctp.opcode = 0;
    strncpy(req->ctp.magic, "magic01", 8);

    /* move buffer from reader to writer */
    msg[1] = uv_buf_init(buf->base, nread);

    /* make sure the buffer is released when the send completes */
    req->req.data = buf->base;

    /* forward packet to venus/codasrv */
    rc = uv_write(&req->req, (uv_stream_t *)&codatunnel, msg, 2, minicb_pipe);
    if (rc) {
        /* unable to forward packet from udp socket to venus/codasrv.
         * free buffers and continue, the other side will assume the packet
         * was dropped and retry in a bit */
        ERROR("uv_write(): rc = %d\n", rc);
        free(req);
        free(buf->base);
    }
}

static void tcp_newconnection_cb(uv_stream_t *bindhandle, int status)
{
    uv_tcp_t *clienthandle;
    struct sockaddr_storage peeraddr;
    int peerlen, rc;
    dest_t *d;

    DEBUG("bindhandle = %p, status = %d)\n", bindhandle, status);
    if (status != 0) {
        DEBUG("tcp_newconnection_cb() --> %s\n", uv_strerror(status));
        return;
    }

    /* clienthandle can't be local because its lifetime extends
       beyond this call; I haven't carefully thought through any possible
       memory leaks due to this malloc (Satya, 3/22/2018) */
    clienthandle = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));

    uv_tcp_init(codatunnel_main_loop, clienthandle);
    rc = uv_accept(bindhandle, (uv_stream_t *)clienthandle);
    DEBUG("uv_accept() --> %d\n", rc);
    if (rc < 0) {
        DEBUG("uv_accept() --> %s\n", uv_strerror(rc));
        free(clienthandle);
        return;
    }

    /* Figure out identity of new client and create dest structure */
    peerlen = sizeof(peeraddr);

    rc = uv_tcp_getpeername(clienthandle, (struct sockaddr *)&peeraddr,
                            &peerlen);
    DEBUG("uv_tcp_getpeername() --> %d\n", rc);
    if (rc < 0) {
        DEBUG("uv_tcp_getpeername() --> %s\n", uv_strerror(rc));
        uv_close((uv_handle_t *)clienthandle, free_tcphandle);
        return;
    }

    d = getdest(&peeraddr, peerlen);
    if (!d) { /* new destination */
        d = createdest(&peeraddr, peerlen, NULL);
    }

    /* Bind this TCP handle and dest */
    clienthandle->data = d;

    d->tcphandle = clienthandle;
    /* all other fields of *d set by cleardest() in createdest() */
    d->state = TLSHANDSHAKE;

    /* disable Nagle */
    uv_tcp_nodelay(d->tcphandle, 1);
    uv_tcp_keepalive(d->tcphandle, 1, CT_TCP_KEEPALIVE_IDLE);

    /* now start receiving data on this TCP connection */
    DEBUG("About to call uv_read_start()\n");
    rc = uv_read_start((uv_stream_t *)d->tcphandle, alloc_cb, recv_tcp_cb);
    DEBUG("uv_read_start() --> %d\n", rc);

    /* Prepare and launch TLS setup;
      ap can't be a local variable because their lifetime is longer
      than this function; */

    async_tls_parms_t *ap = malloc(sizeof(async_tls_parms_t));

    ap->d = d;
    if (codatunnel_I_am_server) {
        DEBUG("codatunnel_I_am_server\n");
        ap->tlsflags = (GNUTLS_SERVER | GNUTLS_NONBLOCK);
    } else {
        DEBUG("codatunnel_I_am_client\n");
        ap->tlsflags = (GNUTLS_CLIENT | GNUTLS_NONBLOCK);
    }

    ap->certverify = IGNORE;
    ap->work.data  = ap;

    DEBUG("about to call uv_queue_work()");
    rc = uv_queue_work(codatunnel_main_loop, &ap->work, setuptls, cleanup_work);
    DEBUG("after call to uv_queue_work()  -> %d\n", rc);
}

static char *path_join(const char *dir, const char *file)
{
    unsigned int pathlen = strlen(dir) + strlen(file) + 2; /* '/' and '\0' */
    char *path           = malloc(pathlen);
    int n;

    assert(path);
    n = snprintf(path, pathlen, "%s/%s", dir, file);
    assert(n >= 0 && n < pathlen);
    return path;
}

static void _cert_reload(uv_work_t *w)
{
    gnutls_certificate_credentials_t *sc = w->data;
    int rc;

    uv_rwlock_wrlock(&credential_load_lock);

    if (*sc) {
        gnutls_certificate_free_credentials(*sc);
        *sc = NULL;
    }

    rc = gnutls_certificate_allocate_credentials(sc);
    if (rc != GNUTLS_E_SUCCESS) {
        ERROR("gnutls_certificate_allocate_credentials() --> %d (%s)\n", rc,
              gnutls_strerror(rc));
        goto unlock_out;
    }
    DEBUG("gnutls_certificate_allocate_credentials successful\n");

    /* Trust dir of certificates is defined both for clients and
       servers; on servers these are needed for server-to-server
       communication such as directory resolution and update */
    rc = gnutls_certificate_set_x509_trust_dir(*sc, sslcert_dir,
                                               GNUTLS_X509_FMT_PEM);
    if (rc < 0) {
        ERROR("gnutls_certificate_set_x509_trust_dir() --> %d (%s)\n", rc,
              gnutls_strerror(rc));
        goto unlock_out;
    }
    DEBUG("gnutls_certificate_set_x509_trust_dir() --> %d\n", rc);

    /* gnutls_certificate_set_x509_trust_dir returns # of processed
     * certificates, but we are checking for GNUTLS_E_SUCCESS later. */
    rc = GNUTLS_E_SUCCESS;

    if (codatunnel_I_am_server) {
        /* Define where the server's private key can be found */
        char *mycrt = path_join(sslcert_dir, "server.crt");
        char *mykey = path_join(sslcert_dir, "server.key");

        rc = gnutls_certificate_set_x509_key_file(*sc, mycrt, mykey,
                                                  GNUTLS_X509_FMT_PEM);
        free(mykey);
        free(mycrt);

        if (rc != GNUTLS_E_SUCCESS) {
            ERROR("gnutls_certificate_set_x509_key_file() --> %d (%s)\n", rc,
                  gnutls_strerror(rc));
            goto unlock_out;
        }
        DEBUG("gnutls_certificate_set_x509_key_file() successful\n");
    }

unlock_out:
    if (rc != GNUTLS_E_SUCCESS && *sc) {
        gnutls_certificate_free_credentials(*sc);
        *sc = NULL;
    }
    uv_rwlock_wrunlock(&credential_load_lock);
}

static void cert_reload_credentials(gnutls_certificate_credentials_t *sc)
{
    /* Schedule a credential reload from a worker thread so that we don't block
     * the mainloop while waiting for the credential_load_lock. */
    uv_work_t *w = malloc(sizeof(uv_work_t));
    w->data      = sc;
    uv_queue_work(codatunnel_main_loop, w, _cert_reload, cleanup_work);
}

static void reload_signal_handler(uv_signal_t *handle, int signum)
{
    printf("codatunneld: reloading x509 certificates\n");
    fflush(stdout);

    gnutls_certificate_credentials_t *sc = handle->data;
    cert_reload_credentials(sc);
}

#define CERT_POLL_INTERVAL 30000 /* check server.crt every 30 seconds */
static void server_cert_poll_cb(uv_fs_poll_t *handle, int status,
                                const uv_stat_t *prev, const uv_stat_t *curr)
{
    /* path does not exist */
    if (status < 0)
        return;

    /* file created, but has no content yet */
    if (status == 0 && curr->st_size == 0)
        return;

    /* at this point the certificate should be updated */
    printf("codatunneld: reloading x509 certificates\n");
    fflush(stdout);

    gnutls_certificate_credentials_t *sc = handle->data;
    cert_reload_credentials(sc);
}

/* main routine of coda tunnel daemon */
void codatunneld(int codatunnel_sockfd, const char *tcp_bindaddr,
                 const char *udp_bindaddr, const char *bind_service,
                 int onlytcp, const char *sslcertdir)
{
    uv_getaddrinfo_t gai_req;
    const struct addrinfo *ai, gai_hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
        .ai_flags    = AI_PASSIVE,
    };
    int rc;

#define GNUTLSERROR(op, retcode)                                            \
    {                                                                       \
        ERROR("%s() --> %d (%s)\n", op, retcode, gnutls_strerror(retcode)); \
        assert(0);                                                          \
    }

    DEBUG("codatunneld: starting\n");

    fprintf(stderr, "codatunneld: starting\n");

    if (tcp_bindaddr)
        codatunnel_I_am_server = 1; /* remember who I am */
    if (onlytcp)
        codatunnel_onlytcp = 1; /* no UDP fallback */

    /* make sure that writing to closed pipes doesn't kill us */
    signal(SIGPIPE, SIG_IGN);

    /* copy sslcertdir */
    sslcert_dir = strdup(sslcertdir);

    uv_rwlock_init(&credential_load_lock);

    /* Define GNUTLS settings before libuv to avoid race condition */
    rc = gnutls_global_init();
    if (rc != GNUTLS_E_SUCCESS) {
        GNUTLSERROR("gnutls_global_init()", rc);
        exit(-1);
    }

    DEBUG("gnutls_global_init() successful\n");

    gnutls_global_set_log_level(1000); /* Only for debugging */

    /* since we're not fully up and running yet, run the initial certificate
     * load from the main thread instead of queueing for a worker. */
    uv_work_t w = { .data = &x509_cred };
    _cert_reload(&w);

    /* GNUTLS is done, proceed to set up libuv */

    codatunnel_main_loop = uv_default_loop();

    /* SIGHUP handler to reload certificates in /etc/coda/ssl */
    uv_signal_t reload_signal;
    uv_signal_init(codatunnel_main_loop, &reload_signal);
    reload_signal.data = &x509_cred;
    uv_signal_start(&reload_signal, reload_signal_handler, SIGHUP);

    /* setup poll handler to check for changes to /etc/coda/ssl/server.crt */
    uv_fs_poll_t server_cert_poll;
    uv_fs_poll_init(codatunnel_main_loop, &server_cert_poll);
    server_cert_poll.data = &x509_cred;

    char *server_cert_path = path_join(sslcert_dir, "server.crt");
    uv_fs_poll_start(&server_cert_poll, server_cert_poll_cb, server_cert_path,
                     CERT_POLL_INTERVAL);
    free(server_cert_path);

    /* setup remotedest array before any IP addresses are encountered */
    initdestarray(codatunnel_main_loop);

    /* bind codatunnel_sockfd: an IPC pipe (ipc=1) so the file fds the app
     * hands off via SCM_RIGHTS can be accepted with uv_accept. */
    uv_pipe_init(codatunnel_main_loop, &codatunnel, 1);
    rc = uv_pipe_open(&codatunnel, codatunnel_sockfd);
    if (rc) {
        ERROR("uv_pipe_open(): %s\n", uv_strerror(rc));
        exit(-1);
    }

    /* resolve the requested udp bind address */
    const char *node    = (udp_bindaddr && *udp_bindaddr) ? udp_bindaddr : NULL;
    const char *service = bind_service ? bind_service : "0";
    rc = uv_getaddrinfo(codatunnel_main_loop, &gai_req, NULL, node, service,
                        &gai_hints);
    if (rc < 0) {
        ERROR("uv_getaddrinfo() --> %s\n", uv_strerror(rc));
        exit(-1);
    }

    /* try to bind to any of the resolved addresses */
    uv_udp_init(codatunnel_main_loop, &udpsocket);
    for (ai = gai_req.addrinfo; ai != NULL; ai = ai->ai_next) {
        if (uv_udp_bind(&udpsocket, ai->ai_addr, 0) == 0)
            break;
    }
    if (!ai) {
        ERROR("uv_udp_bind() unsuccessful, exiting\n");
        exit(-1);
    } else
        uv_freeaddrinfo(gai_req.addrinfo);

    /* set up async callback for forwarding decrypted packets */
    uv_async_init(codatunnel_main_loop, &async_forward, async_send_codatunnel);
    uv_mutex_init(&async_forward_mutex);

    /* file-async funnel: the single worker->loop handoff. The TLS peel-off
     * worker reads inbound daemon->daemon records but may not touch the cookie
     * table, so it enqueues them here and the loop drains them (file_async_cb). */
    uv_async_init(codatunnel_main_loop, &file_async, file_async_cb);
    uv_mutex_init(&file_async_mutex);

    uv_read_start((uv_stream_t *)&codatunnel, alloc_cb, recv_codatunnel_cb);
    uv_udp_recv_start(&udpsocket, alloc_cb, recv_udpsocket_cb);

    if (codatunnel_I_am_server) {
        /* start listening for connect() attempts */
        const struct addrinfo gai_hints2 = {
            .ai_family   = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_flags    = AI_PASSIVE,
        };
        /* service was already set earlier */

        uv_tcp_init(codatunnel_main_loop, &tcplistener);

        /* try to bind to any of the resolved addresses */
        uv_getaddrinfo(codatunnel_main_loop, &gai_req, NULL, tcp_bindaddr,
                       service, &gai_hints2);
        for (ai = gai_req.addrinfo; ai != NULL; ai = ai->ai_next) {
            if (uv_tcp_bind(&tcplistener, ai->ai_addr, 0) == 0)
                break;
        }
        if (!ai) {
            ERROR("uv_tcp_bind() unsuccessful, exiting\n");
            exit(-1);
        } else
            uv_freeaddrinfo(gai_req.addrinfo);

        /* start listening for connect() attempts */
        uv_listen((uv_stream_t *)&tcplistener, 10, tcp_newconnection_cb);
    }

    /* run until the codatunnel connection closes */
    uv_run(codatunnel_main_loop, UV_RUN_DEFAULT);

    /* cleanup any remaining open handles */
    uv_fs_poll_stop(&server_cert_poll);
    uv_signal_stop(&reload_signal);

    uv_walk(codatunnel_main_loop, (uv_walk_cb)uv_close, NULL);
    uv_run(codatunnel_main_loop, UV_RUN_DEFAULT);
    uv_loop_close(codatunnel_main_loop);
    exit(0);

#undef GNUTLSERROR
}

/* from Internet example */
void hexdump(char *desc, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char *)addr;

    // Output description if given.
    if (desc != NULL)
        printf("%s:\n", desc);

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n", len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf("  %s\n", buff);

            // Output the offset.
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf("  %s\n", buff);
}

/* Sink-driven pull, source side. The sink asked for [offset, offset+len) of
 * our file. Serve it: pread up to min(len, what remains of our registered
 * length) at the requested offset and send CT_TRANSFER_DATA. When the bytes
 * served reach our registered length, send CT_TRANSFER_EOF (the definitive
 * end — never inferred from a short chunk) and report FILEDONE locally so
 * the app's finalize returns. A REQUEST that no longer has a live source
 * record (transfer already completed / record freed), or that targets a
 * driver-end (pump-driven) fetch source, is dropped. */
static void ct_transfer_request_arrived(const ct_async_req_t *r)
{
    struct ct_filerec *rec = ct_cookie_waiter(r->cookie);
    if (!rec || rec->role != CT_SOURCE || rec->is_driver)
        return; /* already done, not ours, or a fetch source: drop silently */

    uint64_t end  = rec->offset + rec->length;
    uint64_t want = (uint64_t)r->datalen;
    /* The sink asks for up to its byte quota without knowing how much we
     * actually have, so the final chunk legitimately over-requests; clamping
     * want below serves min(available, quota). Only a start past the end of
     * the registered range is a real overflow. */
    if (r->offset > rec->next)
        goto overflow;

    if (r->offset != rec->next) {
        ERROR("REQUEST: cookie %lu offset %lu != expected %lu\n",
              (unsigned long)r->cookie, (unsigned long)r->offset,
              (unsigned long)rec->next);
        goto error;
    }
    if (want > end - r->offset)
        want = end - r->offset;
    /* serve at most one TLS fragment: cap to the scratch size and to what this
     * channel's negotiated record max can carry, so the record is never split */
    if (want > CT_CHUNKMAX)
        want = CT_CHUNKMAX;
    if (want > (uint64_t)r->d->max_data_payload)
        want = (uint64_t)r->d->max_data_payload;
    if (want == 0)
        goto done; /* nothing left: the sink should already see EOF */

    char *scratch = malloc(CT_CHUNKMAX);
    if (!scratch)
        goto error;
    ssize_t n = pread(rec->fd, scratch, want, (off_t)r->offset);
    if (n <= 0) {
        free(scratch);
        goto error;
    }

    size_t total = sizeof(ctp_t) + sizeof(ct_transfer_data) + (size_t)n;
    char *pkt    = malloc(total);
    if (!pkt) {
        free(scratch);
        goto error;
    }
    ctp_t *h             = (ctp_t *)pkt;
    ct_transfer_data *td = (ct_transfer_data *)(pkt + sizeof(ctp_t));
    memset(pkt, 0, total);
    strncpy(h->magic, CT_MAGIC, sizeof(h->magic));
    h->opcode = CT_TRANSFER_DATA;
    h->msglen = (uint32_t)(sizeof(ct_transfer_data) + (uint32_t)n);
    memcpy(pkt + sizeof(ctp_t) + sizeof(ct_transfer_data), scratch, (size_t)n);
    td->cookie    = ct_hton64(r->cookie);
    td->offset    = ct_hton64(r->offset);
    rec->next     = r->offset + (uint64_t)n;
    uv_buf_t buft = uv_buf_init(pkt, total);
    TLOG("TCPFTP PULL DATA cookie=%lu off=%lu len=%lu\n",
         (unsigned long)r->cookie, (unsigned long)r->offset, (unsigned long)n);
    send_to_tcp_dest(r->d, (ssize_t)total, &buft, NULL);
    free(scratch);

    if (rec->next >= end)
        goto done;
    return;

done:
    /* served everything we registered: definitive end of the transfer */
    TLOG("TCPFTP PULL DONE cookie=%lu total=%lu\n", (unsigned long)r->cookie,
         (unsigned long)(rec->next - rec->offset));
    ct_send_transfer_eof(r->d, r->cookie, NULL);
    ct_push_filedone(r->cookie, CT_STATUS_SUCCESS, rec->next - rec->offset);
    ct_rec_free_now(r->cookie);
    return;

overflow:
    ERROR("REQUEST: cookie %lu range [%lu,%lu) exceeds [%lu,%lu)\n",
          (unsigned long)r->cookie, (unsigned long)r->offset,
          (unsigned long)r->offset + want, rec->offset, end);
    /* fall through */
error:
    if (r->d->state == TCPACTIVE)
        ct_send_transfer_error(r->d, r->cookie, CT_STATUS_IOERR);
    ct_push_filedone(r->cookie, CT_STATUS_IOERR, rec->next - rec->offset);
    ct_rec_free_now(r->cookie);
}
