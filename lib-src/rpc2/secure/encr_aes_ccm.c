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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <rpc2/secure.h>
#include "aes.h"
#include "grunt.h"

/* various constants that define the CCM flags, using fixed values according
 * to RFC4309. */
#define AFLAG    (1<<6)	/* do we have additional authenticated data */
#define NONCELEN 11	/* fixed nonce size, 3 byte salt + 8 byte IV */
#define PARM_L   (AES_BLOCK_SIZE - 1 - NONCELEN) /* size of length field == 4 */
#define CCMflags(len) ((((len/2)-1)<<3) | (PARM_L-1))

struct aes_ccm_ctx {
    uint8_t flag_n_salt[4];
    aes_encrypt_ctx ctx;
    unsigned int icv_len;
    int broken_counter;
};

static int init(void **ctx, const uint8_t *key, size_t len,
		size_t icv_len)
{
    struct aes_ccm_ctx *acc = malloc(sizeof(struct aes_ccm_ctx));
    if (!acc) return 0;

    /* copy salt */
    acc->flag_n_salt[3] = key[--len];
    acc->flag_n_salt[2] = key[--len];
    acc->flag_n_salt[1] = key[--len];
    acc->flag_n_salt[0] = CCMflags(icv_len);
    acc->icv_len = icv_len;

    if      (len >= bytes(256)) len = 256;
    else if (len >= bytes(192)) len = 192;
    else if (len >= bytes(128)) len = 128;
    else goto err_out;

    acc->broken_counter = 0;

    if (aes_encrypt_key(key, len, &acc->ctx) == 0) {
	*ctx = acc;
	return 0;
    }

err_out:
    free(acc);
    return -1;
}

void aes_ccm_tweak(void *ctx, uint32_t version)
{
    struct aes_ccm_ctx *acc = ctx;
    /* SECURE version 0 incorrectly initialized the counter block. Because it
     * clears 5 bits of the salt it weakens the strength of the algorithm.
     *
     * To fix this issue, whenever a v0 client connects to a newer server it
     * will force the client to use the slower (but correct) AES-CBC
     * encryption. However when a new client connects to a v0 server, the
     * client will not be able to pick the algorithm that is used to encrypt
     * client-to-server traffic, we will make the server use AES-CBC for
     * server-to-client traffic. Because the server doesn't know any better,
     * we have to fall back on the broken init. */
    acc->broken_counter = (version == 0);
}

static int init8(void **ctx, const uint8_t *key, size_t len)
{
    return init(ctx, key, len, 8);
}

static int init12(void **ctx, const uint8_t *key, size_t len)
{
    return init(ctx, key, len, 12);
}

static int init16(void **ctx, const uint8_t *key, size_t len)
{
    return init(ctx, key, len, 16);
}


static void release(void **ctx)
{
    if (!*ctx) return;
    memset(*ctx, 0, sizeof(struct aes_ccm_ctx));
    free(*ctx);
    *ctx = NULL;
}

