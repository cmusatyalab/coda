/* BLURB lgpl

                           Coda File System
                              Release 7

          Copyright (c) 2017-2020 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#ifndef _CODATUNNEL_PRIVATE_H_
#define _CODATUNNEL_PRIVATE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <sys/time.h>
#include <uv.h>
#include <gnutls/gnutls.h>

int mapthread(uv_thread_t);

#if 0
#define DEBUG(...)                                                          \
    do {                                                                    \
        struct timeval tt;                                                  \
        gettimeofday(&tt, 0);                                               \
        int myid = mapthread(uv_thread_self());                             \
        fprintf(stderr, "[%d] %ld:%ld %s:%d ", myid, tt.tv_sec, tt.tv_usec, \
                __FUNCTION__, __LINE__);                                    \
        fprintf(stderr, __VA_ARGS__);                                       \
        fflush(stderr);                                                     \
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

/* the actual tunnel daemon (defined in codatunneld.c) */
void codatunneld(int codatunnel_sockfd, const char *tcp_bindaddr,
                 const char *udp_bindaddr, const char *bind_service,
                 int onlytcp) __attribute__((noreturn));

/* Format of encapsulated UDP packets sent on Unix domain connections
   (i.e., between Venus and codatunneld, and between codasrv and
   codatunneld.)  All fields are in the clear. This header is followed
   by `msglen` encrypted bytes of the RPC2 packet that is sent or
   received.  On the network, this header is NOT sent in UDP packets; but
   it IS sent in TCP-tunneled packets
*/
typedef struct codatunnel_packet {
    char magic[8];
    uint32_t is_retry; /* 1 if this is a resend, 0 otherwise */
    uint32_t is_init1; /* 1 if this is an Init1 opcode,0 otherwise */
    uint32_t msglen; /* actual number of bytes in the packet */
    uint32_t addrlen; /* verbatim from sendto() or recvfrom () */
    struct sockaddr_storage addr; /* verbatim from sendto() or recvfrom () */
} ctp_t;

#define MAXRECEIVE (4500 + sizeof(ctp_t))
/* WARNING: gross hack! Above field is space for received packet
   that may be assembled piecemeal from multiple TCP reads; the
   figure of 4500 is RPC2_MAXPACKETSIZE; there are many cleverer
   ways of allocating this space, but the simplicity of this
   approach is fine for initial implementation; get it working
   first, and then fix later
*/

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
    TCPCLOSING    = 5, /* now closing, and waiting to become FREE */
};

const char *tcpstatename(enum deststate);

typedef struct {
    uv_buf_t b; /* b.len is max size of buffer, not useful bytes */
    int numbytes; /* number of useful bytes pointed to by b->base */
} enq_data_t;

typedef struct remotedest {
    struct sockaddr_storage destaddr;
    socklen_t destlen;
    char fqdn[NI_MAXHOST]; /* obtained via getnameinfo() from destaddr */

    enum deststate state; /* All destinations are assumed to be capable of
                             becoming TCPACTIVE; Setting TCPACTIVE should be a
                             commit point: all fields below should have been
                             set before that happens, to avoid race conditions */

    uv_tcp_t *tcphandle; /* only valid if state is TCPACTIVE or TLSHANDSHAKE */
    int packets_sent; /* for help with INIT1 retries */

    gnutls_session_t my_tls_session;

    char *decrypted_record; /* pointer to malloced array of size MAXRECEIVE;
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
    uv_mutex_t tls_send_record_mutex;

    struct minicb_tcp_req *outbound_queue;
    uv_async_t outbound_worker;
    uv_mutex_t outbound_mutex;
} dest_t;

void outbound_worker_cb(uv_async_t *async);

/* Stuff for destination management */
void initdestarray();
dest_t *getdest(const struct sockaddr_storage *, socklen_t);
dest_t *createdest(const struct sockaddr_storage *, socklen_t);
void free_dest(dest_t *d);

/* Procedures to add and remove buffered data from a dest.
   These operate in producer-consumer manner.
   recv_tcp_cb() calls enq_uvbuf() as producer.
   gnutls_record_recv() calls eat_uvbytes() as consumer.
   During TLS handshake, eat_uvbytes() is also called as consumer.
*/
void enq_element(dest_t *, uv_buf_t *, int);
ssize_t eat_uvbytes(gnutls_transport_ptr_t, void *, size_t);

/* Helper/debugging functions */
void hexdump(char *desc, void *addr, int len);
void printsockaddr(const struct sockaddr_storage *, socklen_t);

#endif /* _CODATUNNEL_PRIVATE_H_ */
