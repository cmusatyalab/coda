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
//#define MAXPACKETSIZE (1280 - 40 - 8 - 8)

/* sigh, the default ValidateAttrsPlusSHA packet (21 piggybacked fids) is
 * exactly 1376 bytes, so we cannot be a drop-in replacement for existing
 * clients and servers unless we support a larger MTU.
 * I guess we'll keep the MTU the same as that used by RPC2 for now. */
#define MAXPACKETSIZE (4500)

#define MAXIVLEN  32
#define MAXICVLEN 32

/* Identifiers for authentication algorithms (IANA) */
#define SECURE_ENCR_NULL		11
#define SECURE_ENCR_AES_CBC		12
//#define SECURE_ENCR_AES_CTR		13
#define SECURE_ENCR_AES_CCM_8		14
#define SECURE_ENCR_AES_CCM_12		15
#define SECURE_ENCR_AES_CCM_16		16
//#define SECURE_ENCR_AES_GCM_8		18
//#define SECURE_ENCR_AES_GCM_12	19
//#define SECURE_ENCR_AES_GCM_16	20

struct secure_encr {
    const int id;
    const char *name;
    int  (*encrypt_init)(void **ctx, const uint8_t *key, size_t len);
    void (*encrypt_free)(void **ctx);
    int  (*encrypt)(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
		    uint8_t *iv, const uint8_t *aad, size_t aad_len);
    int  (*decrypt_init)(void **ctx, const uint8_t *key, size_t len);
    void (*decrypt_free)(void **ctx);
    int  (*decrypt)(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
		    const uint8_t *iv, const uint8_t *aad, size_t aad_len);
    const size_t min_keysize;
    const size_t max_keysize;
    const size_t blocksize;
    const size_t iv_len;
    const size_t icv_len;
};

/* Identifiers for authentication algorithms (IANA) */
#define SECURE_AUTH_NONE		0
//#define SECURE_AUTH_HMAC_SHA_1_96	2
#define SECURE_AUTH_AES_XCBC_96		9

struct secure_auth {
    const int id;
    const char *name;
    int  (*auth_init)(void **ctx, const uint8_t *key, size_t len);
    void (*auth_free)(void **ctx);
    void (*auth)(void *ctx, const uint8_t *in, size_t len, uint8_t *icv);
    const size_t keysize;
    const size_t icv_len;
};


struct security_association {
/* incoming packets */

    /* identifier to match an incoming packet with the the correct logical
     * connection. Really only here for the convenience of the application,
     * it is not used by secure_recvfrom. Security identifiers < 256 are
     * considered 'reserved', see secure_sendto/secure_recvfrom */
    uint32_t recv_spi;

    /* The following are used for to detect replay attacks and should be
     * initialized to 0 */
    uint32_t recv_seq;
    unsigned long recv_win;

    /* function descriptor and state for packet validation */
    const struct secure_auth *validate;
    void *validate_context;

    /* function descriptor and state for decryption */
    const struct secure_encr *decrypt;
    void *decrypt_context;

/* outgoing packets */
    /* remote connection identifier */
    uint32_t peer_spi;

    /* sequence number used for outgoing packets, should be initialized to 0 */
    uint32_t peer_seq;

    /* trusted address of the peer, outgoing encrypted packets will be sent to
     * this address, this will be updates when we receive a packet that
     * is correctly validated */
    struct sockaddr_storage peer;
    socklen_t peerlen;

    /* initialization vector/counter, should be initialized to a random value,
     * secure_sendto will properly increment it */
    uint8_t send_iv[MAXIVLEN];

    /* function descriptor and context for encryption */
    const struct secure_encr *encrypt;
    void *encrypt_context;

    /* function descriptor and context for packet authentication */
    const struct secure_auth *authenticate;
    void *authenticate_context;
};


/* initialization */
void secure_init(int verbose);
void secure_release(void);

const struct secure_auth *secure_get_auth_byid(int id);
const struct secure_encr *secure_get_encr_byid(int id);

/* version 0 - was using incorrect AES-CCM counter block initialization */
#define SECURE_VERSION 1
int secure_setup_encrypt(uint32_t secure_version,
			 struct security_association *sa,
			 const struct secure_auth *authenticate,
			 const struct secure_encr *encrypt,
			 const uint8_t *key, size_t len);
int secure_setup_decrypt(uint32_t secure_version,
			 struct security_association *sa,
			 const struct secure_auth *validate,
			 const struct secure_encr *decrypt,
			 const uint8_t *key, size_t len);

/* Password based key derivation function */
#define SECURE_PBKDF_ITERATIONS 10000 /* see comments in secure_aes.c */
int secure_pbkdf(const uint8_t *password, size_t plen,
		 const uint8_t *salt, size_t slen, size_t iterations,
		 uint8_t *key, size_t keylen);

/* cryptographically strong deterministic pseudo random number generator */
void secure_random_bytes(void *buf, size_t len);

/* low level socket interface */
ssize_t secure_sendto(int s, const void *buf, size_t len, int flags,
		      /* to/tolen only used to send non-encrypted packets */
		      const struct sockaddr *to, socklen_t tolen,
		      struct security_association *sa);

ssize_t secure_recvfrom(int s, void *buf, size_t len, int flags,
			struct sockaddr *peer, socklen_t *peerlen, /*untrusted*/
			struct security_association **sa,
			struct security_association *(*GETSA)(uint32_t spi));

/* time-constant comparison */
int secure_compare(const void *user_data, size_t user_len,
                   const void *secret, size_t secret_len);

#endif /* _RPC2_SECURE_H_ */

