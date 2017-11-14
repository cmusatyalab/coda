/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2017 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "codatunnel.private.h"

/*
   Daemon that does the relaying of packets to/from net and localhost.
   Created via fork() by Venus or codasrv.

   Uses single Unix domain socket to talk to Venus or codasrv on
   localhost, and one TCP-tunneled socket to talk to each distinct
   remote Coda server or client.  Also has one UDP socket for backward
   compatibility with legacy servers and clients.

   This code layers UDP socket primitives on top of TCP connections.
   Maintains a single TCP connection for each (host, port) pair
   All UDP packets to/from that (host, port) pair are sent/recvd on this connection.
   All RPC2 connections to/from that (host,port are multiplexed on this connection.
   Minimal changes to rest of the RPC2 code.
   Discards all packets with "RETRY" bit set.

   Possible negative consequences:
   (a) serializes all transmissions to each (host,port) pair
       (but no guarantee that such serialization wasn't happening before)
   (b) SFTP becomes a stop and wait protocol for each 8-packet window
       (since RETRY flag triggered sendahead)

   (Satya, 2017-01-04)
*/


static socklen_t sockaddr_len(const struct sockaddr *addr)
{
    if (addr->sa_family == AF_INET)
        return sizeof(struct sockaddr_in);

    if (addr->sa_family == AF_INET6)
        return sizeof(struct sockaddr_in6);

    return 0;
}


static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    *buf = uv_buf_init(malloc(suggested_size), suggested_size);

    /* gracefully handle allocation failures on libuv < 1.10.0 */
    if (buf->base == NULL) buf->len = 0;
}


typedef struct {
    uv_udp_send_t req;
    uv_buf_t msg[2];
    struct codatunnel_packet packet;
} udp_send_req_t;

static void udp_sent_cb(uv_udp_send_t *arg, int status)
{
    udp_send_req_t *req = (udp_send_req_t *)arg;
    free(req->msg[1].base);
    free(req);
}


static void recv_codatunnel_cb(uv_udp_t *codatunnel, ssize_t nread,
                               const uv_buf_t *buf,
                               const struct sockaddr *addr,
                               unsigned flags)
{
    uv_udp_t *udpsocket = codatunnel->data;
    struct codatunnel_packet *p = (struct codatunnel_packet *)buf->base;
    udp_send_req_t *req;
    static unsigned empties;

    DEBUG("packet received from codatunnel nread=%ld buf=%p addr=%p flags=%u\n",
           nread, buf ? buf->base : NULL, addr, flags);

    if (nread == 0) {
        /* empty packet received, we normally get this after we've drained any
         * pending data from the socket after a wakeup. But we also see these
         * when the other end of a socketpair was closed. Differentiate by
         * counting how many successive empties we get. --JH */
        if (++empties >= 3) {
            DEBUG("codatunnel closed\n");
            uv_stop(codatunnel->loop);
            uv_close((uv_handle_t *)codatunnel, NULL);
        }
        free(buf->base);
        return;
    }
    empties = 0;

    if (nread == -1) {
        /* We shouldn't see read errors on the codatunnel socketpair. --JH */
        uv_close((uv_handle_t *)codatunnel, NULL);
        free(buf->base);
        return;
    }
    if (nread < sizeof(struct codatunnel_packet)) {
        DEBUG("short packet received from codatunnel\n");
        free(buf->base);
        return;
    }

    req = malloc(sizeof(udp_send_req_t));
    /* data to send is what follows the codatunnel packet header */
    req->msg[0] = uv_buf_init(buf->base + sizeof(struct codatunnel_packet),
                              nread - sizeof(struct codatunnel_packet));

    /* move buffer from reader to writer, we won't actually send msg[1] but
     * this way the buffer will get properly freed in `udp_sent_cb` */
    req->msg[1] = uv_buf_init(buf->base, nread);
    /* buf->base = NULL; buf->len = 0; */

    /* forward packet to the remote host */
    uv_udp_send((uv_udp_send_t *)req, udpsocket, req->msg, 1,
                (struct sockaddr *)&p->addr, udp_sent_cb);
}


static void recv_udpsocket_cb(uv_udp_t *udpsocket, ssize_t nread,
                              const uv_buf_t *buf,
                              const struct sockaddr *addr,
                              unsigned flags)
{
    uv_udp_t *codatunnel = udpsocket->data;
    udp_send_req_t *req;
    struct sockaddr_in peer = {
        .sin_family = AF_INET,
    };

    DEBUG("packet received from udpsocket nread=%ld buf=%p addr=%p flags=%u\n",
          nread, buf ? buf->base : NULL, addr, flags);

    if (nread == -1) {
        uv_close((uv_handle_t *)udpsocket, NULL);
        free(buf->base);
        return;
    }

    if (nread == 0) {
        free(buf->base);
        return;
    }

    req = malloc(sizeof(udp_send_req_t));
    req->msg[0] = uv_buf_init((char *)&req->packet,
                              sizeof(struct codatunnel_packet));
    req->packet.addrlen = sockaddr_len(addr);
    memcpy(&req->packet.addr, addr, req->packet.addrlen);
    req->packet.msglen = nread;

    /* move buffer from reader to writer */
    req->msg[1] = uv_buf_init(buf->base, nread);
    /* buf->base = NULL; buf->len = 0; */

    /* forward packet to venus/codasrv */
    uv_udp_send((uv_udp_send_t *)req, codatunnel, req->msg, 2,
                (struct sockaddr *)&peer, udp_sent_cb);
}

/* main routine of coda tunnel daemon */
void codatunneld(int codatunnel_sockfd,
                 const char *tcp_bindaddr,
                 const char *udp_bindaddr,
                 const char *bind_service)
{
    uv_loop_t *loop;
    uv_udp_t codatunnel, udpsocket;
    uv_getaddrinfo_t gai_req;
    const struct addrinfo *ai, gai_hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
        .ai_flags = AI_PASSIVE,
    };

    fprintf(stderr, "codatunneld: starting\n");

    /* make sure that writing to closed pipes doesn't kill us */
    signal(SIGPIPE, SIG_IGN);

    loop = uv_default_loop();

    /* bind codatunnel_sockfd */
    uv_udp_init(loop, &codatunnel);
    uv_udp_open(&codatunnel, codatunnel_sockfd);

    /* resolve the requested udp bind address */
    const char *node = (udp_bindaddr && *udp_bindaddr) ? udp_bindaddr : NULL;
    const char *service = bind_service ? bind_service : "0";
    uv_getaddrinfo(loop, &gai_req, NULL, node, service, &gai_hints);

    /* try to bind to any of the resolved addresses */
    uv_udp_init(loop, &udpsocket);
    for (ai = gai_req.addrinfo; ai != NULL; ai = ai->ai_next) {
        if (uv_udp_bind(&udpsocket, ai->ai_addr, 0) == 0)
            break;
    }
    uv_freeaddrinfo(gai_req.addrinfo);

    /* link connections */
    codatunnel.data = &udpsocket;
    udpsocket.data = &codatunnel;

    uv_udp_recv_start(&codatunnel, alloc_cb, recv_codatunnel_cb);
    uv_udp_recv_start(&udpsocket, alloc_cb, recv_udpsocket_cb);

    /* run until the codatunnel connection closes */
    uv_run(loop, UV_RUN_DEFAULT);

    /* cleanup any remaining open handles */
    uv_walk(loop, (uv_walk_cb)uv_close, NULL);
    uv_run(loop, UV_RUN_DEFAULT);
    uv_loop_close(loop);
    exit(0);
}
