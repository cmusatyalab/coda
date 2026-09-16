/* BLURB lgpl

                            Coda File System
                               Release 8

           Copyright (c) 2026 Carnegie Mellon University
                   Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

#*/

/* TCPFTP parameter block.
 *
 * TCPFTP streams a file's bytes over the codatunnel daemon-to-daemon channel
 * rather than through the in-RPC2-process SFTP loop. The SE still travels with
 * the RPC, but only its *control* payload rides in the request body: a single
 * 8-byte correlation cookie. Everything else - file identity, tag, direction,
 * offset, length, and any in-VM buffers - is resolved locally at each end from
 * its own SE descriptor, so it never crosses; the cookie is the one value that
 * pairs the two codatunneld registrations. This header describes that block;
 * ftp_proto.c implements its (de)serialization so it can be unit-tested without
 * a live connection or LWP context. */

#ifndef _RPC2_TCPFTP_H_
#define _RPC2_TCPFTP_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <rpc2/se.h>

/* Per-connection codatunnel offload state. Embedded in the standalone TCPFTP
 * entry and in the SMARTFTP SFTP_Entry when the connection negotiated TCPFTP.
 * Only the cookie crosses the wire (see ftp_proto.c). */
struct TcpFtpState {
    uint64_t Cookie; /* correlation cookie; 0 == none */
    int VmFd; /* in-VM sink spool fd; -1 == none */
    int GotBlock; /* server: cookie parsed from the request */
};

#define TCPFTP_ROLE_SOURCE 0 /* mirror CT_SOURCE / CT_SINK (codatunnel/ctp.h) */
#define TCPFTP_ROLE_SINK 1

/* Length a local registration reports to the daemon: for a source, size past
 * the seek offset capped at ByteQuota when positive (<= 0 == unlimited,
 * matching classic SFTP); for a sink, 0 (accept however many bytes arrive).
 * See tcpftp_reglen() in tcpftp1.c. */
uint64_t tcpftp_reglen(const struct SFTP_Descriptor *d, int role,
                       uint64_t size);

/* Open/resolve the local file for d's tag + role and register it with the
 * daemon (see tcpftp1.c). in_cookie is 0 (client, generate) or the client's
 * cookie (server, reuse); *out_cookie receives the resulting cookie. Returns 0
 * or an RPC2_SEFAIL* code. */
int tcpftp_register_local(struct TcpFtpState *st, const struct sockaddr *peer,
                          socklen_t plen, struct SFTP_Descriptor *d, int role,
                          uint64_t in_cookie, uint64_t *out_cookie);

/* Wait for the daemon's terminal status; for an in-VM sink, drain the spool
 * back into d's buffer. Returns the terminal status (0 == ok). When nowait
 * is set the wait is non-blocking: a still-pending cookie yields -1
 * (not-done) instead of blocking the io thread. */
long tcpftp_finalize(struct TcpFtpState *st, struct SFTP_Descriptor *d,
                     uint64_t cookie, int nowait);

/* Byte size of the packed param block: a fixed 8 bytes (the correlation
 * cookie) for every supported form. */
size_t tcpftp_param_blocksize(void);

/* Serialize the correlation cookie into buf[0..maxlen). Returns 0 and sets
 * *wrote (=8) on success; -1 if any argument is NULL or maxlen is smaller than
 * tcpftp_param_blocksize(). The cookie is the only wire field; everything else
 * is local to each end and is not encoded. */
int tcpftp_pack_param_block(const uint64_t *cookie, unsigned char *buf,
                            size_t maxlen, size_t *wrote);

/* Inverse of tcpftp_pack_param_block: reads exactly one 8-byte block from buf
 * and stores its cookie into *cookie. Returns 0 on success; -1 if len is not
 * exactly 8, or a pointer is NULL. */
int tcpftp_unpack_param_block(const unsigned char *buf, size_t len,
                              uint64_t *cookie);

/* Register the TCPFTP side-effect definition with RPC2 (grows the SE_DefSpecs
 * table, exactly as SFTP_Activate does). Call before RPC2_Init, alongside
 * SFTP_Activate. No-op if the SE table is already active. Returns the SE
 * number (TCPFTP). */
long TCPFTP_Activate(void);

#ifdef __cplusplus
}
#endif

#endif /* _RPC2_TCPFTP_H_ */
