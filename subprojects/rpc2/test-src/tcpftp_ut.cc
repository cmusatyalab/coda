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
    EXPECT_EQ(TCPFTP_CAPABLE & RPC2SEC_CAPABLE, 0u);
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
