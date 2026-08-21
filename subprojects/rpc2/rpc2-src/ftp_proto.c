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

#include <rpc2/tcpftp.h>
#include <string.h>

/* The wire param block is a single 8-byte correlation cookie. Every other
 * field of a transfer (file identity, tag, direction, offset, length, and any
 * in-VM buffers) is resolved locally at each end from its own SE descriptor,
 * so none of it crosses the wire: only the cookie pairs the two codatunneld
 * registrations (the client generates it in MakeRPC1, the server reuses it in
 * CheckSE). The length no longer depends on the tag. The cookie is carried as
 * its 8 native bytes: an SE body is opaque to RPC2 (rpc2_htonp converts only
 * the 32-bit header; the body is sent verbatim), so no byte-order conversion
 * is applied and the pack/unpack round-trip is exact, like every in-VM field.
 */
#define TCPFTP_BLOCK 8u

size_t tcpftp_param_blocksize(void)
{
    /* One fixed-size block for every supported form. */
    return TCPFTP_BLOCK;
}

int tcpftp_pack_param_block(const uint64_t *cookie, unsigned char *buf,
                            size_t maxlen, size_t *wrote)
{
    if (cookie == NULL || buf == NULL || wrote == NULL || maxlen < TCPFTP_BLOCK)
        return -1;
    memcpy(buf, cookie, sizeof(*cookie));
    *wrote = TCPFTP_BLOCK;
    return 0;
}

int tcpftp_unpack_param_block(const unsigned char *buf, size_t len,
                              uint64_t *cookie)
{
    /* Exactly one 8-byte block; only the cookie is written. Tag, offset,
     * length and the file identity are local to each end and never cross the
     * wire, so they are not touched here. */
    if (buf == NULL || cookie == NULL || len != TCPFTP_BLOCK)
        return -1;
    memcpy(cookie, buf, sizeof(*cookie));
    return 0;
}
