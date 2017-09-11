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
#include <netinet/in.h>

#include "codatunnel.private.h"

/*
   Daemon that does the relaying of packets to/from net and localhost.
   Created via fork() by Venus or codasrv.
   Uses single Unix domain socket to talk to Venus or codasrv on
   localhost, and one TCP-tunnelled socket to talk to each distinct
   remote codasrv or Venus.  Also has one UDP socket for upward
   compatibility with legacy servers and clients.
*/

/* fd and port number of single net-facing bidirectional socket
   for legacy servers and clients */
static int netfacing_udp_fd;
static short netfacing_udp_portnum;

/* fd of single net-facing TCP bind (i.e., rendezvous) socket;
   only relevant when (initiate_tcp_flag == 0) */
static int netfacing_tcp_bind_fd = -1;  /* TCP portnum same as UDP portnum */

/* Set of file descriptors to wait on */
static fd_set BigFDList;
static int BigFDList_Highest;  /* one larger than highest fd in BigFDList */

/* forward refs to local functions */
static void InitializeNetFacingSockets(int codatunnel_sockfd);
static void DoWork(int codatunnel_sockfd) __attribute__((noreturn));
static void HandleFDException(int);
static void HandleOutgoingUDP(int codatunnel_sockfd);
static void HandleIncomingUDP(int codatunnel_sockfd);
static void HandleNewTCPconnect();
static void HandleIncomingTCP(int);


/* main routine of coda tunnel daemon */
void codatunneld(int codatunnel_sockfd, short udp_bindport)
{
    fprintf(stderr, "codatunneld: starting\n");

    netfacing_udp_portnum = udp_bindport;
    InitializeNetFacingSockets(codatunnel_sockfd);
    DoWork(codatunnel_sockfd);
}


void InitializeNetFacingSockets(int codatunnel_sockfd)
{
    int rc;
    struct sockaddr_in saddr;

    /* First create the single UDP socket for legacy clients & servers
       Do this even if netfacing_udp_portnum is zero, because that means
       "use any port"
       */

    netfacing_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (netfacing_udp_fd < 0){
        perror("socket: ");
        exit(-1);
    }
    DEBUG("netfacing_udp_fd = %d\n", netfacing_udp_fd);

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(netfacing_udp_portnum);

    DEBUG("netfacing_udp_portnum = %d\n", netfacing_udp_portnum);
    rc = bind(netfacing_udp_fd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (rc < 0){
        perror("bind: ");
        exit(-1);
    }
    DEBUG("bind succeeded\n");


    netfacing_tcp_bind_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (netfacing_tcp_bind_fd < 0){
        perror("socket: ");
        exit(-1);
    }
    DEBUG("netfacing_tcp_bind_fd = %d\n", netfacing_tcp_bind_fd);

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(netfacing_udp_portnum);
    rc = bind(netfacing_tcp_bind_fd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (rc < 0){
        perror("bind: ");
        exit(-1);
    }
    DEBUG("bind succeeded\n");

    DEBUG("About to set global bit mask\n");

    /* Initialize the global bit mask and find highest fd for select()
       Note that we don't listen on codatunnel_pipefd; it is only for signals
       */
    FD_ZERO(&BigFDList);
    FD_SET(codatunnel_sockfd, &BigFDList);
    BigFDList_Highest = codatunnel_sockfd + 1;

    FD_SET(netfacing_udp_fd, &BigFDList);
    if (BigFDList_Highest <= netfacing_udp_fd)
        BigFDList_Highest = netfacing_udp_fd + 1;

    /* DEBUG:  FD_SET(netfacing_tcp_bind_fd, &BigFDList);  */
    if (BigFDList_Highest <= netfacing_tcp_bind_fd)
        BigFDList_Highest = netfacing_tcp_bind_fd + 1;

    DEBUG("codatunnel: BigFDList_Highest = %d\n", BigFDList_Highest);
}


void DoWork(int codatunnel_sockfd)
{
    int i, rc;
    fd_set readlist, exceptlist;

    while (1) /* main work loop, non-terminating */
    {
        readlist = BigFDList;
        exceptlist = BigFDList;

        rc = select(BigFDList_Highest, &readlist, 0, &exceptlist, 0);
        if (rc < 0)
            continue;

        /* first deal with all the exceptions */
        for (i = 0; i < BigFDList_Highest; i++)
        {
            if (!FD_ISSET(i, &exceptlist)) continue;
            HandleFDException(i);
        }

        /* then deal with fds with incoming data */
        for (i = 0; i < BigFDList_Highest; i++)
        {
            if (!FD_ISSET(i, &readlist)) continue;

            /* fd i has data for me */
            if (i == codatunnel_sockfd) {
                HandleOutgoingUDP(codatunnel_sockfd);
                continue;
            }

            if (i == netfacing_udp_fd) {
                HandleIncomingUDP(codatunnel_sockfd);
                continue;
            }

            if (i == netfacing_tcp_bind_fd) {
                HandleNewTCPconnect();
                continue;
            }

            /* if we get here, it must be an incoming TCP packet */
            HandleIncomingTCP(i);
        }
    }
}


void HandleFDException(int whichfd)
{
    DEBUG("HandleFDExecption(%d)\n", whichfd);
}

void HandleOutgoingUDP(int codatunnel_sockfd)
{
    int rc;
    struct codatunnel_packet p;
    char buf[CODATUNNEL_MAXPACKETSIZE];
    struct iovec iov[2];
    struct msghdr msg = {
        .msg_iov = iov,
        .msg_iovlen = 2,
    };
    size_t buflen;

    iov[0].iov_base = &p;
    iov[0].iov_len = sizeof(struct codatunnel_packet);
    iov[1].iov_base = buf;
    iov[1].iov_len = CODATUNNEL_MAXPACKETSIZE;

    rc = recvmsg(codatunnel_sockfd, &msg, 0);

    /* EOF - parent closed the socket */
    if (rc == 0) exit(0);

    if (rc < sizeof(struct codatunnel_packet))
        return; /* error return */

    buflen = rc - sizeof(struct codatunnel_packet);

    /* send it out on the network */
    sendto(netfacing_udp_fd, buf, buflen, 0,
           (struct sockaddr *)&p.addr, p.addrlen);
}

void HandleIncomingUDP(int codatunnel_sockfd)
{
    int rc;
    struct codatunnel_packet p = {
        .addrlen = sizeof(struct sockaddr_storage),
    };
    char buf[CODATUNNEL_MAXPACKETSIZE];
    struct iovec iov[2];
    struct msghdr msg = {
        .msg_iov = iov,
        .msg_iovlen = 2,
    };

    rc = recvfrom(netfacing_udp_fd, buf, CODATUNNEL_MAXPACKETSIZE, 0,
                  (struct sockaddr *)&p.addr, &p.addrlen);

    if (rc < 0) return; /* error return */

    p.msglen = rc;

    iov[0].iov_base = &p;
    iov[0].iov_len = sizeof(struct codatunnel_packet);
    iov[1].iov_base = buf;
    iov[1].iov_len = p.msglen;

    /* send it to the host */
    sendmsg(codatunnel_sockfd, &msg, 0);
}

void HandleNewTCPconnect()
{
    DEBUG("HandleNewTCPconnect()\n");
}

void HandleIncomingTCP(int whichfd)
{
    DEBUG("HandleIncomingTCP()\n");
}

