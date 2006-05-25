/* BLURB gpl

			   Coda File System
			      Release 6

	    Copyright (c) 2006 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

			Additional copyrights

#*/

/* codatoken.c -- generate Coda authentication tokens */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include <rpc2/secure.h>
#include <rpc2/rpc2.h>
#include "codatoken.h"
#include "auth2.h"

static void generate_Secret(const struct secure_encr *encr, void *encr_ctx,
			    const struct secure_auth *auth, void *auth_ctx,
			    const uint8_t *key, size_t keylen,
			    const uint8_t *identity, size_t idlen,
			    time_t endtime, uint8_t *token, size_t *tokenlen)
{
    uint8_t *payload, *p, icv[MAXICVLEN];
    size_t len;
    int i;

    /* packet length aligns to next blocksize boundary */
    len = 2 * sizeof(uint32_t) + keylen + idlen;
    len += encr->blocksize - 1;
    len &= ~(encr->blocksize - 1);
    assert(*tokenlen >= encr->iv_len + len + 8);

    /* fill in secret token bits */
    secure_random_bytes(token, encr->iv_len);
    payload = token + encr->iv_len;
    payload[0] = 0xC0; payload[1] = 0xDA;
    payload[2] = keylen; payload[3] = idlen;
    ((uint32_t *)payload)[1] = htonl(endtime);
    p = payload + 2 * sizeof(uint32_t);
    memcpy(p, key, keylen); p += keylen;
    memcpy(p, identity, idlen); p += idlen;

    /* pad tail */
    for (i = 1; p < payload + len; i++) *(p++) = i;

    /* encrypt and authenticate */
    encr->encrypt(encr_ctx, payload, payload, len, token, NULL, 0);
    len += encr->iv_len;
    auth->auth(auth_ctx, token, len, icv);
    memcpy(token + len, icv, 8);
    *tokenlen = len + 8;
}

static int validate_Secret(const struct secure_encr *decr, void *decr_ctx,
			   const struct secure_auth *auth, void *auth_ctx,
			   uint8_t *token, size_t tokenlen,
			   uint8_t **key, size_t *keylen,
			   uint8_t **identity, size_t *idlen,
			   time_t *endtime)
{
    uint8_t icv[MAXICVLEN], *payload = token + decr->iv_len, *p;
    size_t len = tokenlen - 8;
    int i;

    if (tokenlen < decr->iv_len + 2 * sizeof(uint32_t) + 8)
	return -1;

    /* validate checksum */
    auth->auth(auth_ctx, token, len, icv);
    if (memcmp(token + len, icv, 8) != 0) return -1;

    /* decrypt */
    len -= decr->iv_len;
    if (len % decr->blocksize) return -1;
    decr->decrypt(decr_ctx, payload, payload, len, token, NULL, 0);

    /* check magic and validate header */
    if (payload[0] != 0xC0 || payload[1] != 0xDA ||
	2 * sizeof(uint32_t) + payload[2] + payload[3] > len)
	return -1;

    /* check padding */
    p = payload + 2 * sizeof(uint32_t) + payload[2] + payload[3];
    for (i = 1; p < payload + len; i++)
	if (*(p++) != i) return -1;

    /* extract contents */
    *key =  payload + 2 * sizeof(uint32_t);
    *keylen = payload[2];

    *identity = payload + 2 * sizeof(uint32_t) + *keylen;
    *idlen = payload[3];

    *endtime = ntohl(((uint32_t *)payload)[1]);
    return 0;
}

int getauth2key(uint8_t *token, size_t token_size,
		uint8_t auth2key[AUTH2KEYSIZE])
{
    /* make sure we're initialized */
    secure_init(0);
    return secure_pbkdf(token, token_size, NULL, 0, SECURE_PBKDF_ITERATIONS,
			auth2key, AUTH2KEYSIZE);
}

int generate_CodaToken(uint8_t auth2key[AUTH2KEYSIZE], uint32_t viceid,
		       uint32_t lifetime, ClearToken *ctoken,
		       EncryptedSecretToken estoken)
{
    const struct secure_encr *encr;
    const struct secure_auth *auth;
    void *encr_ctx, *auth_ctx;
    uint8_t identity[11];
    size_t idlen, estokenlen;
    int rc;

    /* construct the clear token */
    ctoken->ViceId	   = viceid;
    ctoken->BeginTimestamp = time(0) - 900;
    ctoken->EndTimestamp   = time(0) + lifetime;
    ctoken->AuthHandle	   = -1;
    secure_random_bytes(ctoken->HandShakeKey, sizeof(RPC2_EncryptionKey));

    /* viceid is 32-bits, so we only need 10 characters */
    sprintf((char *)identity, "%u", viceid);
    idlen = strlen((char *)identity);

    /* setup encryption/authentication functions */
    encr = secure_get_encr_byid(SECURE_ENCR_AES_CBC);
    auth = secure_get_auth_byid(SECURE_AUTH_AES_XCBC_96);
    assert(encr && auth);

    assert(encr->max_keysize + auth->keysize <= AUTH2KEYSIZE);
    rc = encr->encrypt_init(&encr_ctx, auth2key, encr->max_keysize);
    assert(!rc);
    rc = auth->auth_init(&auth_ctx, auth2key + encr->max_keysize,auth->keysize);
    assert(!rc);

    estokenlen = sizeof(EncryptedSecretToken);
    generate_Secret(encr, encr_ctx, auth, auth_ctx,
		    ctoken->HandShakeKey, sizeof(RPC2_EncryptionKey),
		    identity, strlen((char *)identity), ctoken->EndTimestamp,
		    (uint8_t *)estoken, &estokenlen);

    /* release encryption/authentication contexts */
    encr->encrypt_free(&encr_ctx);
    auth->auth_free(&auth_ctx);
    return 0;
}

int validate_CodaToken(uint8_t auth2key[AUTH2KEYSIZE],
		       EncryptedSecretToken estoken,
		       uint32_t *viceid, time_t *endtime,
		       RPC2_EncryptionKey *sessionkey)
{
    const struct secure_encr *decr;
    const struct secure_auth *auth;
    void *decr_ctx, *auth_ctx;
    uint8_t *key, *identity;
    size_t keylen, idlen;
    int rc;

    /* setup decryption/authentication functions */
    decr = secure_get_encr_byid(SECURE_ENCR_AES_CBC);
    auth = secure_get_auth_byid(SECURE_AUTH_AES_XCBC_96);
    if (!decr || !auth || decr->max_keysize + auth->keysize > AUTH2KEYSIZE)
	return -1;

    decr->decrypt_init(&decr_ctx, auth2key, decr->max_keysize);
    auth->auth_init(&auth_ctx, auth2key + decr->max_keysize, auth->keysize);

    rc = validate_Secret(decr, decr_ctx, auth, auth_ctx,
			 (uint8_t *)estoken, sizeof(EncryptedSecretToken),
			 &key, &keylen, &identity, &idlen, endtime);
    if (rc || keylen != sizeof(RPC2_EncryptionKey))
	return -1;

    /* null-terminate the identity. Safe, since identity points into the
     * decrypted estoken and there are at least 8 (now) unused checksum bytes */
    identity[idlen] = '\0';
    *viceid = strtoul((char *)identity, NULL, 10);
    memcpy(sessionkey, key, sizeof(RPC2_EncryptionKey));

    /* release decryption/authentication contexts */
    decr->decrypt_free(&decr_ctx);
    auth->auth_free(&auth_ctx);
    return 0;
}

