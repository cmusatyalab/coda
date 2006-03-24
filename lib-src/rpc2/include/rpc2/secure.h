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

#ifndef _RPC2_SECURE_H_
#define _RPC2_SECURE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>

/* RFC2460 - Internet Protocol, Version 6 (IPv6) Specification
 * Section 5 - Packet Size Issues
 *
 *   a minimal IPv6 implementation (e.g., in a boot ROM) may simply restrict
 *   itself to sending packets no larger than 1280 octets, and omit
 *   implementation of Path MTU Discovery.
 *...
 *   In response to an IPv6 packet that is sent to an IPv4 destination ... the
 *   IPv6 node is not required to reduce the size of subsequent packets to less
 *   than 1280, but must include a Fragment header ... this means the payload
 *   may have to be reduced to 1232 octets (1280 minus 40 for the IPv6 header
 *   and 8 for the Fragment header), and smaller still if additional extension
 *   headers are used.
 *
 * So until someone implements Path MTU Discovery, fragmentation, and
 * reassembly in RPC2, we're just going to follow the minimal IPv6
 * implementation guidelines.
 *
 * We limit the maximum packet size to: IPv6 minimum MTU - sizeof(IPv6 header) -
 * sizeof(Fragment header) - sizeof(UDP header) = 1224
 */
#define MAXPACKETSIZE (1280 - 40 - 8 - 8)
#define MAXIVLEN  32
#define MAXICVLEN 32

/* Identifiers for authentication algorithms (IANA) */
#define SECURE_ENCR_NULL	11
#define SECURE_ENCR_AES_CBC	12
#define SECURE_ENCR_AES_CTR	13
#define SECURE_ENCR_AES_CCM_8	14
#define SECURE_ENCR_AES_CCM_12	15
#define SECURE_ENCR_AES_CCM_16	16
#define SECURE_ENCR_AES_GCM_8	18
#define SECURE_ENCR_AES_GCM_12	19
#define SECURE_ENCR_AES_GCM_16	20

struct secure_crypt {
    const int id;
    const char *name;
    int (*init)(void **ctx, const uint8_t *key);
    void (*release)(void **ctx);
    int (*func)(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
		const uint8_t *iv);
    const size_t min_keysize;
    const size_t max_keysize;
    const size_t blocksize;
    const size_t iv_len;
    const size_t icv_len;
};

/* Identifiers for authentication algorithms (IANA) */
#define SECURE_AUTH_NONE	  0
#define SECURE_AUTH_HMAC_SHA_1_96 2
#define SECURE_AUTH_AES_XCBC_96	  9

struct secure_auth {
    const int id;
    const char *name;
    int (*init)(void **ctx, const uint8_t *key);
    void (*release)(void **ctx);
    void (*func)(void *ctx, const uint8_t *in, size_t len, uint8_t *icv);
    const size_t keysize;
    const size_t icv_len;
};


struct security_association {
    /* incoming packets */
    uint32_t recv_spi;
    uint32_t recv_seq;
    unsigned long recv_win;

    const struct secure_auth *validate;
    void *validate_context;

    const struct secure_crypt *decrypt;
    void *decrypt_context;

    /* outgoing packets */
    uint32_t peer_spi;
    uint32_t peer_seq;

    struct sockaddr_storage peer;
    socklen_t peerlen;
    uint8_t send_iv[MAXIVLEN];

    const struct secure_crypt *encrypt;
    void *encrypt_context;

    const struct secure_auth *authenticate;
    void *authenticate_context;
};


/* initialize */
void secure_init(int verbose);
void secure_release(void);

/* cryptographically strong deterministic pseudo random number generator */
void secure_random_bytes(uint8_t *buf, size_t len);

/* low level socket interface */
ssize_t secure_sendto(int s, const void *buf, size_t len, int flags,
		      /* to/tolen only used to send non-encrypted packets */
		      const struct sockaddr *to, socklen_t tolen,
		      struct security_association *sa);

ssize_t secure_recvfrom(int s, void *buf, size_t len, int flags,
			struct sockaddr *peer, socklen_t *peerlen, /*untrusted*/
			struct security_association **sa,
			struct security_association *(*GETSA)(uint32_t spi));

#endif /* _RPC2_SECURE_H_ */

