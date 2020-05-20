/* BLURB lgpl

                           Coda File System
                              Release 8

          Copyright (c) 2017-2020 Carnegie Mellon University
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>
#include <gnutls/gnutls.h>

#include "codatunnel.private.h"

/* Global variables within codatunnel daemon */
static int codatunnel_I_am_server = 0; /* only clients initiate;
                                          only servers accept */
static int codatunnel_onlytcp     = 0; /* whether to use UDP fallback;
                                          default is yes */

static uv_loop_t *codatunnel_main_loop;
static uv_udp_t codatunnel; /* facing Venus or CodaSrv */
static uv_udp_t udpsocket; /* facing the network */
static uv_tcp_t tcplistener; /* facing the network, only on servers */

static uv_async_t async_forward;
static uv_mutex_t async_forward_mutex;

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

typedef struct gnutls_send_req {
    uv_work_t req;
    dest_t *dest;
    uv_buf_t buf;
    ssize_t len;
} gnutls_send_req_t;

/* forward refs for workhorse functions; many are cb functions */
static void recv_codatunnel_cb(uv_udp_t *, ssize_t, const uv_buf_t *,
                               const struct sockaddr *saddr, socklen_t slen);
static void send_to_udp_dest(ssize_t, const uv_buf_t *,
                             const struct sockaddr *saddr, socklen_t slen);
static void send_to_tcp_dest(dest_t *, ssize_t, const uv_buf_t *);
static void try_creating_tcp_connection(dest_t *);
static void recv_tcp_cb(uv_stream_t *, ssize_t, const uv_buf_t *);
static void recv_tcp_cb_handoff(dest_t *, void *, size_t);
static void tcp_connect_cb(uv_connect_t *, int);
static void recv_udpsocket_cb(uv_udp_t *, ssize_t, const uv_buf_t *,
                              const struct sockaddr *, unsigned);
static void tcp_newconnection_cb(uv_stream_t *, int);

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
    dest_t *d             = req->dest;

    DEBUG("minicb_tcp(%p, %p, %d)\n", arg, arg->data, status);

    if (status == 0)
        d->packets_sent += req->msglen;

    req->write_status = status;
    uv_sem_post(&req->write_done);
}

/* called when we're about to free dest_t, dequeue any pending writes */
void drain_outbound_queue(dest_t *d)
{
    minicb_tcp_req_t *mtr;

    uv_mutex_lock(&d->outbound_mutex);
    while (d->outbound_queue) {
        mtr               = d->outbound_queue;
        d->outbound_queue = mtr->qnext;
        minicb_tcp(&mtr->req, -EPIPE);
    }
    uv_mutex_unlock(&d->outbound_mutex);
}

static void recv_codatunnel_cb(uv_udp_t *codatunnel, ssize_t nread,
                               const uv_buf_t *buf, const struct sockaddr *addr,
                               unsigned flags)
{
    static unsigned empties;

    DEBUG("packet received from codatunnel nread=%ld buf=%p addr=%p flags=%u\n",
          nread, buf ? buf->base : NULL, addr, flags);

    if (nread == UV_ENOBUFS)
        return;

    if (nread == 0) {
        /* empty packet received, we normally get this after we've drained any
         * pending data from the socket after a wakeup. But we also see these
         * when the other end of a socketpair was closed. Differentiate by
         * counting how many successive empties we get. --JH */
        if (++empties >= 3) {
            DEBUG("codatunnel closed\n");
            uv_stop(codatunnel_main_loop);
            uv_close((uv_handle_t *)codatunnel, NULL);
        }
        free(buf->base);
        return;
    }
    empties = 0;

    if (nread < 0) {
        /* We shouldn't see read errors on the codatunnel socketpair. -JH */
        /* if we close the socketpair endpoint, we might just as well stop */
        uv_stop(codatunnel_main_loop);
        uv_close((uv_handle_t *)codatunnel, NULL);
        free(buf->base);
        return;
    }
    if (nread < sizeof(ctp_t)) {
        DEBUG("short packet received from codatunnel\n");
        free(buf->base);
        return;
    }

    /* We have a legit packet; it was already been read into buf before this
     * upcall was invoked by libuv */

    ctp_t *p = (ctp_t *)buf->base;

    if (nread != (sizeof(ctp_t) + p->msglen)) {
        DEBUG("incomplete packet received from codatunnel\n");
        free(buf->base);
        return;
    }

    dest_t *d = getdest(&p->addr, p->addrlen);

    /* Try to establish a new TCP connection for future use;
     * do this only once per INIT1 (avoiding retries) to avoid TCP SYN flood;
     * Only clients should attempt this, because of NAT firewalls */
    if (p->is_init1 && !p->is_retry && !codatunnel_I_am_server) {
        if (!d) /* new destination */
            d = createdest(&p->addr, p->addrlen);

        if (d->state == ALLOCATED) {
            d->state = TCPATTEMPTING;
            try_creating_tcp_connection(d);
        }
    }

    /* what do we do with packet p for destination d? */

    if (d && (d->state == TCPACTIVE)) {
        /* Changed this to always send retries so we get RPC2_BUSY as a keep
         * alive on long running operations -JH */
        if (0) { // p->is_retry && (d->packets_sent > 0)) {
            /* drop retry packet;
               only exception is when nothing has yet been sent on new TCP
               connection; the state may have become TCPACTIVE after most
               recent retry;  those earlier retries via UDP may all have
               been lost (e.g., because of firewall settings at dest);
               make sure you send at least this one; all future
               retries will be dropped;  no harm if earlier retries got through
               (Satya, 1/20/2018)
            */
            free(buf->base);
        } else {
            send_to_tcp_dest(d, nread, buf);
            /* free buf in cascaded cb */
        }
    }
    /* Possibile states for destination d: ALLOCATED, TCPATTEMPTING, and
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
        send_to_udp_dest(nread, buf, addr, flags);
        /* free buf only in cascaded cb */
    } else
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

