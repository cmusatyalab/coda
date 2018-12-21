/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2006-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
#*/

/* Implementation of AES modes of operation and test vectors */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <rpc2/secure.h>

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
int aes_cbc_encrypt(const aes_block *in, aes_block *out, size_t nblocks,
                    const aes_block *iv, aes_encrypt_ctx *ctx)
{
    int i;
    for (i = 0; i < nblocks; i++) {
        out[i].u64[0] = in[i].u64[0] ^ iv->u64[0];
        out[i].u64[1] = in[i].u64[1] ^ iv->u64[1];

        aes_encrypt(&out[i], &out[i], ctx);

        iv = &out[i];
    }
    return nblocks;
}

int aes_cbc_decrypt(const aes_block *in, aes_block *out, size_t nblocks,
                    const aes_block *iv, aes_decrypt_ctx *ctx)
{
    /* go backwards from the end to avoid an extra copy on every iteration */
    int i;
    for (i = nblocks - 1; i > 0; i--) {
        aes_decrypt(&in[i], &out[i], ctx);
        xor128(&out[i], &in[i - 1]);
    }
    aes_decrypt(&in[0], &out[0], ctx);
    xor128(&out[0], iv);

    return nblocks;
}

/* AES-based pseudo random function
 *
 * RFC 4434: The AES-XCBC-PRF-128 Algorithm for the Internet Key Exchange
 *	     Protocol (IKE)
 */
int aes_xcbc_prf_init(void **ctx, const uint8_t *key, size_t len)
{
    aes_block tmp;
    int rc;

    if (len != sizeof(aes_block)) {
        memset(tmp.u8, 0, sizeof(aes_block));

        if (len > sizeof(aes_block)) {
            /* long input key, use the digest as prf key */
            if (aes_xcbc_mac_init(ctx, tmp.u8, sizeof(aes_block)))
                return -1;
            aes_xcbc_mac_128(*ctx, key, len, &tmp);
            aes_xcbc_mac_release(ctx);
        } else
            /* short input key, use zero padded key as prf key */
            memcpy(tmp.u8, key, len);

        key = tmp.u8;
    }

    rc = aes_xcbc_mac_init(ctx, key, sizeof(aes_block));
    if (len != sizeof(aes_block))
        memset(tmp.u8, 0, sizeof(aes_block));

    return rc;
}
/* #define aes_xcbc_prf_release aes_xcbc_mac_release
 * #define aes_xcbc_prf_128	aes_xcbc_mac_128 */

