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


/* the actual tunnel daemon (defined in codatunneld.c) */
void codatunneld(int codatunnel_sockfd,
                 const char *tcp_bindaddr,
                 const char *udp_bindaddr,
                 const char *bind_service)
    __attribute__((noreturn));


/* Format of encapsulated UDP packets sent on Unix domain connections
   (i.e., between Venus and codatunneld, and between codasrv and codatunneld.)
   All fields are in the clear. This header is followed by `msglen` encrypted
   bytes of the RPC2 packet that is sent or received.
*/
struct codatunnel_packet {
    struct sockaddr_storage addr; /* verbatim from sendto() or recvfrom () */
    socklen_t addrlen;            /* verbatim from sendto() or recvfrom () */
    int is_retry;                 /* 1 if this is a resend, 0 otherwise */
    size_t msglen;                /* actual number of bytes in the packet */
};

#endif /* _CODATUNNEL_PRIVATE_H_ */
