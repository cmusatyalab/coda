/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 2026 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently
#*/

#include <arpa/inet.h>
#include <stdint.h>
#include <sys/socket.h>

#include "gtest/gtest.h"

#include "ctp.h"

/* The framing struct keeps its historical byte layout: the 8-byte magic word,
   the four 32-bit control words, and the destination address. We only
   repurposed the former is_init0 word into a multi-valued opcode, so the size
   must still be exactly the sum of those fields. The assertion is written
   against the field widths (not a hard-coded total) because
   sizeof(struct sockaddr_storage) is platform dependent. */
TEST(ctp, size_matches_layout)
{
    EXPECT_EQ(sizeof(ctp_t), (size_t)CT_MAGICSZ + 4 * sizeof(uint32_t) +
                                 sizeof(struct sockaddr_storage));
}

/* CT_PKT must stay 0 and CT_INIT0 must stay 1: an old daemon only ever wrote
   that word to 0 (a plain packet) or 1 (INIT0), so byte-for-byte the on-the-wire
   values are identical to today and an old peer never observes another value. */
TEST(ctp, packet_and_init0_opcodes_keep_backward_compatible_values)
{
    EXPECT_EQ(CT_PKT, 0u);
    EXPECT_EQ(CT_INIT0, 1u);
}

/* The opcode crosses the daemon-to-daemon TLS hop in network byte order
   (htonl on send, ntohl on receive, alongside is_retry/msglen). Verify every
   opcode round-trips through that conversion. */
TEST(ctp, every_opcode_survives_honlt_ntohl)
{
    const uint32_t opcodes[] = { CT_PKT,           CT_INIT0,
                                 CT_FILEREG,       CT_FILEUNREG,
                                 CT_FILEDONE,      CT_TRANSFER_READY,
                                 CT_TRANSFER_DATA, CT_TRANSFER_EOF,
                                 CT_TRANSFER_ERROR };
    for (uint32_t op : opcodes)
        EXPECT_EQ(ntohl(htonl(op)), op);
}

/* The file-transfer opcodes are new in 4.1 and must keep stable values across
   daemon versions: a 4.1 daemon streaming to an older peer (or vice versa) only
   stays correct if both sides number the opcodes identically. Lock the values. */
TEST(ctp, file_transfer_opcode_values_are_stable)
{
    EXPECT_EQ(CT_TRANSFER_READY, 5);
    EXPECT_EQ(CT_TRANSFER_DATA, 6);
    EXPECT_EQ(CT_TRANSFER_EOF, 7);
    EXPECT_EQ(CT_TRANSFER_ERROR, 8);
}

/* The data-path payload structs are raw byte layouts (no struct-padding
   surprises, no platform-dependent sizing): two 64-bit fields -> 16 bytes, one
   64-bit -> 8, the 64+32+32 error record -> 16. ct_transfer_data.offset must be 64
   bits so files beyond 4 GiB can be addressed. */
TEST(ctp, data_path_payload_struct_sizes)
{
    EXPECT_EQ(sizeof(ct_transfer_ready), sizeof(uint64_t));
    EXPECT_EQ(sizeof(ct_transfer_data), 2 * sizeof(uint64_t));
    EXPECT_EQ(sizeof(ct_transfer_eof), sizeof(uint64_t));
    EXPECT_EQ(sizeof(ct_transfer_error), 2 * sizeof(uint64_t));
}

/* The "later end" flag is the single bit that disambiguates store-from-fetch
   start timing; lock its value. */
TEST(ctp, filreg_later_flag_bit)
{
    EXPECT_EQ(CT_FILREG_DRIVER, 1);
}

/* The sink-driven pull needs a request: which bytes of the source's file it
 * wants next. Lock the opcode value (next free after CT_TRANSFER_ERROR). */
TEST(ctp, transfer_request_opcode_is_stable)
{
    EXPECT_EQ(CT_TRANSFER_REQUEST, 9);
}

/* Three 64-bit fields: cookie (which transfer), offset (absolute file
 * offset), len (bytes wanted). 24 bytes, no padding surprises. */
TEST(ctp, transfer_request_struct_size)
{
    EXPECT_EQ(sizeof(ct_transfer_request), 3 * sizeof(uint64_t));
}

/* It must round-trip the same byte-conversion as the other transfer opcodes. */
TEST(ctp, transfer_request_in_opcode_list)
{
    EXPECT_EQ(ntohl(htonl(CT_TRANSFER_REQUEST)), CT_TRANSFER_REQUEST);
}
