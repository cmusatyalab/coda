/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2017-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

#ifndef _CODATUNNEL_WRAPPER_H_
#define _CODATUNNEL_WRAPPER_H_

#include <sys/socket.h>
#include <sys/types.h>

/* Flag bits for codatunnel_sendto to hint that this is a retried UDP send
   and also a separate bit to hint that this is an Init1 packet */
/* Linux currently uses the following bits already 0x6005ffff, they may be
 * going up with 'standard flags', and down with linux specific ones or
 * something hopefully the following bit is a 'meet in the middle' case that
 * won't be reached anytime soon although we strip the bit before passing
 * 'flags' on to libc anyway. */
#define CODATUNNEL_ISRETRY_HINT 0x01000000
#define CODATUNNEL_ISINIT1_HINT 0x02000000
#define CODATUNNEL_HINTS (CODATUNNEL_ISRETRY_HINT | CODATUNNEL_ISINIT1_HINT)

/* return socket to codatunneld when tunnel is started, otherwise return -1 */
int codatunnel_socket();

ssize_t codatunnel_sendto(int sockfd, const void *buf, size_t len, int flags,
                          const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t codatunnel_recvfrom(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *src_addr, socklen_t *addrlen);

#endif /* _CODATUNNEL_WRAPPER_H_ */
