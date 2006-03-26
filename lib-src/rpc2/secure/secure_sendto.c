/* BLURB lgpl
			Coda File System
			    Release 6

	  Copyright (c) 2005-2006 Carnegie Mellon University
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
#include "grunt.h"

ssize_t secure_sendto(int s, const void *buf, size_t len, int flags,
		      const struct sockaddr *to, socklen_t tolen,
		      struct security_association *sa)
{
    uint8_t out[MAXPACKETSIZE];
    size_t padded_size;
    ssize_t n;
    int i, pad_align, padding;
    uint8_t *p, *icv;

    if (!sa || (!sa->encrypt && !sa->authenticate)) {
	/* make sure the other side will not mistake this as encrypted */
	if (*(uint32_t *)buf >= 256) {
	    errno = EINVAL;
	    return -1;
	}
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
		 sa->encrypt->blocksize : sizeof(uint32_t)) - 1;
    padded_size = (len + 2 * sizeof(uint8_t) + pad_align) & ~pad_align;
    assert(padded_size - len >= 2 * sizeof(uint8_t));
    padding = padded_size - 2 * sizeof(uint8_t) - len;

    /* check if there is enough room */
    if ((2 * sizeof(uint32_t) + sa->encrypt->iv_len + padded_size + sa->authenticate->icv_len) > sizeof(out))
    {
	errno = EMSGSIZE;
	return -1;
    }

    /* pack the header */
    p = out;
    *int32(p) = htonl(sa->peer_spi); p += sizeof(uint32_t);
    *int32(p) = htonl(sa->peer_seq); p += sizeof(uint32_t);

    /* increment and copy iv */
    if (sa->encrypt->iv_len) {
	for (i = sa->encrypt->iv_len - 1; i >= 0; i--)
	    if (++(sa->send_iv[i]))
		break;

	memcpy(p, sa->send_iv, sa->encrypt->iv_len);
	p += sa->encrypt->iv_len;
    }

    /* copy payload */
    memcpy(p, buf, len);

    /* append padding */
    for (i = 1; i <= padding; i++)
	p[len++] = i;
    p[len++] = padding; /* length of padding */
    p[len++] = 0;       /* next_header */

    /* encrypt the payload */
    if (sa->encrypt) {
	n = sa->encrypt->encrypt(sa->encrypt_context,
				 p, p, len, out + 2 * sizeof(uint32_t));
	if (n < 0) {
	    errno = EMSGSIZE;
	    return -1;
	}
	len = n + 2 * sizeof(uint32_t) + sa->encrypt->iv_len;
    }

    /* add message authentication */
    if (sa->authenticate) {
	icv = out + len;
	sa->authenticate->auth(sa->authenticate_context, out, len, icv);
	len += sa->authenticate->icv_len;
    }

    buf = out;
    to = (struct sockaddr *)&sa->peer;
    tolen = sa->peerlen;

send_packet:
    n = sendto(s, buf, len, flags, to, tolen);
#ifdef __linux__
    if (n == -1 && errno == ECONNREFUSED)
    {
	/* On linux ECONNREFUSED is a result of a previous sendto
	 * triggering an ICMP bad host/port response. This
	 * behaviour seems to be required by RFC1122, but in
	 * practice this is not implemented by other UDP stacks.
	 * We retry the send, because the failing host was possibly
	 * not the one we tried to send to this time. --JH
	 */
	n = sendto(s, buf, len, 0, to, tolen);
    }
#endif
    return n;
}
