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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>

#include <rpc2/codatunnel.h>
#include "codatunnel.private.h"
#include "wrapper.h"


/* global flag below controls whether codatunnel is used */
static int codatunnel_enable_codatunnel = 0; /* non zero to enable tunneling */

/* fd in parent of open hfsocket */
static int codatunnel_vside_sockfd = -1;  /* v2t: venus to tunnel */


int codatunnel_fork(int argc, char **argv,
                    const char *tcp_bindaddr,
                    const char *udp_bindaddr,
                    const char *bind_service)
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
    */
    int rc, sockfd[2];

    DEBUG("codatunnel_fork(\"%s:%s\", \"%s:%s\")\n",
          tcp_bindaddr, bind_service, udp_bindaddr, bind_service);

    /* codatunnel is enabled when the daemon process is forked */
    codatunnel_enable_codatunnel = 1;


    /* Create socketpair for host-facing UDP communication */
    rc = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockfd);
    if (rc < 0) {
        perror("codatunnel_fork: socketpair() failed: ");
        return -1;
    }

    DEBUG("hfsocket_fdpair after socketpair() is: [%d, %d]\n", sockfd[0], sockfd[1]);
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
        return 0;  /* this is the only success return */
    }

    /* If I get here, I must be the newborn child */
    DEBUG("Child: codatunneld fork succeeded\n");
    close(sockfd[0]); /* codatunnel_vside_sockfd */


    /* if possible, rename child's command line for "ps ax" */
    if (argc) {
        uv_setup_args(argc, argv);
        uv_set_process_title("codatunneld");
    }

    /* launch the tunnel and never return */
    codatunneld(sockfd[1], tcp_bindaddr, udp_bindaddr, bind_service);
    __builtin_unreachable(); /* should never reach here */
}


int codatunnel_socket()
{
    return codatunnel_vside_sockfd;  /* already created by socketpair () */
}


ssize_t codatunnel_sendto(int sockfd, const void *buf, size_t len, int flags,
                          const struct sockaddr *addr, socklen_t addrlen)
{
    int rc;
    struct codatunnel_packet p;
    struct iovec iov[2];
    struct msghdr msg = {
        .msg_iov = iov,
        .msg_iovlen = 2,
    };

    if (!codatunnel_enable_codatunnel) {
        return sendto(sockfd, buf, len, flags & ~CODATUNNEL_ISRETRY_HINT,
                      addr, addrlen);
    }

    /* construct the codatunnel packet */
    memcpy(&p.addr, addr, addrlen);
    p.addrlen = addrlen;
    p.is_retry = flags & CODATUNNEL_ISRETRY_HINT;
    p.msglen = len;

    iov[0].iov_base = &p;
    iov[0].iov_len = sizeof(struct codatunnel_packet);
    iov[1].iov_base = (void *)buf;
    iov[1].iov_len = len;

    DEBUG("sending packet to codatunneld size=%ld\n",
          sizeof(struct codatunnel_packet) + len);

    /* then send it to codatunneld */
    rc = sendmsg(sockfd, &msg, 0);

    if (rc < 0)
        return rc;

    /* adjust returned value for size of packet header */
    rc -= sizeof(struct codatunnel_packet);
    if (rc < 0) {
        errno = ENOBUFS;
        return -1;
    }
    return rc;
}


ssize_t codatunnel_recvfrom(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *from, socklen_t *fromlen)
{
    int rc;
    struct codatunnel_packet p;
    struct iovec iov[2];
    struct msghdr msg = {
        .msg_iov = iov,
        .msg_iovlen = 2,
    };

    if (!codatunnel_enable_codatunnel) {
        return recvfrom(sockfd, buf, len, flags, from, fromlen);
    }

    iov[0].iov_base = &p;
    iov[0].iov_len = sizeof(struct codatunnel_packet);
    iov[1].iov_base = buf;
    iov[1].iov_len = len;

    /* get the packet from codatunneld */
    rc = recvmsg(sockfd, &msg, 0);

    DEBUG("received packet from codatunneld size=%d\n", rc);

    if (rc < 0)
        return rc; /* error */

    /* were the buffers we passed large enough? */
    if ((msg.msg_flags & MSG_TRUNC) || (*fromlen < p.addrlen))
    {
        errno = ENOSPC;
        return -1;
    }

    /* copy peer address */
    memcpy(from, &p.addr, p.addrlen);
    *fromlen = p.addrlen;

    /* make sure we received enough data to read the packet header */
    rc -= sizeof(struct codatunnel_packet);
    if (rc < 0) {
        /* this should not happen, codatunneld probably crashed */
        errno = EBADF;
        return -1;
    }
    return rc;
}