/* check test vectors */
static void check_aes_monte_carlo(int verbose)
{
    int i, j, k, keysize[] = { 128, 192, 256 };
    int runs = sizeof(aes_ecb_em) / AES_BLOCK_SIZE / 3;
    uint8_t ekey[bytes(256)];
    uint8_t dkey[bytes(256)];
    aes_block ebuf, elast, dbuf, dlast;
    uint64_t *ep, *dp;

    aes_encrypt_ctx ectx;
    aes_decrypt_ctx dctx;

    const char *etestvector = aes_ecb_em;
    const char *dtestvector = aes_ecb_dm;

    /* run both encryption and decryption tests in parallel */
    for (k = 0; k < 3; k++) {
        if (verbose)
            fprintf(stderr, "AES%d monte carlo test:        ", keysize[k]);
        memset(ekey, 0, bytes(256));
        memset(ebuf.u8, 0, AES_BLOCK_SIZE);
        memset(dkey, 0, bytes(256));
        memset(dbuf.u8, 0, AES_BLOCK_SIZE);

        for (i = 0; i < runs; i++) {
            aes_encrypt_key(ekey, keysize[k], &ectx);
            aes_decrypt_key(dkey, keysize[k], &dctx);

            for (j = 0; j < 9999; j++) {
                aes_encrypt(&ebuf, &ebuf, &ectx);
                aes_decrypt(&dbuf, &dbuf, &dctx);
            }

            memcpy(elast.u8, ebuf.u8, AES_BLOCK_SIZE);
            memcpy(dlast.u8, dbuf.u8, AES_BLOCK_SIZE);

            /* and encrypt/decrypt once more to hit 10000 */
            aes_encrypt(&ebuf, &ebuf, &ectx);
            aes_decrypt(&dbuf, &dbuf, &dctx);

            if (memcmp(ebuf.u8, etestvector, AES_BLOCK_SIZE) != 0 ||
                memcmp(dbuf.u8, dtestvector, AES_BLOCK_SIZE) != 0) {
                fprintf(stderr, "AES monte carlo test FAILED\n");
                abort();
            }

            /* XOR last keysize bits of the ciphertext with the key */
            ep = (uint64_t *)ekey;
            dp = (uint64_t *)dkey;
            switch (keysize[k]) {
            case 256:
                *(ep++) ^= elast.u64[0];
                *(dp++) ^= dlast.u64[0];
            case 192:
                *(ep++) ^= elast.u64[1];
                *(dp++) ^= dlast.u64[1];
            default:
                xor128((aes_block *)ep, &ebuf);
                xor128((aes_block *)dp, &dbuf);
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
    int runs = sizeof(aes_ecb_vt) / sizeof(aes_block) / 3;

    uint8_t key[bytes(256)];
    aes_block text, buf;
    const char *testvector = aes_ecb_vt;
    aes_encrypt_ctx ctx;

    for (k = 0; k < 3; k++) {
        if (verbose)
            fprintf(stderr, "AES%d variable plaintext test: ", keysize[k]);
        memset(key, 0, bytes(256));
        memset(text.u8, 0, sizeof(aes_block));
        text.u8[0] = 0x80;

        aes_encrypt_key(key, keysize[k], &ctx);
        for (i = 0; i < runs; i++) {
            aes_encrypt(&text, &buf, &ctx);

            if (memcmp(buf.u8, testvector, sizeof(aes_block)) != 0) {
                fprintf(stderr, "AES variable plaintext test FAILED\n");
                abort();
            }
            testvector += sizeof(aes_block);

            shift_right(text.u8, sizeof(aes_block));
        }
        if (verbose)
            fprintf(stderr, "PASSED\n");
    }
}

static void check_aes_variable_key(int verbose)
{
    int i, k, keysize[] = { 128, 192, 256 };
    int runs, tests;

    uint8_t key[bytes(256)];
    aes_block text, buf;
    const char *testvector = aes_ecb_vk;
    aes_encrypt_ctx ctx;

    /* annoyingly there are only 128 tests for the 128-bit key, but up to 256
   * for the 256-bit keys. So we have to do some figuring out how many loops
   * we should really make */
    tests = sizeof(aes_ecb_vk) / sizeof(aes_block);
    if (tests <= 384)
        runs = tests / 3;
    else if (tests <= 512)
        runs = (tests - 128) / 2;
    else
        runs = tests - 320;

    for (k = 0; k < 3; k++) {
        if (verbose)
            fprintf(stderr, "AES%d variable key test:       ", keysize[k]);
        memset(key, 0, bytes(256));
        memset(text.u8, 0, sizeof(aes_block));
        key[0] = 0x80;

        for (i = 0; i < runs; i++) {
            aes_encrypt_key(key, keysize[k], &ctx);
            aes_encrypt(&text, &buf, &ctx);

            if (memcmp(buf.u8, testvector, sizeof(aes_block)) != 0) {
                fprintf(stderr, "AES variable key tests FAILED\n");
                abort();
            }
            testvector += sizeof(aes_block);

            if (shift_right(key, bytes(keysize[k])))
                break;
        }
        if (verbose)
            fprintf(stderr, "PASSED\n");
    }
}

/* test vectors for AES-CBC from RFC 3602 */
static const uint8_t aes_cbc_key1[] =
    "\x06\xa9\x21\x40\x36\xb8\xa1\x5b\x51\x2e\x03\xd5\x34\x12\x00\x06";
static const uint8_t aes_cbc_iv1[] =
    "\x3d\xaf\xba\x42\x9d\x9e\xb4\x30\xb4\x22\xda\x80\x2c\x9f\xac\x41";
static const uint8_t aes_cbc_pt1[] = "Single block msg";
static const uint8_t aes_cbc_ct1[] =
    "\xe3\x53\x77\x9c\x10\x79\xae\xb8\x27\x08\x94\x2d\xbe\x77\x18\x1a";

static const uint8_t aes_cbc_key2[] =
    "\xc2\x86\x69\x6d\x88\x7c\x9a\xa0\x61\x1b\xbb\x3e\x20\x25\xa4\x5a";
static const uint8_t aes_cbc_iv2[] =
    "\x56\x2e\x17\x99\x6d\x09\x3d\x28\xdd\xb3\xba\x69\x5a\x2e\x6f\x58";
static const uint8_t aes_cbc_pt2[] =
    "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
    "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f";
static const uint8_t aes_cbc_ct2[] =
    "\xd2\x96\xcd\x94\xc2\xcc\xcf\x8a\x3a\x86\x30\x28\xb5\xe1\xdc\x0a"
    "\x75\x86\x60\x2d\x25\x3c\xff\xf9\x1b\x82\x66\xbe\xa6\xd6\x1a\xb1";

static const uint8_t aes_cbc_key3[] =
    "\x6c\x3e\xa0\x47\x76\x30\xce\x21\xa2\xce\x33\x4a\xa7\x46\xc2\xcd";
static const uint8_t aes_cbc_iv3[] =
    "\xc7\x82\xdc\x4c\x09\x8c\x66\xcb\xd9\xcd\x27\xd8\x25\x68\x2c\x81";
static const uint8_t aes_cbc_pt3[] =
    "This is a 48-byte message (exactly 3 AES blocks)";
static const uint8_t aes_cbc_ct3[] =
    "\xd0\xa0\x2b\x38\x36\x45\x17\x53\xd4\x93\x66\x5d\x33\xf0\xe8\x86"
    "\x2d\xea\x54\xcd\xb2\x93\xab\xc7\x50\x69\x39\x27\x67\x72\xf8\xd5"
    "\x02\x1c\x19\x21\x6b\xad\x52\x5c\x85\x79\x69\x5d\x83\xba\x26\x84";

static const uint8_t aes_cbc_key4[] =
    "\x56\xe4\x7a\x38\xc5\x59\x89\x74\xbc\x46\x90\x3d\xba\x29\x03\x49";
static const uint8_t aes_cbc_iv4[] =
    "\x8c\xe8\x2e\xef\xbe\xa0\xda\x3c\x44\x69\x9e\xd7\xdb\x51\xb7\xd9";
static const uint8_t aes_cbc_pt4[] =
    "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
    "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
    "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
    "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf";
static const uint8_t aes_cbc_ct4[] =
    "\xc3\x0e\x32\xff\xed\xc0\x77\x4e\x6a\xff\x6a\xf0\x86\x9f\x71\xaa"
    "\x0f\x3a\xf0\x7a\x9a\x31\xa9\xc6\x84\xdb\x20\x7e\xb0\xef\x8e\x4e"
    "\x35\x90\x7a\xa6\x32\xc3\xff\xdf\x86\x8b\xb7\xb2\x9d\x3d\x46\xad"
    "\x83\xce\x9f\x9a\x10\x2e\xe9\x9d\x49\xa5\x3e\x87\xf4\xc3\xda\x55";

/* test vectors for AES-CBC from NIST special publication 800-38A */
static const uint8_t aes_cbc128_key[] =
    "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c";
/* static const uint8_t aes_cbc128_iv[16] = 000102...0d0e0f */
static const uint8_t aes_cbc128_pt[] =
    "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a"
    "\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51"
    "\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef"
    "\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10";
static const uint8_t aes_cbc128_ct[] =
    "\x76\x49\xab\xac\x81\x19\xb2\x46\xce\xe9\x8e\x9b\x12\xe9\x19\x7d"
    "\x50\x86\xcb\x9b\x50\x72\x19\xee\x95\xdb\x11\x3a\x91\x76\x78\xb2"
    "\x73\xbe\xd6\xb8\xe3\xc1\x74\x3b\x71\x16\xe6\x9e\x22\x22\x95\x16"
    "\x3f\xf1\xca\xa1\x68\x1f\xac\x09\x12\x0e\xca\x30\x75\x86\xe1\xa7";

static const uint8_t aes_cbc192_key[] =
    "\x8e\x73\xb0\xf7\xda\x0e\x64\x52\xc8\x10\xf3\x2b\x80\x90\x79\xe5"
    "\x62\xf8\xea\xd2\x52\x2c\x6b\x7b";
/* static const uint8_t aes_cbc192_iv[16] = 000102...0d0e0f */
#define aes_cbc192_pt aes_cbc128_pt
static const uint8_t aes_cbc192_ct[] =
    "\x4f\x02\x1d\xb2\x43\xbc\x63\x3d\x71\x78\x18\x3a\x9f\xa0\x71\xe8"
    "\xb4\xd9\xad\xa9\xad\x7d\xed\xf4\xe5\xe7\x38\x76\x3f\x69\x14\x5a"
    "\x57\x1b\x24\x20\x12\xfb\x7a\xe0\x7f\xa9\xba\xac\x3d\xf1\x02\xe0"
    "\x08\xb0\xe2\x79\x88\x59\x88\x81\xd9\x20\xa9\xe6\x4f\x56\x15\xcd";

static const uint8_t aes_cbc256_key[] =
    "\x60\x3d\xeb\x10\x15\xca\x71\xbe\x2b\x73\xae\xf0\x85\x7d\x77\x81"
    "\x1f\x35\x2c\x07\x3b\x61\x08\xd7\x2d\x98\x10\xa3\x09\x14\xdf\xf4";
/* static const uint8_t aes_cbc256_iv[16] = 000102...0d0e0f */
#define aes_cbc256_pt aes_cbc128_pt
static const uint8_t aes_cbc256_ct[] =
    "\xf5\x8c\x4c\x04\xd6\xe5\xf1\xba\x77\x9e\xab\xfb\x5f\x7b\xfb\xd6"
    "\x9c\xfc\x4e\x96\x7e\xdb\x80\x8d\x67\x9f\x77\x7b\xc6\x70\x2c\x7d"
    "\x39\xf2\x33\x69\xa9\xd9\xba\xcf\xa5\x30\xe2\x63\x04\x23\x14\x61"
    "\xb2\xeb\x05\xe2\xc3\x9b\xe9\xfc\xda\x6c\x19\x07\x8c\x6a\x9d\x1b";

static int check_aes_cbc_vector(const uint8_t *key, size_t keylen,
                                const aes_block *iv, const uint8_t *pt,
                                const uint8_t *ct, size_t nblocks)
{
    aes_encrypt_ctx ectx;
    aes_decrypt_ctx dctx;
    aes_block buf[4];

    aes_encrypt_key(key, keylen, &ectx);
    aes_cbc_encrypt((aes_block *)pt, buf, nblocks, iv, &ectx);
    if (memcmp(buf[0].u8, ct, nblocks * sizeof(aes_block)) != 0)
        return 1;

    aes_decrypt_key(key, keylen, &dctx);
    aes_cbc_decrypt(buf, buf, nblocks, iv, &dctx);
    if (memcmp(buf[0].u8, pt, nblocks * sizeof(aes_block)) != 0)
        return 1;

    return 0;
}

static void check_aes_cbc(int verbose)
{
    aes_block iv;
    int i, rc = 0;

    if (verbose)
        fprintf(stderr, "AES-CBC test vectors:           ");

    /* RFC 3602 AES-CBC test vectors */
    rc += check_aes_cbc_vector(aes_cbc_key1, 128, (aes_block *)aes_cbc_iv1,
                               aes_cbc_pt1, aes_cbc_ct1, 1);
    rc += check_aes_cbc_vector(aes_cbc_key2, 128, (aes_block *)aes_cbc_iv2,
                               aes_cbc_pt2, aes_cbc_ct2, 2);
    rc += check_aes_cbc_vector(aes_cbc_key3, 128, (aes_block *)aes_cbc_iv3,
                               aes_cbc_pt3, aes_cbc_ct3, 3);
    rc += check_aes_cbc_vector(aes_cbc_key4, 128, (aes_block *)aes_cbc_iv4,
                               aes_cbc_pt4, aes_cbc_ct4, 4);

    /* NIST AES-CBC test vectors */
    for (i = 0; i < sizeof(aes_block); i++)
        iv.u8[i] = i;
    rc += check_aes_cbc_vector(aes_cbc128_key, 128, &iv, aes_cbc128_pt,
                               aes_cbc128_ct, 4);
    rc += check_aes_cbc_vector(aes_cbc192_key, 192, &iv, aes_cbc192_pt,
                               aes_cbc192_ct, 4);
    rc += check_aes_cbc_vector(aes_cbc256_key, 256, &iv, aes_cbc256_pt,
                               aes_cbc256_ct, 4);
    if (rc) {
        fprintf(stderr, "AES-CBC test vectors FAILED\n");
        abort();
    }

    if (verbose)
        fprintf(stderr, "PASSED\n");
}

/* test vectors for AES-XCBC-PRF-128 from RFC 4434 */
static void check_aes_xcbc_prf(int verbose)
{
    uint8_t key[20], input[20];
    aes_block output;
    const char *PRV;
    void *ctx;
    int i, rc = 0;

    if (verbose)
        fprintf(stderr, "AES-XCBC-PRF-128 test vectors:  ");

    /* setup keys and inputs */
    for (i = 0; i < 20; i++)
        key[i] = input[i] = i;
    key[16] = 0xed;
    key[17] = 0xcb;

    PRV = "\x47\xf5\x1b\x45\x64\x96\x62\x15\xb8\x98\x5c\x63\x05\x5e\xd3\x08";
    aes_xcbc_prf_init(&ctx, key, 16);
    aes_xcbc_prf_128(ctx, input, 20, &output);
    aes_xcbc_prf_release(&ctx);
    rc = (memcmp(output.u8, PRV, sizeof(aes_block)) != 0);

    PRV = "\x0f\xa0\x87\xaf\x7d\x86\x6e\x76\x53\x43\x4e\x60\x2f\xdd\xe8\x35";
    aes_xcbc_prf_init(&ctx, key, 10);
    aes_xcbc_prf_128(ctx, input, 20, &output);
    aes_xcbc_prf_release(&ctx);
    rc += (memcmp(output.u8, PRV, sizeof(aes_block)) != 0);

    PRV = "\x8c\xd3\xc9\x3a\xe5\x98\xa9\x80\x30\x06\xff\xb6\x7c\x40\xe9\xe4";
    aes_xcbc_prf_init(&ctx, key, 18);
    aes_xcbc_prf_128(ctx, input, 20, &output);
    aes_xcbc_prf_release(&ctx);
    rc += (memcmp(output.u8, PRV, sizeof(aes_block)) != 0);

    if (rc) {
        fprintf(stderr, "AES-XCBC-PRF-128 test vectors FAILED\n");
        abort();
    }

    if (verbose)
        fprintf(stderr, "PASSED\n");
}

void secure_aes_init(int verbose)
{
    static int initialized = 0;

    if (initialized)
        return;
    initialized++;

/* Initialize */
#ifdef AES_INIT_FUNC
    AES_INIT_FUNC;
#endif

    /* run the AES test vectors */
    check_aes_monte_carlo(verbose);
    check_aes_variable_text(verbose);
    check_aes_variable_key(verbose);
    check_aes_cbc(verbose);
    check_aes_xcbc_prf(verbose);
}
