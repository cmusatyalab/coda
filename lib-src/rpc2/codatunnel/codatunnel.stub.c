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

#include <sys/types.h>
#include <sys/socket.h>
#include <rpc2/codatunnel.h>
#include "wrapper.h"

/* If codatunnel is not configured, this file provides no-op routines
   to keep the linker happy */

/* stubs to make codatunnel related code a noop */
int codatunnel_fork(int argc, char **argv,
                    const char *tcp_bindaddr,
                    const char *udp_bindaddr,
                    const char *bind_service)
{
    return -1;
}

int codatunnel_socket()
{
    return -1;
}

ssize_t codatunnel_sendto(int sockfd, const void *buf, size_t len, int flags,
                          const struct sockaddr *addr, socklen_t addrlen)
{
    return sendto(sockfd, buf, len, flags & ~CODATUNNEL_ISRETRY_HINT,
                    addr, addrlen);
}

ssize_t codatunnel_recvfrom(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *from, socklen_t *fromlen)
{
    return recvfrom(sockfd, buf, len, flags, from, fromlen);
}
