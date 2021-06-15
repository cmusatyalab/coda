/* BLURB lgpl
			Coda File System
			    Release 6

	  Copyright (c) 2005-2017 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
#*/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <rpc2/secure.h>

#include "codatunnel/wrapper.h"
#include "grunt.h"

/* RFC 2406 - IP Encapsulating Security Payload (ESP)
 * Section 3.4.3  Sequence Number Verification
 *
 *   All ESP implementations MUST support the anti-replay service, though its
 *   use may be enabled or disabled by the receiver on a per-SA basis. This
 *   service MUST NOT be enabled unless the authentication service also is
 *   enabled for the SA ... For each received packet, the receiver MUST verify
 *   that the packet contains a Sequence Number that does not duplicate the
 *   Sequence Number of any other packets received during the life of this SA.
 *   ... Duplicates are rejected through the use of a sliding receive window.
 *   ... A MINIMUM window size of 32 MUST be supported
 */
/* Return 0 when a packet with this sequence number hasn't been seen before.
 * Return -1 if the sequence number has been seen (or is too old to track) */
static int sequence_number_verification(const struct security_association *sa,
                                        uint32_t seq)
{
    int offset;

    /* no authentication service, avoid anti-replay check, PASS */
    if (!sa->validate && !sa->decrypt)
        return 0;

    offset = (int)sa->recv_seq - (int)seq;

    /* right of the receive window, PASS */
    if (offset < 0) {
        /* Check if seq wrapped around MAX_UINT */
        if (seq < sa->recv_seq)
            return -1;

        return 0;
    }

    /* too old, FAIL */
    /* we already checked if offset is < 0 */
    if (offset >= (int)(sizeof(sa->recv_win) * 8))
        return -1;

    /* duplicate packet, FAIL */
    if (sa->recv_win & (1UL << offset))
        return -1;

    /* not yet received, PASS */
    return 0;
}

/* Packet successfully passed the integrity check so we know it came from
 * a trusted source, we can safely update the receive window and peer address */
static int integrity_check_passed(struct security_association *sa, uint32_t seq,
                                  const struct sockaddr *peer,
                                  socklen_t peerlen)
{
    int offset = (int)sa->recv_seq - (int)seq;

    /* offset was already checked against the window by check_seq, so it is
     * known to be either more recent (< 0) or between 1 and
     * sizeof(sa->recv_win) * 8 */
    if (offset < 0) {
        sa->recv_seq = seq;
        sa->recv_win <<= -offset;
        offset = 0;
    }

    /* check one more time, this might be a duplicate that was received while
     * we were validating the previous packet */
    if (sa->recv_win & 1UL << offset)
        return -1;

    sa->recv_win |= 1UL << offset;

    /* this should never happen, but just in case... */
    if (peerlen > sizeof(sa->peer))
        return 0;

    /* memcmp is probably about as expensive as the memcpy here, the only
     * difference is that without memcmp we always dirty memory, but with all
     * the other things going on the cost is probably not measurable. So there
     * is no reason to try to be smart and check if the addresses are already
     * the same. */
    memcpy(&sa->peer, peer, peerlen);
    sa->peerlen = peerlen;

    return 0;
}

static ssize_t packet_decryption(struct security_association *sa, uint8_t *out,
                                 const uint8_t *in, ssize_t len)
{
    uint8_t i, padlength, next_header;
    const uint8_t *aad, *iv;

    aad = in;
    in += 2 * sizeof(uint32_t);
    len -= 2 * sizeof(uint32_t);

    /* NULL encryption */
    if (!sa->decrypt) {
        memcpy(out, in, len);
        return len;
    }

    iv = in;
    in += sa->decrypt->iv_len;
    len -= sa->decrypt->iv_len;

    len = sa->decrypt->decrypt(sa->decrypt_context, in, out, len, iv, aad,
                               2 * sizeof(uint32_t));
    if (len < 2)
        return -1;

    /* we don't use next_header (yet) */
    next_header = out[--len];
    if (next_header != 0)
        return -1;

    /* check length of padding */
    padlength = out[--len];
    if (padlength > len)
        return -1;

    /* check padding */
    for (i = padlength; i > 0; i--) {
        if (out[--len] != i)
            return -1;
    }
    return len;
}

ssize_t secure_recvfrom(int s, void *buf, size_t len, int flags,
                        struct sockaddr *peer, socklen_t *peerlen,
                        struct security_association **ret_sa,
                        struct security_association *(*GETSA)(uint32_t spi))
{
    uint8_t packet[MAXPACKETSIZE];
    struct sockaddr_storage from;
    socklen_t fromlen               = sizeof(from);
    struct security_association *sa = NULL;
    uint32_t spi                    = 0, seq;
    ssize_t n, estimated_payload;
    int err;

    if (ret_sa)
        *ret_sa = NULL;

