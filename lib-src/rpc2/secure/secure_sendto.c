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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <rpc2/secure.h>

#include "codatunnel/wrapper.h"
#include "grunt.h"

ssize_t secure_sendto(int s, const void *buf, size_t len, int flags,
                      const struct sockaddr *to, socklen_t tolen,
                      struct security_association *sa)
{
    uint8_t out[MAXPACKETSIZE];
    size_t padded_size;
    ssize_t n;
    int i, pad_align, padding;
    uint8_t *aad, *iv, *payload, *icv;

    if (!sa || (!sa->encrypt && !sa->authenticate)) {
        /* make sure the other side will not mistake this as encrypted */
        if (len >= 2 * sizeof(uint32_t) && ntohl(*((uint32_t *)buf)) >= 256) {
            errno = EINVAL;
            return -1;
        }
        n = len;
        goto send_packet;
    }

    /* check for sequence number overflow */
    sa->peer_seq++;
    if (sa->peer_seq == 0) {
        /* log(sa, "exhausted sequence number space"); */
        sa->peer_seq--;
        errno = EPIPE;
        return -1;
    }

    /* calculate the amount of padding we will need */
    pad_align = (sa->encrypt->blocksize > sizeof(uint32_t) ?
                     sa->encrypt->blocksize :
                     sizeof(uint32_t)) -
                1;
    padded_size = (len + 2 * sizeof(uint8_t) + pad_align) & ~pad_align;
    assert(padded_size - len >= 2 * sizeof(uint8_t));
    padding = padded_size - 2 * sizeof(uint8_t) - len;

    /* check if there is enough room */
    if ((2 * sizeof(uint32_t) + sa->encrypt->iv_len + padded_size +
         sa->authenticate->icv_len) > sizeof(out)) {
        errno = EMSGSIZE;
        return -1;
    }

    /* pack the header */
    aad                  = out;
    ((uint32_t *)aad)[0] = htonl(sa->peer_spi);
    ((uint32_t *)aad)[1] = htonl(sa->peer_seq);
    iv                   = aad + 2 * sizeof(uint32_t);

    /* increment and copy iv */
    if (sa->encrypt->iv_len) {
        for (i = sa->encrypt->iv_len - 1; i >= 0; i--)
            if (++(sa->send_iv[i]))
                break;

        memcpy(iv, sa->send_iv, sa->encrypt->iv_len);
    }
    payload = iv + sa->encrypt->iv_len;

    /* copy payload */
    memcpy(payload, buf, len);

    /* append padding */
    n = len;
    for (i = 1; i <= padding; i++)
        payload[n++] = i;
    payload[n++] = padding; /* length of padding */
    payload[n++] = 0; /* next_header */

    /* encrypt the payload */
    if (sa->encrypt) {
        n = sa->encrypt->encrypt(sa->encrypt_context, payload, payload, n, iv,
                                 aad, 2 * sizeof(uint32_t));
        if (n < 0) {
            errno = EMSGSIZE;
            return -1;
        }
        n += 2 * sizeof(uint32_t) + sa->encrypt->iv_len;
    }

    /* add message authentication */
    if (sa->authenticate) {
        icv = out + n;
        sa->authenticate->auth(sa->authenticate_context, out, n, icv);
        n += sa->authenticate->icv_len;
    }

    buf   = out;
    to    = (struct sockaddr *)&sa->peer;
    tolen = sa->peerlen;

send_packet:
    padding = n - len;
    n       = codatunnel_sendto(s, buf, n, flags, to, tolen);
#ifdef __linux__
    if (n == -1 && errno == ECONNREFUSED) {
        /* On linux ECONNREFUSED is a result of a previous sendto
         * triggering an ICMP bad host/port response. This
         * behaviour seems to be required by RFC1122, but in
         * practice this is not implemented by other UDP stacks.
         * We retry the send, because the failing host was possibly
         * not the one we tried to send to this time. --JH
         */
        n = codatunnel_sendto(s, buf, n, 0, to, tolen);
    }
#endif
    n -= padding;
    if (n < -1)
        n = -1;
    return n;
}