/* worker thread function to send packet with TLS */
static void _send_to_tls_dest(uv_work_t *req)
{
    gnutls_send_req_t *w = (gnutls_send_req_t *)req->data;
    dest_t *d            = w->dest;
    int rc;

    if (d->state != TCPACTIVE || !d->my_tls_session) {
        ERROR("about to send packet, but we have no active tls session\n");
        return;
    }

    DEBUG("About to call gnutls_record_send()\n");
    rc = gnutls_record_send(d->my_tls_session, w->buf.base, w->len);
    DEBUG("Just returned from gnutls_record_send()\n");
    /* actual sending of bytes happens in upcall of above  */

    if (rc != w->len) { /* something went wrong */
        ERROR("gnutls_record_send(): rc = %d\n", rc);
    }
}

static void _send_to_tls_done(uv_work_t *req, int status)
{
    gnutls_send_req_t *w = (gnutls_send_req_t *)req->data;
    free(w->buf.base);
    free(w);
}

/* To accommodate TLS, send_to_tcp_dest() has been split;
   top half invokes TLS; upcall from TLS engine invokes bottom half which
   does the actual sending on TCP */
static void send_to_tcp_dest(dest_t *d, ssize_t nread, const uv_buf_t *buf)
{
    DEBUG("send_to_tcp_dest(%p, %ld, %p)\n", d, nread, buf);

    /* Convert ctp_d fields to network order, before encryption */
    ctp_t *p = (ctp_t *)buf->base;
    DEBUG("is_retry = %u  is_init1 = %u  msglen = %u\n", p->is_retry,
          p->is_init1, p->msglen);

    p->is_retry = htonl(p->is_retry);
    p->is_init1 = htonl(p->is_init1);
    p->msglen   = htonl(p->msglen);
    /* ignoring addr and addrlen; will be clobbered by dest_t->destaddr and
     * dest_t->destlen on the other side of the tunnel */

    /* We assume that TLS is in use; code does not work without TLS
       Using asserts for now; perhaps we need to be less brutal? */

    assert(d->my_tls_session);
    assert(d->state == TCPACTIVE); /* never reach here in TLSHANDSHAKE */

    /* Do gnutls operations on separate thread to avoid blocking
     * due to TLS */
    gnutls_send_req_t *w = malloc(sizeof(gnutls_send_req_t));
    w->dest              = d;
    w->buf               = uv_buf_init(buf->base, buf->len);
    w->len               = nread;
    w->req.data          = w;
    uv_queue_work(codatunnel_main_loop, &w->req, _send_to_tls_dest,
                  _send_to_tls_done);
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
    DEBUG("waking outbound_worker %p\n", &d->outbound_worker);
    uv_async_send(&d->outbound_worker);
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
        }
    }
}

