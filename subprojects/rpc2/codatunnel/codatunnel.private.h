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

#ifndef _CODATUNNEL_PRIVATE_H_
#define _CODATUNNEL_PRIVATE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>

#include <rpc2/codatunnel.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flag bits for codatunnel_sendto to hint that this is a retried UDP send
   and also a separate bit to hint that this is an Init0 packet */
/* Linux currently uses the following bits already 0x6005ffff, they may be
 * going up with 'standard flags', and down with linux specific ones or
 * something hopefully the following bit is a 'meet in the middle' case that
 * won't be reached anytime soon although we strip the bit before passing
 * 'flags' on to libc anyway. */
#define CODATUNNEL_ISRETRY_HINT 0x01000000
#define CODATUNNEL_ISINIT0_HINT 0x02000000
#define CODATUNNEL_HINTS (CODATUNNEL_ISRETRY_HINT | CODATUNNEL_ISINIT0_HINT)

/* return socket to codatunneld when tunnel is started, otherwise return -1 */
int codatunnel_socket();

void codatunnel_init0(const struct sockaddr *addr, socklen_t addrlen,
                      const char *peername);

ssize_t codatunnel_sendto(int sockfd, const void *buf, size_t len, int flags,
                          const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t codatunnel_recvfrom(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *src_addr, socklen_t *addrlen);

/* The daemon-driven file-transfer control plane. Internal to the RPC2
 * libraries: the only production caller is the TCPFTP side-effect binding
 * (libse), which streams a file's bytes over the tunnel instead of the
 * in-RPC2 SFTP loop. Not part of the app-facing API in <rpc2/codatunnel.h>.
 *
 * codatunnel_file_register() hands the local codatunneld a local file `fd`
 * (reading from `offset` for CT_SOURCE, expecting `length`/sink for CT_SINK)
 * to stream over the tunnel channel identified by `peer`. The fd is passed
 * to the daemon via SCM_RIGHTS in the same datagram as the registration. The
 * caller keeps its own handle to `fd`. A 64-bit `cookie`
 * identifies the registration and is returned in *cookie (set to 0 on
 * failure). codatunnel_file_wait() blocks (cooperatively) until the daemon
 * reports a terminal status for that cookie, releases and returns the
 * SFTP-space status (0 == success).
 * codatunnel_file_unreg() releases without blocking. When the tunnel is not
 * started these all report -1 / no-op.
 */
int codatunnel_file_register(const struct sockaddr *peer, socklen_t addrlen,
                             int fd, uint64_t offset, uint64_t length, int role,
                             uint64_t *cookie);
int codatunnel_file_wait(uint64_t cookie, int timeout_ticks, uint64_t *nbytes);
void codatunnel_file_unreg(uint64_t cookie);

#ifdef __cplusplus
}
#endif

#endif /* _CODATUNNEL_PRIVATE_H_ */