static int aes_ccm_crypt(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
			 const uint8_t *iv, const uint8_t *aad,
			 size_t aad_len, int encrypt)
{
    struct aes_ccm_ctx *acc = (struct aes_ccm_ctx *)ctx;
    int i, n, nblocks;
    uint8_t CMAC[AES_BLOCK_SIZE], CTR[AES_BLOCK_SIZE], S0[AES_BLOCK_SIZE];
    uint8_t tmp[AES_BLOCK_SIZE], *p;
    const uint8_t *end = tmp + AES_BLOCK_SIZE, *src;

    if (!encrypt) {
	if (len < acc->icv_len)
	    return 0;
	len -= acc->icv_len;
    }

    if (aad_len) acc->flag_n_salt[0] |= AFLAG;
    else	 acc->flag_n_salt[0] &= ~AFLAG;

    /* initialize CMAC (initial seed for authentication) */
    CMAC[0] = acc->flag_n_salt[0];
    CMAC[1] = acc->flag_n_salt[1];
    CMAC[2] = acc->flag_n_salt[2];
    CMAC[3] = acc->flag_n_salt[3];
    int32(CMAC)[1] = int32(iv)[0];
    int32(CMAC)[2] = int32(iv)[1];
    int32(CMAC)[3] = htonl(len);
    aes_encrypt(CMAC, CMAC, &acc->ctx);

    /* initialize counter block */
    CTR[0] = acc->flag_n_salt[0] & 0x07;
    CTR[1] = acc->flag_n_salt[1];
    CTR[2] = acc->flag_n_salt[2];
    CTR[3] = acc->flag_n_salt[3];
    int32(CTR)[1] = int32(iv)[0];
    int32(CTR)[2] = int32(iv)[1];
    int32(CTR)[3] = 0;

    if (acc->broken_counter) {
	CTR[0] = acc->flag_n_salt[0];
	CTR[3] = acc->flag_n_salt[3] & 0x07;
    }

    /* save the first counter block. we will use that for authentication. */
    aes_encrypt(CTR, S0, &acc->ctx);

    /* authenticate the header (spi and sequence number values) */
    /* ugly that this CCM header is not aligned on a 4-byte boundary
     * we have to copy everything byte-by-byte */

    p = tmp;
    /* length of authenticated data */
#if 0 /* don't know if something like htonll actually exists */
    if (aad_len >= UINT_MAX) {
	uint64_t x = htonll(aad_len);
	*(p++) = 0xff; *(p++) = 0xff;
	memcpy(p, (void *)&x, sizeof(uint64_t));
	p += sizeof(uint64_t);
    } else
#endif
    if (aad_len >= (1<<16) - (1<<8)) {
	uint32_t x = htonl(aad_len);
	*(p++) = 0xff; *(p++) = 0xfe;
	memcpy(p, (void *)&x, sizeof(uint32_t));
	p += sizeof(uint32_t);
    } else {
	uint16_t x = htons(aad_len);
	memcpy(p, (void *)&x, sizeof(uint16_t));
	p += sizeof(uint16_t);
    }

    while (aad_len > 0) {
	n = end - p;
	if (aad_len < n) n = aad_len;
	memcpy(p, aad, n); p += n;
	if (p < end) memset(p, 0, end - p);

	xor128(CMAC, tmp);
	aes_encrypt(CMAC, CMAC, &acc->ctx);

	aad += n; aad_len -= n; p = tmp;
    }

    nblocks = (len + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;
    while (nblocks--)
    {
	/* increment counter */
	for (i = AES_BLOCK_SIZE-1; i >=0; i--)
	    if (++CTR[i] != 0) break;

	/* get the next keystream block */
	aes_encrypt(CTR, tmp, &acc->ctx);

	/* we don't need the counter block anymore, so re-use it so we can zero
	 * pad the last partial input block to the full block size */
	if (!nblocks && len % AES_BLOCK_SIZE) {
	    memset(CTR, 0, AES_BLOCK_SIZE);
	    memcpy(CTR, in, len % AES_BLOCK_SIZE);
	    src = CTR;
	} else
	    src = in;

	/* Checksum the plaintext before we encrypt */
	if (encrypt)
	    xor128(CMAC, src);

	if (out != src)
	    memcpy(out, src, AES_BLOCK_SIZE);
	xor128(out, tmp);

	/* Checksum the plaintext after decryption */
	if (!encrypt)
	    xor128(CMAC, out);

	aes_encrypt(CMAC, CMAC, &acc->ctx);

	in  += AES_BLOCK_SIZE;
	out += AES_BLOCK_SIZE;
    }

    /* finalize the ICV calculation */
    xor128(CMAC, S0);
    if (encrypt) {
	memcpy(out, CMAC, acc->icv_len);
	len += acc->icv_len;
    }
    else if (memcmp(in, CMAC, acc->icv_len) != 0)
	return 0;

    return len;
}

static int encrypt(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
		   uint8_t *iv, const uint8_t *aad, size_t aad_len)
{
    return aes_ccm_crypt(ctx, in, out, len, iv, aad, aad_len, 1);
}

static int decrypt(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
		   const uint8_t *iv, const uint8_t *aad, size_t aad_len)
{
    return aes_ccm_crypt(ctx, in, out, len, iv, aad, aad_len, 0);
}

struct secure_encr secure_ENCR_AES_CCM_8 = {
    .id	          = SECURE_ENCR_AES_CCM_8,
    .name         = "ENCR-AES-CCM-8",
    .encrypt_init = init8,
    .encrypt_free = release,
    .encrypt      = encrypt,
    .decrypt_init = init8,
    .decrypt_free = release,
    .decrypt      = decrypt,
    .min_keysize  = bytes(128) + 3,
    .max_keysize  = bytes(256) + 3,
    .blocksize    = AES_BLOCK_SIZE,
    .iv_len       = 8,
    .icv_len      = 8,
};

struct secure_encr secure_ENCR_AES_CCM_12 = {
    .id	          = SECURE_ENCR_AES_CCM_12,
    .name         = "ENCR-AES-CCM-12",
    .encrypt_init = init12,
    .encrypt_free = release,
    .encrypt      = encrypt,
    .decrypt_init = init12,
    .decrypt_free = release,
    .decrypt      = decrypt,
    .min_keysize  = bytes(128) + 3,
    .max_keysize  = bytes(256) + 3,
    .blocksize    = AES_BLOCK_SIZE,
    .iv_len       = 8,
    .icv_len      = 12,
};

struct secure_encr secure_ENCR_AES_CCM_16 = {
    .id	          = SECURE_ENCR_AES_CCM_16,
    .name         = "ENCR-AES-CCM-16",
    .encrypt_init = init16,
    .encrypt_free = release,
    .encrypt      = encrypt,
    .decrypt_init = init16,
    .decrypt_free = release,
    .decrypt      = decrypt,
    .min_keysize  = bytes(128) + 3,
    .max_keysize  = bytes(256) + 3,
    .blocksize    = AES_BLOCK_SIZE,
    .iv_len       = 8,
    .icv_len      = 16,
};