static void cleanup_work(uv_work_t *w, int status)
{
    free(w);
}

static void free_tcphandle(uv_handle_t *handle)
{
    free(handle);
}

static void setuptls(uv_work_t *w)
{
    /*
       setuptls() is done in a separate thread in the uv thread pool,
       via uv_queue_work(),  because of blocking in handshake;
       w->data is really of type (async_tls_parms_t *ap);

       Encapsulate all the set up of TLS for destination ap->d;
       ap->certverify  indicates whether setup includes verification of peer
       identify Coda clients always insist on verifying server's identity;
       Coda servers don't care about Coda client identity;
       However, Coda servers connect to each other for resolution, etc.  and
       in those cases, both sides verify the other's identitify.
    */

    int rc;

    async_tls_parms_t *ap        = (async_tls_parms_t *)w->data;
    dest_t *d                    = ap->d;
    gnutls_init_flags_t tlsflags = ap->tlsflags;
    peercheck_t certverify       = ap->certverify;

    /* make sure credentials are not destroyed/reloaded during the handshake */
    uv_rwlock_rdlock(&credential_load_lock);

    /* local TLS setup errors, fall back on UDP connection */
#define GNUTLSERROR(op, retcode)                                       \
    {                                                                  \
        ERROR("%s(%s) --> %d (%s)\n", op, d->fqdn, retcode,            \
              gnutls_strerror(retcode));                               \
        if (d->state != TCPCLOSING) {                                  \
            if (certverify == IGNORE) {                                \
                free_dest(d);                                          \
            } else {                                                   \
                d->state = ALLOCATED;                                  \
                uv_read_stop((uv_stream_t *)d->tcphandle);             \
                uv_close((uv_handle_t *)d->tcphandle, free_tcphandle); \
                d->tcphandle = NULL;                                   \
            }                                                          \
        }                                                              \
        uv_rwlock_rdunlock(&credential_load_lock);                     \
        return;                                                        \
    }

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

    if (certverify == IGNORE) { /* don't bother checking client identity */
        gnutls_certificate_server_set_request(d->my_tls_session,
                                              GNUTLS_CERT_IGNORE);
    } else { /* I am a server; verify peer identify */
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
    DEBUG("About to do gnutls_handshake(%s)\n", d->fqdn);
eagain:
    rc = gnutls_handshake(d->my_tls_session);

    if (rc == GNUTLS_E_INTERRUPTED || rc == GNUTLS_E_AGAIN)
    /* || rc == GNUTLS_E_WARNING_ALERT_RECEIVED */
    {
        DEBUG("gnutls_handshake(%s) got non-fatal error, trying again\n",
              d->fqdn);
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

    DEBUG("gnutls_handshake(%s) successful\n", d->fqdn);
    d->certvalidation_failed = 0;
    d->state = TCPACTIVE; /* commit point for encrypted TCP tunnel */
    uv_rwlock_rdunlock(&credential_load_lock);
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
        return;
    }

    /* TCP connection successful */
    DEBUG("tcp_connect_cb(%p, %d) --> %p\n", d, status, d->tcphandle);
    d->tcphandle->data = d; /* point back, for use in upcalls */
    d->uvcount         = 0;
    d->uvoffset        = 0;
    for (i = 0; i < UVBUFLIMIT; i++) {
        ((d->enqarray)[i].b).base = NULL;
        ((d->enqarray)[i].b).len  = 0;
        (d->enqarray)[i].numbytes = 0;
    }
    d->decrypted_record = NULL;
    d->packets_sent     = 0;
    d->state            = TLSHANDSHAKE;

    /* disable Nagle */
    uv_tcp_nodelay(d->tcphandle, 1);

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
    if (!d->decrypted_record) {
        DEBUG("Allocating d->decrypted_record\n");
        d->decrypted_record = malloc(MAXRECEIVE); /* space for next record */
        if (!d->decrypted_record) {
            ERROR("malloc() failed\n");
            free_dest(d);
            return;
        }
    }
    /* else partially assembled TLS record already exists; just extend it */

    /* Assemble at most one gnutls_record at a time */
    uv_mutex_lock(&d->tls_receive_record_mutex);

    /* We use d->uvcount only as advisory information here;
       Don't bother with mutex because eat_uv_bytes() called
       by gnutls_record_recv() does the definitive check
    */
    DEBUG("d->uvcount = %d\n", d->uvcount);
    while (d->uvcount) {
        /* bytes still left to be consumed by eat_uvbytes() */
        DEBUG("d->uvcount = %d\n", d->uvcount);

        /* Try to peel off a complete encrypted record */
        DEBUG("About to call gnutls_record_recv()\n");
        int rc = gnutls_record_recv(d->my_tls_session, d->decrypted_record,
                                    MAXRECEIVE);
        DEBUG("Just returned from gnutls_record_recv(), rc = %d\n", rc);

        if (rc == GNUTLS_E_INTERRUPTED) {
            DEBUG("gnutls_record_recv() --> GNUTLS_E_INTERRUPTED\n");
            continue; /* as if nothing had happened */
        }

        if (rc == GNUTLS_E_AGAIN) {
            /* eat_uvbytes() ran out of bytes;
               need to continue when more bytes are received in next uv upcall;
               leave current d->decrypted_record undisturbed for continuation
            */
            goto unlock_out;
        }
        if (rc <= 0) { /* something went wrong */
            ERROR("gnutls_record_recv(): rc = %d\n", rc);
            d->state = TCPCLOSING; /* all hope is lost */
            goto unlock_out;
        }
        if (rc >= MAXRECEIVE) {
            ERROR("Monster packet: gnutls_record_recv() --> %lu\n", MAXRECEIVE);
            d->state = TCPCLOSING; /* all hope is lost */
            goto unlock_out;
        }

        /* Yay!  We have a complete gnutls record of length rc;
           Hand it off to codasrv/Venus;
           Then carry on with this loop */

        DEBUG("Yay!  we have a complete gnutls record of length %d\n", rc);

        /* Unhook this record from d */
        void *justreceived = d->decrypted_record;

        /* Now allocate a new buffer for next gnutls record */
        d->decrypted_record = malloc(MAXRECEIVE); /* clobber */
        if (!d->decrypted_record) {
            ERROR("malloc() failed\n");
            d->state = TCPCLOSING;
            goto unlock_out;
        }
        recv_tcp_cb_handoff(d, justreceived, rc);
        /* free of record just received  will happen in handoff code */
    }

unlock_out:
    uv_mutex_unlock(&d->tls_receive_record_mutex);
}

static void recv_tcp_cb(uv_stream_t *tcphandle, ssize_t nread,
                        const uv_buf_t *buf)
{
    DEBUG("recv_tcp_cb (%p, %d, %p)\n", tcphandle, (int)nread, buf);

    DEBUG("buf->base = %p  buf->len = %lu\n", buf->base, buf->len);
    /* hexdump ("buf->base", buf->base, 64);  */

    dest_t *d = tcphandle->data;
    DEBUG("d = %p\n", d);

    if (nread < 0) {
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

    /* else nread > 0: we have successfully received some bytes;
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
    uv_work_t *w = malloc(sizeof(uv_work_t));
    w->data      = d;
    uv_queue_work(codatunnel_main_loop, w, peeloff_and_decrypt, NULL);
}

static void recv_tcp_cb_handoff(dest_t *d, void *received_packet, size_t nread)
{ /* received_packet points to decrypted packet of length nread;
     prep it, and then hand it off to codasrv/Venus */
    minicb_udp_req_t *req;

    /* Replace recipient address with sender's address, so that
       recvfrom() can provide the "from" address. */
    ctp_t *p = (ctp_t *)received_packet;
    memcpy(&p->addr, &d->destaddr, d->destlen);
    p->addrlen = d->destlen;

    /* Prepare to send  */
    req = malloc(sizeof(*req));
    if (!req) {
        /* unable to allocate, free buffers and trigger a disconnection
         * because we have no other way to force a retry. */
        ERROR("malloc() failed\n");
        free(received_packet);
        free_dest(d);
        return;
    }

    req->msg = uv_buf_init(received_packet, nread); /* to send */

    /* make sure the buffer is released when the send completes */
    req->req.data = received_packet;
    req->qnext    = NULL;

    /* append packet to queue of pending packets */
    uv_mutex_lock(&async_forward_mutex);
    minicb_udp_req_t **q = (minicb_udp_req_t **)&async_forward.data;
    while (*q != NULL)
        q = &(*q)->qnext;
    *q = req;
    uv_mutex_unlock(&async_forward_mutex);

    /* signal mainloop to send this packet */
    uv_async_send(&async_forward);
}

void async_send_codatunnel(uv_async_t *async)
{
    minicb_udp_req_t *req;
    struct sockaddr_in dummy_peer = {
        .sin_family = AF_INET,
    };
    int rc;

    /* pop request off the queue */
    while (1) {
        uv_mutex_lock(&async_forward_mutex);
        req                = async_forward.data;
        async_forward.data = req ? req->qnext : NULL;
        uv_mutex_unlock(&async_forward_mutex);

        if (!req) /* queue emptied, nothing to do */
            return;

        /* forward packet to venus/codasrv */
        rc = uv_udp_send(&req->req, &codatunnel, &req->msg, 1,
                         (struct sockaddr *)&dummy_peer, minicb_udp);

        DEBUG("codatunnel.send_queue_count = %lu\n",
              codatunnel.send_queue_count);
        if (rc) {
            /* unable to forward packet from tcp connection to venus/codasrv */
            ERROR("uv_udp_send(): rc = %d\n", rc);
            minicb_udp(&req->req, rc);
        }
    }
}

static void recv_udpsocket_cb(uv_udp_t *udpsocket, ssize_t nread,
                              const uv_buf_t *buf, const struct sockaddr *addr,
                              unsigned flags)
{
    minicb_udp_req_t *req;
    uv_buf_t msg[2];
    struct sockaddr_in dummy_peer = {
        .sin_family = AF_INET,
    };
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
    req->ctp.is_retry = req->ctp.is_init1 = 0;
    strncpy(req->ctp.magic, "magic01", 8);

    /* move buffer from reader to writer */
    msg[1] = uv_buf_init(buf->base, nread);

    /* make sure the buffer is released when the send completes */
    req->req.data = buf->base;

    /* forward packet to venus/codasrv */
    rc = uv_udp_send((uv_udp_send_t *)req, &codatunnel, msg, 2,
                     (struct sockaddr *)&dummy_peer, minicb_udp);
    DEBUG("codatunnel.send_queue_count = %lu\n", codatunnel.send_queue_count);
    if (rc) {
        /* unable to forward packet from udp socket to venus/codasrv.
         * free buffers and continue, the other side will assume the packet
         * was dropped and retry in a bit */
        ERROR("uv_udp_send(): rc = %d\n", rc);
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
        return;
    }

    /* Figure out identity of new client and create dest structure */
    peerlen = sizeof(peeraddr);

    rc = uv_tcp_getpeername(clienthandle, (struct sockaddr *)&peeraddr,
                            &peerlen);
    DEBUG("uv_tcp_getpeername() --> %d\n", rc);
    if (rc < 0) {
        DEBUG("uv_tcp_getpeername() --> %s\n", uv_strerror(rc));
        return;
    }

    d = getdest(&peeraddr, peerlen);
    if (!d) { /* new destination */
        d = createdest(&peeraddr, peerlen);
    }

    /* Bind this TCP handle and dest */
    clienthandle->data = d;

    d->tcphandle = clienthandle;
    /* all other fields of *d set by cleardest() in createdest() */
    d->state = TLSHANDSHAKE;

    /* disable Nagle */
    uv_tcp_nodelay(d->tcphandle, 1);

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

/* barrier wait to make sure gnutls_handshake has finished */
void wait_for_handshakes(void)
{
    uv_rwlock_wrlock(&credential_load_lock);
    uv_rwlock_wrunlock(&credential_load_lock);
}

static void cert_reload_credentials(gnutls_certificate_credentials_t *sc)
{
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

static void reload_signal_handler(uv_signal_t *handle, int signum)
{
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

    cert_reload_credentials(&x509_cred);

    /* GNUTLS is done, proceed to set up libuv */

    codatunnel_main_loop = uv_default_loop();

    /* setup SIGHUP handler to reload x509 credentials */
    uv_signal_t reload_signal;
    uv_signal_init(codatunnel_main_loop, &reload_signal);
    reload_signal.data = &x509_cred;
    uv_signal_start(&reload_signal, reload_signal_handler, SIGHUP);

    /* do this before any IP addresses are encountered */
    initdestarray(codatunnel_main_loop);

    /* bind codatunnel_sockfd */
    uv_udp_init(codatunnel_main_loop, &codatunnel);
    uv_udp_open(&codatunnel, codatunnel_sockfd);

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

    uv_udp_recv_start(&codatunnel, alloc_cb, recv_codatunnel_cb);
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
