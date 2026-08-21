/* BLURB lgpl

                           Coda File System
                              Release 8

           Copyright (c) 2026 Carnegie Mellon University
                   Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                         Additional copyrights

#*/

/* Pure wire protocol for codatunneld. No libuv, no gnutls, so this can be
 * included (and unit-tested) without pulling the tunnel dependencies into
 * a build. The same header wraps both the daemon-to-daemon TLS records and
 * the Unix-socket ("vside") exchanges between an app and its codatunneld.
 *
 * The `opcode` word is the former `is_init0` hint, reused in place. The byte
 * layout of ctp_t is unchanged, so sizeof(ctp_t) and the "magic01" magic word
 * are preserved for backward compatibility with peers that only ever wrote the
 * word to 0 (a plain packet) or 1 (INIT0).
 */

#ifndef _CODATUNNEL_CTP_H_
#define _CODATUNNEL_CTP_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#define CT_MAGIC "magic01"
#define CT_MAGICSZ 8

enum ct_opcode
{
    CT_PKT       = 0, /* encapsulated RPC2 packet (today's plain packet) */
    CT_INIT0     = 1, /* peername hint (NUL-padded body) */
    CT_FILEREG   = 2, /* app->daemon: register a local transfer */
    CT_FILEUNREG = 3, /* app->daemon: cancel/release a registration */
    CT_FILEDONE  = 4, /* daemon->app: terminal state of a registration */
    CT_TRANSFER_READY =
        5, /* daemon->daemon: sink (driver end) is open, proceed */
    CT_TRANSFER_DATA  = 6, /* daemon->daemon: file bytes @{cookie,offset} */
    CT_TRANSFER_EOF   = 7, /* daemon->daemon: no more bytes for cookie */
    CT_TRANSFER_ERROR = 8, /* daemon->daemon: local open/xfer failed */
    CT_TRANSFER_REQUEST =
        9, /* daemon->daemon: sink asks the source for bytes @{cookie,offset} */
};

/* role values for ct_filereg.role */
#define CT_SOURCE 0
#define CT_SINK 1

/*
 * ct_filereg.flags bits. The "driver" bit drives the CT_TRANSFER_READY start
 * rule: a single daemon can only see its own FILEREG, so it cannot tell which
 * end registered first. The RPC-server side (CheckSE) is always the driver
 * end — it passes the client's cookie back through *cookie, which is the
 * "non-zero in -> use as-is (server side)" convention, so the app sets this
 * bit from that single fact. A SOURCE that is "driver" (a fetch) starts
 * streaming immediately (the sink pre-registered); a SOURCE that is not
 * "driver" (a store) waits for the sink's CT_TRANSFER_READY before streaming.
 */
#define CT_FILREG_DRIVER 0x1

/* CT_CHUNKMAX, CT_MAX_RECORD and the TLS payload ceiling live further down,
 * after ctp_t and ct_transfer_data are defined (the payload size is derived
 * from sizeof() of both). */

/* Status values carried in ct_filedone.status and ct_transfer_error.status,
 * in the SFTP error space (0 == success). The daemon maps its local errno
 * into these; the app's SE layer maps them back to SE status at finalize. */
#define CT_STATUS_SUCCESS 0
#define CT_STATUS_NOENT 1 /* file does not exist */
#define CT_STATUS_ACCES 2 /* permission denied */
#define CT_STATUS_NOSPC 3 /* disk / quota full */
#define CT_STATUS_IOERR 4 /* any other local I/O or resource error */
#define CT_STATUS_TIMEOUT 5 /* channel death or cancellation */

typedef struct codatunnel_packet {
    char magic[8];
    uint32_t is_retry;
    uint32_t opcode; /* was is_init0; values from enum ct_opcode */
    uint32_t msglen;
    uint32_t addrlen;
    struct sockaddr_storage addr;
} ctp_t;

/* The layout must stay exactly: magic + four 32-bit words + the address.
   Written against the field widths (not a hard-coded total) because
   sizeof(struct sockaddr_storage) is platform dependent. The typedef form is
   used (rather than a C11 _Static_assert) so this compiles under C99 too. */
typedef char ctp_size_invariant[(sizeof(ctp_t) ==
                                 (size_t)CT_MAGICSZ + 4 * sizeof(uint32_t) +
                                     sizeof(struct sockaddr_storage)) ?
                                    1 :
                                    -1];

typedef struct ct_filedone {
    uint64_t cookie;
    uint64_t nbytes;
    uint32_t status; /* SFTP error space; 0 == success */
    uint32_t pad;
} ct_filedone;

typedef struct ct_filereg {
    uint64_t cookie;
    uint32_t role; /* CT_SOURCE or CT_SINK */
    uint32_t flags; /* CT_FILREG_* bits */
    uint16_t peerlen;
    uint16_t pad;
    uint64_t offset;
    uint64_t length; /* total file length / expected sink bytes */
    struct sockaddr_storage peer; /* which tunnel channel this rides */
} ct_filereg;

/*
 * The daemon-to-daemon data-path payloads, as the file-transfer opcodes
 * arrive in the body region of a ctp_t envelope over the TLS channel
 * (opcode == 5..8). The body itself is written in network byte order there
 * (the ctp_t header does that already); the structs here are the pure byte
 * layout. Only the ctp_t envelope is the shared framing; these ride in its
 * body region. CT_TRANSFER_DATA is followed (in the same record) by up to
 * CT_CHUNKMAX payload bytes, located at offset sizeof(ct_transfer_data).
 */
typedef struct ct_transfer_ready {
    uint64_t cookie;
} ct_transfer_ready;

typedef struct ct_transfer_data {
    uint64_t cookie;
    uint64_t offset; /* absolute file offset of the payload that follows */
} ct_transfer_data;

typedef struct ct_transfer_eof {
    uint64_t cookie;
} ct_transfer_eof;

typedef struct ct_transfer_error {
    uint64_t cookie;
    uint32_t status; /* SFTP error space; 0 == none */
    uint32_t pad;
} ct_transfer_error;

typedef struct ct_transfer_request {
    uint64_t cookie;
    uint64_t offset; /* absolute file offset of the requested bytes */
    uint64_t len; /* how many bytes the sink wants (<= CT_CHUNKMAX) */
} ct_transfer_request;

/* A TLS record's content payload size is negotiated per session and, in
 * practice, ranges from 512 up to 16384 bytes depending on configuration and
 * TLS extensions. A full-chunk CT_TRANSFER_DATA record is the ctp_t header
 * (whose addr field is a whole sockaddr_storage on every platform we build
 * for) + the ct_transfer_data header + the payload. Size the payload to the
 * largest record we could possibly expect (16384) so the scratch and
 * reassembly buffers cover any record the daemon will send or accept. Each
 * daemon actually reads/sends/accepts only what its own session negotiated
 * (gnutls_record_get_max_size()), which is <= this, so every record stays
 * inside a single TLS fragment and gnutls never splits it. */
#define CT_TLSMAXPAYLOAD 16384
#define CT_CHUNKMAX \
    (CT_TLSMAXPAYLOAD - (size_t)sizeof(ctp_t) - sizeof(ct_transfer_data))

/* Largest record a daemon will accept on a channel: a full-chunk
 * CT_TRANSFER_DATA. Everything the daemon decrypts today must fit in this;
 * bigger is a protocol violation (the connection is dropped). */
#define CT_MAX_RECORD (sizeof(ctp_t) + sizeof(ct_transfer_data) + CT_CHUNKMAX)

#endif /* _CODATUNNEL_CTP_H_ */
