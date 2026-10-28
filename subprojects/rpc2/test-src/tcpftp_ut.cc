/* BLURB gpl

                            Coda File System
                               Release 8

           Copyright (c) 2026 Carnegie Mellon University
                   Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  General Public Licence, as shown in the file LICENSE.
The technical and financial contributors to Coda are listed in the file
CREDITS.

                        Additional copyrights
                           none currently
#*/

#include <arpa/inet.h>
#include <string.h>

#include <gtest/gtest.h>

#include <rpc2/rpc2.h>
#include <rpc2/sftp.h>
#include <rpc2/tcpftp.h>

#include "ctp.h"

/* The daemon's ct_hton64/ct_ntoh64 are static in codatunneld.c, so they are
 * not exported through ctp.h for the unit build; mirror them here so the
 * wire-layout assertions can run against the real struct. */
static uint64_t ct_hton64(uint64_t v)
{
    uint32_t hi = htonl((uint32_t)(v >> 32));
    uint32_t lo = htonl((uint32_t)v);
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t ct_ntoh64(uint64_t v)
{
    uint32_t hi = ntohl((uint32_t)(v >> 32));
    uint32_t lo = ntohl((uint32_t)v);
    return ((uint64_t)hi << 32) | lo;
}

/* The param block carries ONLY the correlation cookie. Everything else about
 * a transfer (file identity, tag, direction, offset, length, and any in-VM
 * buffers) is resolved locally at each end from its own SE descriptor, so it
 * never crosses the wire. A distinct 8-byte cookie is round-tripped and the
 * tests assert that exactly those 8 bytes (and nothing else) are written. */

TEST(tcpftp, blocksize_is_eight)
{
    /* The block is a single 8-byte cookie for every supported form. */
    EXPECT_EQ(tcpftp_param_blocksize(), 8u);
}

TEST(tcpftp, pack_writes_eight_bytes)
{
    const uint64_t cookie = 0x0102030405060708ULL;
    unsigned char buf[16] = { 0 };
    size_t wrote          = 0;
    ASSERT_EQ(tcpftp_pack_param_block(&cookie, buf, sizeof(buf), &wrote), 0);
    EXPECT_EQ(wrote, 8u);
    /* The cookie is carried as its 8 native bytes (the SE body is opaque to
     * RPC2, so no byte-order conversion): exactly the in-memory bytes. */
    EXPECT_EQ(sizeof(cookie), 8u);
    EXPECT_EQ(memcmp(buf, &cookie, 8), 0);
    EXPECT_EQ(buf[8], 0); /* nothing past the 8-byte cookie */
}

TEST(tcpftp, roundtrip_only_cookie_crosses)
{
    const uint64_t cookie = 0x1122334455667788ULL;
    unsigned char buf[16];
    size_t wrote = 0;
    ASSERT_EQ(tcpftp_pack_param_block(&cookie, buf, sizeof(buf), &wrote), 0);

    /* The destination mirrors a real receiving end: unpack must recover the
     * cookie exactly and touch nothing else (there is nothing else). */
    uint64_t in = 0xdeadULL;
    ASSERT_EQ(tcpftp_unpack_param_block(buf, wrote, &in), 0);
    EXPECT_EQ(in, cookie);
}

TEST(tcpftp, pack_rejects_undersized_buffer)
{
    const uint64_t cookie = 0x1ULL;
    unsigned char tiny[7]; /* one byte short of the 8-byte cookie */
    size_t wrote = 0;
    EXPECT_EQ(tcpftp_pack_param_block(&cookie, tiny, sizeof(tiny), &wrote), -1);
}

TEST(tcpftp, unpack_rejects_short_buffer)
{
    unsigned char small[7];
    memset(small, 0, sizeof(small));
    uint64_t in = 0;
    EXPECT_EQ(tcpftp_unpack_param_block(small, sizeof(small), &in), -1);
}

TEST(tcpftp, unpack_rejects_trailing_bytes)
{
    const uint64_t cookie = 0x5ULL;
    unsigned char buf[16];
    size_t wrote = 0;
    ASSERT_EQ(tcpftp_pack_param_block(&cookie, buf, sizeof(buf), &wrote), 0);
    uint64_t in = 0;
    /* Exactly one 8-byte block: a 9th byte means malformed input. */
    EXPECT_EQ(tcpftp_unpack_param_block(buf, wrote + 1, &in), -1);
    /* ...and the same 8 bytes unpack cleanly. */
    EXPECT_EQ(tcpftp_unpack_param_block(buf, wrote, &in), 0);
}

TEST(tcpftp, unpack_null_safety)
{
    const uint64_t cookie = 0x9ULL;
    unsigned char buf[8];
    size_t wrote = 0;
    uint64_t in  = 0;
    EXPECT_EQ(tcpftp_pack_param_block(NULL, buf, sizeof(buf), &wrote), -1);
    EXPECT_EQ(tcpftp_pack_param_block(&cookie, NULL, sizeof(buf), &wrote), -1);
    EXPECT_EQ(tcpftp_pack_param_block(&cookie, buf, sizeof(buf), NULL), -1);
    ASSERT_EQ(tcpftp_pack_param_block(&cookie, buf, sizeof(buf), &wrote), 0);
    EXPECT_EQ(tcpftp_unpack_param_block(NULL, wrote, &in), -1);
    EXPECT_EQ(tcpftp_unpack_param_block(buf, wrote, NULL), -1);
}

TEST(tcpftp, capability_flag_does_not_collide)
{
    EXPECT_EQ(TCPFTP_CAPABLE, 0x20u);
    EXPECT_EQ(RPC2SEC_CAPABLE, 0x10u);
    EXPECT_EQ(TCPFTP_PIGGY_CAPABLE, 0x40u);
    EXPECT_EQ(TCPFTP_CAPABLE & RPC2SEC_CAPABLE, 0u);
    EXPECT_EQ(TCPFTP_PIGGY_CAPABLE & (TCPFTP_CAPABLE | RPC2SEC_CAPABLE), 0u);
}

/* The SFTP->TCPFTP upgrade is opt-in: RPC2_TCPFTP is read once in RPC2_Init
 * into the rpc2_tcpftp global, and rpc2_tcpftp_capable() ANDs it with
 * codatunnel_enabled(). The gate defaults off, so a peer must never
 * advertise capability out of the box. codatunnel_enabled() is 0 in the unit
 * build (no daemon), so we pin the default-off contract here; the "on" path
 * needs a running codatunneld and is covered by the ctest harness. */
TEST(tcpftp, upgrade_gate_default_off)
{
    extern int rpc2_tcpftp;
    const int saved = rpc2_tcpftp;
    rpc2_tcpftp     = 0; /* default: RPC2_TCPFTP unset */
    EXPECT_EQ(rpc2_tcpftp_capable(), 0);
    rpc2_tcpftp = saved;
}

/* A codasrv codatunneld is server-mode: it only accepts tunnels and never
 * initiates one, so a server-to-server RPC (e.g. resolution) must not
 * negotiate TCPFTP. If it did, the side effect would register a transfer on a
 * peer for which no daemon-to-daemon channel can exist, and block forever.
 * Only an initiating daemon (venus) may advertise. */
TEST(tcpftp, server_daemon_must_not_advertise)
{
    EXPECT_EQ(rpc2_tcpftp_advertise(1, 0), 1); /* client, gate on */
    EXPECT_EQ(rpc2_tcpftp_advertise(1, 1), 0); /* server, gate on */
    EXPECT_EQ(rpc2_tcpftp_advertise(0, 0), 0); /* client, gate off */
    EXPECT_EQ(rpc2_tcpftp_advertise(0, 1), 0); /* server, gate off */
}

/* The source registration length is the size past the seek offset, capped at
 * ByteQuota only when it is positive. The <= 0 values mean unlimited, matching
 * classic SFTP's "> 0" quota tests; in particular 0 (an app that memsets its
 * SE and never sets a quota, e.g. rs_ShipLogs) must ship the whole file, not
 * zero bytes. A sink always registers 0. */
TEST(tcpftp, reglen_unlimited_convention)
{
    struct SFTP_Descriptor d;

    memset(&d, 0, sizeof(d));
    d.SeekOffset = 100;

    /* quota 0: unlimited (the rs_ShipLogs regression) */
    d.ByteQuota = 0;
    EXPECT_EQ(tcpftp_reglen(&d, TCPFTP_ROLE_SOURCE, 1000), 900u);

    /* quota -1: unlimited */
    d.ByteQuota = -1;
    EXPECT_EQ(tcpftp_reglen(&d, TCPFTP_ROLE_SOURCE, 1000), 900u);

    /* no seek */
    d.SeekOffset = 0;
    EXPECT_EQ(tcpftp_reglen(&d, TCPFTP_ROLE_SOURCE, 1000), 1000u);

    /* seek past the end: nothing to send */
    d.SeekOffset = 1000;
    EXPECT_EQ(tcpftp_reglen(&d, TCPFTP_ROLE_SOURCE, 1000), 0u);
    d.SeekOffset = 0;

    /* sink always registers 0 */
    EXPECT_EQ(tcpftp_reglen(&d, TCPFTP_ROLE_SINK, 1000), 0u);
}

TEST(tcpftp, reglen_positive_quota_caps)
{
    struct SFTP_Descriptor d;

    memset(&d, 0, sizeof(d));
    d.SeekOffset = 0;

    /* quota below the size: capped */
    d.ByteQuota = 400;
    EXPECT_EQ(tcpftp_reglen(&d, TCPFTP_ROLE_SOURCE, 1000), 400u);

    /* quota above the size: no effect */
    d.ByteQuota = 2000;
    EXPECT_EQ(tcpftp_reglen(&d, TCPFTP_ROLE_SOURCE, 1000), 1000u);

    /* quota applies past the seek offset */
    d.SeekOffset = 100;
    d.ByteQuota  = 400;
    EXPECT_EQ(tcpftp_reglen(&d, TCPFTP_ROLE_SOURCE, 1000), 400u);
}

/* The REQUEST body carries three 64-bit network-order fields; the peeloff
 * reads cookie at +0, offset at +8, len at +16. */
TEST(tcpftp, request_wire_layout)
{
    ct_transfer_request t;
    memset(&t, 0, sizeof(t));
    t.cookie     = ct_hton64(0x1122334455667788ULL);
    t.offset     = ct_hton64(0x1000);
    t.len        = ct_hton64(CT_CHUNKMAX);
    uint8_t *raw = (uint8_t *)&t;
    EXPECT_EQ(ct_ntoh64(*(uint64_t *)raw), 0x1122334455667788ULL);
    EXPECT_EQ(ct_ntoh64(*(uint64_t *)(raw + 8)), 0x1000ULL);
    EXPECT_EQ(ct_ntoh64(*(uint64_t *)(raw + 16)), (uint64_t)CT_CHUNKMAX);
}

/* The piggyback flag contract shared by the SFTP and TCPFTP side effects:
 * TCPFTP_PIGGY must be exactly SFTP_PIGGY so that sftp_AddPiggy (which sets
 * the flag and SEDataOffset itself) and the receivers' flag tests behave
 * identically in both side effects, and it must be disjoint from SFTP_TCPFTP:
 * a packet carries either the piggy region or the 8-byte cookie, never both. */
TEST(tcpftp, piggy_flag_contract)
{
    EXPECT_EQ(TCPFTP_PIGGY, SFTP_PIGGY);
    EXPECT_EQ(TCPFTP_PIGGY, 0x2u);
    EXPECT_EQ(SFTP_TCPFTP, 0x40u);
    EXPECT_EQ(TCPFTP_PIGGY & SFTP_TCPFTP, 0u);
}

/* Piggy region layout: with the flag set, the file's bytes occupy
 * Body[SEDataOffset..BodyLength) with no per-file framing. The zero-byte
 * file is a legal degenerate case (SEDataOffset == BodyLength), not a "no
 * piggy" marker -- the flag, not the region, says a file is present. The
 * complementary cookie case carries exactly one 8-byte param block. */
TEST(tcpftp, piggy_region_layout)
{
    /* The header tag is only complete through the enclosing buffer type,
     * so drive the layout off a real (unallocated) packet buffer. */
    RPC2_PacketBuffer pb;

    memset(&pb, 0, sizeof(pb));

    /* Nonzero piggy: 3 bytes of ordinary body, then 5 bytes of file. */
    pb.Header.SEFlags      = TCPFTP_PIGGY;
    pb.Header.SEDataOffset = 3;
    pb.Header.BodyLength   = 8;
    EXPECT_EQ((unsigned long)pb.Header.BodyLength -
                  (unsigned long)pb.Header.SEDataOffset,
              5u);

    /* Zero-byte piggy: the region shrinks to nothing, flag still set. */
    pb.Header.BodyLength = 3;
    EXPECT_EQ((unsigned long)pb.Header.BodyLength -
                  (unsigned long)pb.Header.SEDataOffset,
              0u);
    EXPECT_EQ(pb.Header.SEFlags & TCPFTP_PIGGY, TCPFTP_PIGGY);

    /* Cookie case: no piggy flag; the region is exactly the 8-byte block. */
    pb.Header.SEFlags    = SFTP_TCPFTP;
    pb.Header.BodyLength = 11;
    EXPECT_EQ((unsigned long)pb.Header.BodyLength -
                  (unsigned long)pb.Header.SEDataOffset,
              8u);
}

/* CT_FILEPROGRESS is the daemon->app per-chunk progress: a new opcode
 * appended after the original 0-9 block (the explicit value 10 keeps every
 * earlier opcode stable for backward compatibility), and its vside body
 * reuses the exact 24-byte layout of the CT_FILEDONE body so the demux size
 * check stays symmetric. The body is written in native byte order (the app
 * half of the vside memcpys it straight into the struct), so the field
 * offsets are plain struct offsets: cookie at +0, running bytes at +8. */
TEST(tcpftp, fileprogress_wire_layout)
{
    EXPECT_EQ(CT_FILEPROGRESS, 10u);
    EXPECT_EQ(CT_FILEPROGRESS, (int)CT_TRANSFER_REQUEST + 1);

    /* Same size as the terminal body... */
    EXPECT_EQ(sizeof(ct_fileprogress), sizeof(ct_filedone));
    EXPECT_EQ(sizeof(ct_fileprogress), 24u);

    /* ...and the same field offsets. */
    ct_fileprogress pr;
    memset(&pr, 0, sizeof(pr));
    pr.cookie    = 0x0102030405060708ULL;
    pr.nbytes    = 0x1000;
    pr.status    = 0;
    pr.pad       = 0;
    uint8_t *raw = (uint8_t *)&pr;
    EXPECT_EQ(*(uint64_t *)raw, 0x0102030405060708ULL);
    EXPECT_EQ(*(uint64_t *)(raw + 8), 0x1000ULL);
    EXPECT_EQ(*(uint32_t *)(raw + 16), 0u);
    EXPECT_EQ(*(uint32_t *)(raw + 20), 0u);
}
