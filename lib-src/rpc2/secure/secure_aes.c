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

/* helper functions for AES */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "aes.h"
#include "grunt.h"
#include "testvectors.h"

/* Simple cbc encrypt/decrypt implementation,
 * - Assumes buffers are aligned on a 4-byte boundary.
 * - Assumes length is a multiple of AES_BLOCK_SIZE.
 * - Allows for in-place encryption (in == out).
 * - Does not modify iv.
 * - Minimizes data copies.
 */
int aes_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t len,
		    const uint8_t *iv, aes_encrypt_ctx *ctx)
{
    int blocks = len / AES_BLOCK_SIZE;
    assert((len % AES_BLOCK_SIZE) == 0);
    while (blocks--)
    {
	int32(out)[0] = int32(in)[0] ^ int32(iv)[0];
	int32(out)[1] = int32(in)[1] ^ int32(iv)[1];
	int32(out)[2] = int32(in)[2] ^ int32(iv)[2];
	int32(out)[3] = int32(in)[3] ^ int32(iv)[3];

	aes_encrypt(out, out, ctx);

	iv = out;
	in += AES_BLOCK_SIZE;
	out += AES_BLOCK_SIZE;
    }
    return len;
}

int aes_cbc_decrypt(const uint8_t *in, uint8_t *out, size_t len,
		    const uint8_t *iv, aes_decrypt_ctx *ctx)
{
    int blocks = (len / AES_BLOCK_SIZE) - 1;
    assert((len % AES_BLOCK_SIZE) == 0);

    /* go backwards from the end to avoid an extra copy on every iteration */
    in  += blocks * AES_BLOCK_SIZE;
    out += blocks * AES_BLOCK_SIZE;
    while (blocks--)
    {
	aes_decrypt(in, out, ctx);
	in -= AES_BLOCK_SIZE;
	xor128(out, in);
	out -= AES_BLOCK_SIZE;
    }
    aes_decrypt(in, out, ctx);
    xor128(out, iv);

    return len;
}


/* check test vectors */
static void check_aes_monte_carlo(int verbose)
{
    int i, j, k, keysize[] = { 128, 192, 256 };
    int runs = sizeof(aes_ecb_em) / AES_BLOCK_SIZE / 3;
    uint8_t ekey[bytes(256)], ebuf[AES_BLOCK_SIZE], elast[AES_BLOCK_SIZE];
    uint8_t dkey[bytes(256)], dbuf[AES_BLOCK_SIZE], dlast[AES_BLOCK_SIZE];
    uint32_t *ep, *dp;

    aes_encrypt_ctx ectx;
    aes_decrypt_ctx dctx;

    const char *etestvector = aes_ecb_em;
    const char *dtestvector = aes_ecb_dm;

    /* run both encryption and decryption tests in parallel */
    for (k = 0; k < 3; k++) {
	if (verbose)
	    fprintf(stderr, "AES%d monte carlo test:        ", keysize[k]);
	memset(ekey, 0, bytes(256)); memset(ebuf, 0, AES_BLOCK_SIZE);
	memset(dkey, 0, bytes(256)); memset(dbuf, 0, AES_BLOCK_SIZE);

	for (i = 0; i < runs; i++) {
	    aes_encrypt_key(ekey, keysize[k], &ectx);
	    aes_decrypt_key(dkey, keysize[k], &dctx);

	    for (j = 0; j < 10000; j++) {
		aes_encrypt(ebuf, ebuf, &ectx);
		aes_decrypt(dbuf, dbuf, &dctx);
		if (j == 9998) {
		    memcpy(elast, ebuf, AES_BLOCK_SIZE);
		    memcpy(dlast, dbuf, AES_BLOCK_SIZE);
		}
	    }

	    if (memcmp(ebuf, etestvector, AES_BLOCK_SIZE) != 0 ||
	        memcmp(ebuf, etestvector, AES_BLOCK_SIZE) != 0)
	    {
		fprintf(stderr, "AES monte carlo test FAILED\n");
		exit(-1);
	    }

	    /* XOR last keysize bits of the ciphertext with the key */
	    ep = (uint32_t *)ekey;
	    dp = (uint32_t *)dkey;
	    switch(keysize[k]) {
	    case 256:
		*(ep++) ^= int32(elast)[0]; *(ep++) ^= int32(elast)[1];
		*(dp++) ^= int32(dlast)[0]; *(dp++) ^= int32(dlast)[1];
	    case 192:
		*(ep++) ^= int32(elast)[2]; *(ep++) ^= int32(elast)[3];
		*(dp++) ^= int32(dlast)[2]; *(dp++) ^= int32(dlast)[3];
	    default:
		xor128(ep, ebuf);
		xor128(dp, dbuf);
	    }
	    etestvector += AES_BLOCK_SIZE;
	    dtestvector += AES_BLOCK_SIZE;
	}
	if (verbose)
	    fprintf(stderr, "PASSED\n");
    }
}

