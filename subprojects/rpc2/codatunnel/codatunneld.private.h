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

#ifndef _CODATUNNELD_PRIVATE_H_
#define _CODATUNNELD_PRIVATE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <sys/time.h>
#include <stdlib.h>
#include <uv.h>
#include <gnutls/gnutls.h>

#include "ctp.h"

int mapthread(uv_thread_t);

#if 0 /* per-packet/per-record trace; off by default (see TLOG below) */
#define DEBUG(...)                                                      \
    do {                                                                \
        struct timeval tt;                                              \
        gettimeofday(&tt, 0);                                           \
        int myid = mapthread(uv_thread_self());                         \
        fprintf(stderr, "[%d] %ld.%06ld %s:%d ", myid, (long)tt.tv_sec, \
                (long)tt.tv_usec, __FUNCTION__, __LINE__);              \
        fprintf(stderr, __VA_ARGS__);                                   \
        fflush(stderr);                                                 \
    } while (0)
#else
#define DEBUG(...)
#endif

#define ERROR(...)                                         \
    do {                                                   \
        fprintf(stderr, "%s:%d ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                      \
        fflush(stderr);                                    \
    } while (0)

/* Timeline log for the daemon-driven file transfer and the per-packet tunnel
 * trace. Gated behind the CODATUNNEL_TLOG env var (default OFF) so a normal
 * build is silent and disk-safe; set CODATUNNEL_TLOG to any value to turn on
 * the full trace for a debugging session (no rebuild needed to toggle). Every
 * line is prefixed with a gettimeofday stamp (sec.usec) so client and server
 * daemon logs can be turned into timelines and correlated. */
static inline int ct_tlog_enabled(void)
{
    static int en = -1;

    if (en < 0)
        en = (getenv("CODATUNNEL_TLOG") != NULL);
    return en;
}

#define TLOG(...)                                                             \
    do {                                                                      \
        if (ct_tlog_enabled()) {                                              \
            struct timeval tt;                                                \
            gettimeofday(&tt, 0);                                             \
            fprintf(stderr, "%ld.%06ld ", (long)tt.tv_sec, (long)tt.tv_usec); \
            fprintf(stderr, __VA_ARGS__);                                     \
            fflush(stderr);                                                   \
        }                                                                     \
    } while (0)

/* the actual tunnel daemon (defined in codatunneld.c) */
void codatunneld(int codatunnel_sockfd, const char *tcp_bindaddr,
                 const char *udp_bindaddr, const char *bind_service,
                 int onlytcp, const char *sslcertdir) __attribute__((noreturn));

/* Format of encapsulated UDP packets sent on Unix domain connections
   (i.e., between Venus and codatunneld, and between codasrv and
   codatunneld.)  All fields are in the clear. This header is followed
   by `msglen` encrypted bytes of the RPC2 packet that is sent or
   received.  On the network, this header is NOT sent in UDP packets; but
   it IS sent in TCP-tunneled packets
*/
/* The framing struct ctp_t, the ct_opcode enum, and CT_MAX_RECORD live in
   "ctp.h" (pure wire protocol; no tunnel dependencies). */

/* Transitions always:  FREE --> ALLOCATED --> (optionally)TCPATTEMPTING -->
 * TLSHANDSHAKE --> TCPACTIVE --> TCPCLOSING --> FREE */
enum deststate
{
    FREE          = 0, /* this entry is not allocated */
    ALLOCATED     = 1, /* entry allocated, but TCP is not active; UDP works */
    TCPATTEMPTING = 2, /* entry allocated, tcp connect is being attempted;
                          UDP works */
    TLSHANDSHAKE  = 3, /* tcp connection is good; TLS handshake in progress */
    TCPACTIVE     = 4, /* entry allocated, its tcphandle is good, and TLS
                          handshake successful */
    TLSERROR      = 5, /* an error occurred in TLS worker threads */
    TCPCLOSING    = 6, /* now closing, and waiting to become FREE */
};

const char *tcpstatename(enum deststate);

typedef struct {
    uv_buf_t b; /* b.len is max size of buffer, not useful bytes */
    int numbytes; /* number of useful bytes pointed to by b->base */
} enq_data_t;

typedef struct remotedest {
    struct sockaddr_storage destaddr;
    socklen_t destlen;
    const char *fqdn; /* passed by INIT0 packet on client, NULL on server */

    enum deststate state; /* All destinations are assumed to be capable of
                             becoming TCPACTIVE; Setting TCPACTIVE should be a
                             commit point: all fields below should have been
                             set before that happens, to avoid race conditions */
    char certvalidation_failed; /* when certificate validation fails we
                                   suppress UDP, but will retry TLS connections
                                   for INIT0 packets*/

    uv_tcp_t *tcphandle; /* only valid if state is TCPACTIVE or TLSHANDSHAKE */

    gnutls_session_t my_tls_session;
    size_t max_data_payload; /* largest CT_TRANSFER_DATA payload this session's
                              * negotiated TLS record max can carry:
                              * gnutls_record_get_max_size() minus the ctp_t and
                              * ct_transfer_data headers. Captured at handshake
                              * (before TCPACTIVE); a sender caps each record to
                              * min(CT_CHUNKMAX, this) so it fits in one fragment */
    char *decrypted_record; /* pointer to malloced array of size CT_MAX_RECORD;
			     filled by gnutls_record_recv() by reassembly from
                             calls to eat_uvbytes(); must be preserved across
			     successive calls to gnutls_record_recv() for
			     reassembly to work properly */

    /* Space to buffer packets from recv_tcp_cb() en route to
       gnutls_record_recv(). enq_uvbuf() appends packets to this list.
       eat_uvbytes() peels off bytes in these packets.  We use a simple
       array, because we don't expect this queue to get very long.  Most
       common case will be an exact match: one input packet waiting,
       that is completely consumed in one call.  If the future proves
       otherwise, change to a linked list structure instead of array. */

#define UVBUFLIMIT 10 /* drop packets beyond this limit */
    enq_data_t enqarray[UVBUFLIMIT]; /* array of structures */
    int uvcount; /* how many elements in use in above array */
    uv_mutex_t uvcount_mutex; /* protects uvcount */
    uv_cond_t uvcount_nonzero; /* signaled when uvcount goes above zero */
    int uvoffset; /* array index of next unused byte in ((enqarray[0].b)->base[]  */

    /* Mutexes below ensure that only one gnutls_recv_record() and
     one gnutls_send_record() can be in progress at a time; this is needed because
     gnutls serializes data records on the TCP stream; currently, this appears to be
     a 5-byte header that indicates a length, followed by that many bytes; however,
     this may change in the future to be something more complex; use of a mutex eliminates
     dependence on the exact serialization format; it is essential that
     all the pieces of  a serialized record appear consecutively in the TCP stream;
     interleaving in an multi-threaded environment could be disastrous */
    uv_mutex_t tls_receive_record_mutex;
    uv_mutex_t tls_send_mutex;
    /* Guards the gnutls session itself: gnutls is not thread-safe, so
     * gnutls_record_send() (worker thread) and gnutls_record_recv()
     * (peel-off worker thread) must never run concurrently on the same
     * session, even though the two record mutexes above are distinct */
    uv_mutex_t tls_session_mutex;
    void *tls_send_queue;

    uv_async_t wakeup; /* wakeup for outgoing packets or dest_t teardown */
    struct minicb_tcp_req *outbound_queue;
    uv_mutex_t outbound_mutex;

    /* Receive backpressure (TCPACTIVE only): when the enqarray recv queue
     * fills, we stop pulling from the socket (so it stays in the kernel
     * buffer and pushes back on the writer) instead of dropping; when the
     * queue drains to zero the thread pool resumes the read. read_paused and
     * resume_read are guarded by uvcount_mutex. */
    int read_paused; /* read stopped because recv queue is full */
    uv_async_t resume_read; /* queue drained -> loop resumes uv_read_start */
    /* Restart file transfers deferred while this channel was handshaking.
     * Sent from setuptls() (a worker) once the channel commits TCPACTIVE; the
     * callback runs on the loop because starting a pump is loop-only. */
    uv_async_t redrive;
} dest_t;

static inline void free_tcphandle(uv_handle_t *handle)
{
    free(handle);
}

void outbound_worker_cb(uv_async_t *async);
void resume_read_cb(uv_async_t *async);
void ct_redrive_cb(uv_async_t *async);

/* Stuff for destination management */
void initdestarray(uv_loop_t *mainloop);
dest_t *getdest(const struct sockaddr_storage *, socklen_t);
dest_t *createdest(const struct sockaddr_storage *, socklen_t,
                   const char *peername);
void free_dest(dest_t *d);

/* Procedures to add and remove buffered data from a dest.
   These operate in producer-consumer manner.
   recv_tcp_cb() calls enq_uvbuf() as producer.
   gnutls_record_recv() calls eat_uvbytes() as consumer.
   During TLS handshake, eat_uvbytes() is also called as consumer.
*/
void enq_element(dest_t *, const uv_buf_t *, int);
ssize_t eat_uvbytes(gnutls_transport_ptr_t, void *, size_t);
int poll_uvbytes(gnutls_transport_ptr_t gtp, unsigned int ms);

void drain_outbound_queues(dest_t *d);
void ct_fail_dest_transfers(dest_t *d);

/* Helper/debugging functions */
void hexdump(char *desc, void *addr, int len);
void printsockaddr(const struct sockaddr_storage *, socklen_t);

#endif /* _CODATUNNELD_PRIVATE_H_ */
