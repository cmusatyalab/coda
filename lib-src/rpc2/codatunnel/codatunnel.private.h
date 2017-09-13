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

#ifndef _CODATUNNEL_PRIVATE_H_
#define _CODATUNNEL_PRIVATE_H_

#include <sys/types.h>
#include <sys/socket.h>

#if 0
#define DEBUG(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)
#else
#define DEBUG(...)
#endif

/*
   With codatunnel.h, this code has to be linkable into two different
   worlds: the LWP/RPC2 world of Venus & codasrv, and the
   pthreads/SSL/TLS world of codatunneld.  Notice that there are no
   dependencies on LWP or RPC2 header files.  Although this source
   code is physically located in the RPC2 part of the code base for
   convenience, it is logically separable.
*/

/* the actual tunnel daemon (defined in codatunneld.c) */
void codatunneld(int codatunnel_sockfd,
                 const char *tcp_bindaddr,
                 const char *udp_bindaddr,
                 const char *bind_service)
    __attribute__((noreturn));


/* WARNING: Ugly manual definitions */
/* For reasons of avoiding dependence on RPC2/LWP headers, manually
   define below value equal to MAXPACKETSIZE in RPC2. I know this is
   bad programming practice, but it is the lesser evil. MAXPACKETSIZE
   in RPC2 has not changed since 1984, and I can't see it changing in
   the future.
*/
#define CODATUNNEL_MAXPACKETSIZE 4500


/* Format of encapsulated UDP packets sent on Unix domain connections
   (i.e., between Venus and codatunneld, and between codasrv and codatunneld.)
   All fields are in the clear except last one (out).
*/
struct codatunnel_packet {
    struct sockaddr_storage addr; /* verbatim from sendto() or recvfrom () */
    socklen_t addrlen;            /* verbatim from sendto() or recvfrom () */
    int is_retry;                 /* 1 if this is a resend, 0 otherwise */
    size_t msglen;                /* actual number of bytes in the packet */
};

#endif /* _CODATUNNEL_PRIVATE_H_ */
