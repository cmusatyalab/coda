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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <rpc2/secure.h>
#include "aes.h"
#include "grunt.h"
#include "testvectors.h"

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
    int i, carry = 0;

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

/*
 * Statistical random number generator tests defined in
 * FIPS 140-1 - 4.11.1 Power-Up Tests
 *
 *  A single bit stream of 20,000 consecutive bits of output from the
 *  generator is subjected to each of the following tests. If any of the
 *  tests fail, then the module shall enter an error state.
 *
 * The Monobit Test
 *  1. Count the number of ones in the 20,000 bit stream. Denote this
 *     quantity by X.
 *  2. The test is passed if 9,654 < X < 10,346
 *
 * The Poker Test
 *  1. Divide the 20,000 bit stream into 5,000 contiguous 4 bit
 *     segments. Count and store the number of occurrences of each of
 *     the 16 possible 4 bit values. Denote f(i) as the number of each 4
 *     bit value i where 0 < i < 15.
 *  2. Evaluate the following: X = (16/5000) * (Sum[f(i)]^2)-5000
 *  3. The test is passed if 1.03 < X < 57.4
 *
 * The Runs Test
 *  1. A run is defined as a maximal sequence of consecutive bits of
 *     either all ones or all zeros, which is part of the 20,000 bit
 *     sample stream. The incidences of runs (for both consecutive zeros
 *     and consecutive ones) of all lengths ( 1) in the sample stream
 *     should be counted and stored.
 *  2. The test is passed if the number of runs that occur (of lengths 1
 *     through 6) is each within the corresponding interval specified
 *     below. This must hold for both the zeros and ones; that is, all
 *     12 counts must lie in the specified interval. For the purpose of
 *     this test, runs of greater than 6 are considered to be of length 6.
 *       Length of Run			    Required Interval
 *	     1					2,267-2,733
 *	     2					1,079-1,421
 *	     3					502-748
 *	     4					223-402
 *	     5					90-223
 *	     6+					90-223
 *
 * The Long Run Test
 *  1. A long run is defined to be a run of length 34 or more (of either
 *     zeros or ones).
 *  2. On the sample of 20,000 bits, the test is passed if there are NO
 *     long runs.
 */
#define TESTSIZE (20000 / (sizeof(uint32_t) * 8))

static void check_random(int verbose)
{
    uint32_t data[TESTSIZE], val;
    int i, j, idx, failed = 0;
    int ones, f[16], run, odd, longrun;

    secure_random_bytes((uint8_t *)data, sizeof(data));

    /* the tests do not define the 'endianess' of the stream, so
     * I assume little endian */

    /* Monobit Test */
    if (verbose)
	fprintf(stderr, "PRNG monobit test:              ");
    for (ones = 0, i = 0 ; i < TESTSIZE; i++) {
	val = data[i];
	while (val) {
	    if (val & 1) ones++;
	    val >>= 1;
	}
    }
    if (ones <= 9654 || ones >= 10346) {
	fprintf(stderr, "PRNG monobit test FAILED\n");
	failed++;
    } else if (verbose)
	fprintf(stderr, "PASSED\n");

    /* Poker Test */
    if (verbose)
	fprintf(stderr, "PRNG poker test:                ");
    memset(f, 0, sizeof(f));
    for (i = 0 ; i < TESTSIZE; i++) {
	for (j = 0; j < 32; j += 4) {
	    idx = (data[i] >> j) & 0xf;
	    f[idx]++;
	}
    }
    for (val = 0, i = 0; i < 16; i++)
	val += f[i] * f[i];
    assert((val & 0xf0000000) == 0);
    val <<= 4;
    if (val <= 25005150 || val >= 25287000) {
	fprintf(stderr, "PRNG poker test FAILED\n");
	failed++;
    } else if (verbose)
	fprintf(stderr, "PASSED\n");

    /* Runs Test */
    if (verbose)
	fprintf(stderr, "PRNG runs test:                 ");
    memset(f, 0, sizeof(f));
    odd = run = longrun = 0;
    for (i = 0 ; i < TESTSIZE; i++) {
	val = data[i];
	for (j = 0; j < 32; j++) {
	    if (odd ^ (val & 1)) {
		if (run) {
		    if (run > longrun)
			longrun = run;
		    if (run > 6)
			run = 6;
		    idx = run - 1 + (odd ? 6 : 0);
		    f[idx]++;
		}
		odd = val & 1;
		run = 0;
	    }
	    run++;
	    val >>= 1;
	}
    }
    if (run > longrun)
	longrun = run;
    if (run > 6)
	run = 6;
    idx = run - 1 + (odd ? 6 : 0);
    f[idx]++;

    if (f[0] <= 2267 || f[0] >= 2733 || f[6] <= 2267 || f[6] >= 2733 ||
	f[1] <= 1079 || f[1] >= 1421 || f[7] <= 1079 || f[7] >= 1421 ||
	f[2] <= 502  || f[2] >= 748  || f[8] <= 502  || f[8] >= 748 ||
	f[3] <= 223  || f[3] >= 402  || f[9] <= 223  || f[9] >= 402 ||
	f[4] <= 90   || f[4] >= 223  || f[10] <= 90  || f[10] >= 223 ||
	f[5] <= 90   || f[5] >= 223  || f[11] <= 90  || f[11] >= 223)
    {
	fprintf(stderr, "PRNG runs test FAILED\n");
	failed++;
    } else if (verbose)
	fprintf(stderr, "PASSED\n");

    /* Long Run Test */
    if (verbose)
	fprintf(stderr, "PRNG long run test:             ");
    if (longrun >= 34) {
	fprintf(stderr, "PRNG long run test FAILED\n");
	failed++;
    } else if (verbose)
	fprintf(stderr, "PASSED\n");

    if (failed)
	exit(-1);
}

static int initialized;

void secure_init(int verbose)
{
    if (initialized) return;
    initialized++;

    /* Initialize and run the AES test vectors */
    aes_init();
    check_aes_monte_carlo(verbose);
    check_aes_variable_text(verbose);
    check_aes_variable_key(verbose);

    /* Initialize and test the randomness of the PRNG */
    secure_random_init();
    check_random(verbose);
}

void secure_release(void)
{
    secure_random_release();
    initialized = 0;
}