    /* validate arguments */
    if (peer && !peerlen) {
        errno = EINVAL;
        return -1;
    }

    if (!peer) {
        peer    = (struct sockaddr *)&from;
        peerlen = &fromlen;
    }

    n = codatunnel_recvfrom(s, packet, MAXPACKETSIZE, flags | MSG_TRUNC, peer,
                            peerlen);
    if (n < 0)
        return n;

    /* If we truncated packets because the packet buffer is too small */
    err = ENOMEM;

    /* if we received a truncated packet, but the caller would assume this
     * packet did not get truncated we have to drop the received packet */
    if (n > MAXPACKETSIZE && n <= len)
        goto drop;

    /* check if we have valid spi & seq */
    if (n >= 8) {
        spi = (packet[0] << 24) | (packet[1] << 16) | (packet[2] << 8) |
              packet[3];
        seq = (packet[4] << 24) | (packet[5] << 16) | (packet[6] << 8) |
              packet[7];
    }

    /* RFC 2406 - IP Encapsulating Security Payload (ESP)
     * Section 2.1  Security Parameters Index
     *   The set of SPI values in the range 1 through 255 are reserved by the
     *   Internet Assigned Numbers Authority (IANA) for future use ... The SPI
     *   value of zero (0) is reserved for local, implementation-specific use
     *   and MUST NOT be sent on the wire.
     */
    /* Since we're not actually IPsec, we use the reserved range for
     * non-encrypted packets */
    if (spi < 256)
        goto not_encrypted;

    /* truncated packets will fail validation, drop them */
    if (n > MAXPACKETSIZE)
        goto drop;

    /* If we dropped the packet because we could not find a matching SA */
    err = ENOENT;

    /* RFC 2406 - IP Encapsulating Security Payload (ESP)
     * Section 3.4.2  Security Association Lookup
     *   If no valid Security Association exists for this session ... the
     *   receiver MUST discard the packet
     */
    sa = GETSA ? GETSA(spi) : NULL;
    if (!sa) {
        secure_audit("SA lookup failed", spi, seq, peer);
        goto drop;
    }

    /* All other errors, drop silently the way the kernel drops incoming
     * packets when the UDP checksum failed */
    err = EAGAIN;

    /* only check sequence numbers if we can validate the received packet */
    if ((sa->validate->icv_len || sa->decrypt->icv_len) &&
        sequence_number_verification(sa, seq) == -1)
        goto drop; /* drop duplicate packets */

    /* rough check if the packet would overflow the receive buffer
     * there may be less data because the sender may add between 0
     * and 255 bytes of padding */
    n -= sa->validate->icv_len;
    estimated_payload = n - (2 * sizeof(uint32_t)) - sa->decrypt->iv_len - 2;
    if (estimated_payload < 0 || (unsigned int)estimated_payload > len)
        /* should we log this, or return a different error? (EMSGSIZE?) */
        goto drop;

    if (sa->validate && sa->validate->icv_len) {
        /* RFC 2406 - IP Encapsulating Security Payload (ESP)
         * Section 3.4.4  Integrity Check Value Verification
         *   If authentication has been selected, the receiver computes the ICV
         *   over the ESP packet minus the Authentication Data ... If the test
         *   fails, then the receiver MUST discard the received IP datagram as
         *   invalid
         */
        uint8_t tmp_icv[MAXICVLEN];
        size_t icv_len;

        /* icv must be aligned on a 32-bit boundary */
        if (n & 3) {
            secure_audit("unaligned ICV", spi, seq, peer);
            goto drop;
        }

        assert(sa->validate->icv_len <= MAXICVLEN);

        /* Perform the ICV computation */
        sa->validate->auth(sa->validate_context, packet, n, tmp_icv);
        icv_len = sa->validate->icv_len;
        if (!secure_compare(packet + n, icv_len, tmp_icv, icv_len)) {
            secure_audit("ICV check failed", spi, seq, peer);
            goto drop;
        }

        if (integrity_check_passed(sa, seq, peer, *peerlen) == -1)
            goto drop; /* drop duplicate packets */
    }

    n = packet_decryption(sa, buf, packet, n);
    if (n < 0) {
        secure_audit("Decryption failed", spi, seq, peer);
        goto drop;
    }

    /* we passed integrity check for combined mode decryption/validation
     * algorithms such as AES-CCM */
    if (!sa->validate->icv_len && sa->decrypt && sa->decrypt->icv_len)
        if (integrity_check_passed(sa, seq, peer, *peerlen) == -1)
            goto drop; /* drop duplicate packets */

    goto done;

not_encrypted:
    if ((ssize_t)len < n)
        n = len;
    if (n > 0)
        memcpy(buf, packet, n);

done:
    if (ret_sa)
        *ret_sa = sa;
    return n;

drop:
    errno = err;
    return -1;
}
