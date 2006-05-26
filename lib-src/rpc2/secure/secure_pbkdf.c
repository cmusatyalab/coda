/* BLURB lgpl
			Coda File System
			    Release 6

	    Copyright (c) 2006 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
#*/

/*
 * RFC 2898: PKCS #5: Password-Based Cryptography Specification Version 2.0
 *
 * Using AES-XCBC-PRF-128 as the pseudo random function.
 */

#include <string.h>
#include <stdlib.h>

#include <rpc2/secure.h>
#include "aes.h"
#include "grunt.h"

static void F(void *ctx, uint8_t *U0, size_t U0len, uint32_t i,
	      size_t iterations, uint8_t prv[AES_BLOCK_SIZE])
{
    uint8_t UN[AES_BLOCK_SIZE];
    size_t n;

    i = htonl(i);
    memcpy(U0 + U0len - sizeof(uint32_t), &i, sizeof(uint32_t));

    aes_xcbc_prf_128(ctx, U0, U0len, UN);
    memcpy(prv, UN, AES_BLOCK_SIZE);

    for (n = 1; n < iterations; n++) {
	aes_xcbc_prf_128(ctx, UN, AES_BLOCK_SIZE, UN);
	xor128(prv, UN);
    }
}

/* Password Based Key Derivation Function (PKCS #5 version 2) */
int secure_pbkdf(const uint8_t *password, size_t plen,
		 const uint8_t *salt, size_t slen, size_t iterations,
		 uint8_t *key, size_t keylen)
{
    size_t U0len, nblocks = keylen / AES_BLOCK_SIZE;
    uint8_t *U0;
    uint32_t i = 1;
    void *ctx;

    U0len = slen + sizeof(uint32_t);
    U0 = malloc(U0len);
    if (!U0) return -1;

    if (aes_xcbc_prf_init(&ctx, password, plen)) {
	free(U0);
	return -1;
    }

    /* recommended minimum number of iterations is 1000 */
    if (iterations < 1000) iterations = 1000;

    memset(U0, 0, U0len);
    if (salt && slen)
	memcpy(U0, salt, slen);

    for (i = 1; i <= nblocks; i++) {
	F(ctx, U0, U0len, i, iterations, key);
	key += AES_BLOCK_SIZE;
	keylen -= AES_BLOCK_SIZE;
    }
    if (keylen) {
	uint8_t tmp[AES_BLOCK_SIZE];
	F(ctx, U0, U0len, i, iterations, tmp);
	memcpy(key, tmp, keylen);
	memset(tmp, 0, AES_BLOCK_SIZE);
    }
    aes_xcbc_prf_release(&ctx);
    memset(U0, 0, U0len);
    free(U0);
    return 0;
}