/* not very efficient, but it should work as-is on both big and little endian
 * systems. */
static int shift_right(uint8_t *buf, size_t buflen)
{
    unsigned int i;
    int carry = 0;

    for (i = 0; i < buflen; i++) {
	if (carry) {
	    carry = 0;
	    buf[i] |= 0x80;
	} else {
	    carry = buf[i] & 0x1;
	    buf[i] >>= 1;
	}
    }
    return carry;
}

static void check_aes_variable_text(int verbose)
{
    int i, k, keysize[] = { 128, 192, 256 };
    int runs = sizeof(aes_ecb_vt) / AES_BLOCK_SIZE / 3;

    uint8_t key[bytes(256)], text[AES_BLOCK_SIZE], buf[AES_BLOCK_SIZE];
    const char *testvector = aes_ecb_vt;
    aes_encrypt_ctx ctx;

    for (k = 0; k < 3; k++) {
	if (verbose)
	    fprintf(stderr, "AES%d variable plaintext test: ", keysize[k]);
	memset(key, 0, bytes(256));
	memset(text, 0, AES_BLOCK_SIZE);
	text[0] = 0x80;

	aes_encrypt_key(key, keysize[k], &ctx);
	for (i = 0; i < runs; i++) {
	    aes_encrypt(text, buf, &ctx);

	    if (memcmp(buf, testvector, AES_BLOCK_SIZE) != 0)
	    {
		fprintf(stderr, "AES variable plaintext test FAILED\n");
		exit(-1);
	    }
	    testvector += AES_BLOCK_SIZE;

	    shift_right(text, AES_BLOCK_SIZE);
	}
	if (verbose)
	    fprintf(stderr, "PASSED\n");
    }
}

static void check_aes_variable_key(int verbose)
{
    int i, k, keysize[] = { 128, 192, 256 };
    int runs, tests;

    uint8_t key[bytes(256)], text[AES_BLOCK_SIZE], buf[AES_BLOCK_SIZE];
    const char *testvector = aes_ecb_vk;
    aes_encrypt_ctx ctx;

    /* annoyingly there are only 128 tests for the 128-bit key, but up to 256
     * for the 256-bit keys. So we have to do some figuring out how many loops
     * we should really make */
    tests = sizeof(aes_ecb_vk) / AES_BLOCK_SIZE;
    if (tests <= 384)      runs = tests / 3;
    else if (tests <= 512) runs = (tests - 128) / 2;
    else                   runs = tests - 320;

    for (k = 0; k < 3; k++) {
	if (verbose)
	    fprintf(stderr, "AES%d variable key test:       ", keysize[k]);
	memset(key, 0, bytes(256));
	memset(text, 0, AES_BLOCK_SIZE);
	key[0] = 0x80;

	for (i = 0; i < runs; i++) {
	    aes_encrypt_key(key, keysize[k], &ctx);
	    aes_encrypt(text, buf, &ctx);

	    if (memcmp(buf, testvector, AES_BLOCK_SIZE) != 0)
	    {
		fprintf(stderr, "AES variable key tests FAILED\n");
		exit(-1);
	    }
	    testvector += AES_BLOCK_SIZE;

	    if (shift_right(key, keysize[k]/8))
		break;
	}
	if (verbose)
	    fprintf(stderr, "PASSED\n");
    }
}

void secure_aes_init(int verbose)
{
    static int initialized = 0;

    if (initialized) return;
    initialized++;

    /* Initialize */
#ifdef AES_INIT_FUNC
    AES_INIT_FUNC;
#endif

    /* run the AES test vectors */
    check_aes_monte_carlo(verbose);
    check_aes_variable_text(verbose);
    check_aes_variable_key(verbose);
}

