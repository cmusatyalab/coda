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

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <rpc2/secure.h>

#include "aes.h"
#include "grunt.h"

static int encrypt_init(void **ctx, const uint8_t *key, size_t len)
{
    *ctx = malloc(sizeof(aes_encrypt_ctx));
    if (!*ctx)
        return -1;

    if (len >= bytes(256))
        len = 256;
    else if (len >= bytes(192))
        len = 192;
    else if (len >= bytes(128))
        len = 128;
    else
        goto err_out;

    if (aes_encrypt_key(key, len, *ctx) == 0)
        return 0;

err_out:
    free(*ctx);
    *ctx = NULL;
    return -1;
}

static int encrypt(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
                   uint8_t *iv, const uint8_t *aad, size_t aad_len)
{
    int n;
    /* CBC mode encryption requires an unpredictable IV, so we encrypt the
     * passed IV block (which is a counter) once. */
    aes_encrypt((aes_block *)iv, (aes_block *)iv, ctx);

    n = aes_cbc_encrypt((aes_block *)in, (aes_block *)out,
                        len / sizeof(aes_block), (aes_block *)iv, ctx);
    return n * sizeof(aes_block);
}

static void encrypt_free(void **ctx)
{
    if (!*ctx)
        return;
    memset(*ctx, 0, sizeof(aes_encrypt_ctx));
    free(*ctx);
    *ctx = NULL;
}

static int decrypt_init(void **ctx, const uint8_t *key, size_t len)
{
    *ctx = malloc(sizeof(aes_decrypt_ctx));
    if (!*ctx)
        return -1;

    if (len >= bytes(256))
        len = 256;
    else if (len >= bytes(192))
        len = 192;
    else if (len >= bytes(128))
        len = 128;
    else
        goto err_out;

    if (aes_decrypt_key(key, len, *ctx) == 0)
        return 0;

err_out:
    free(*ctx);
    *ctx = NULL;
    return -1;
}

static int decrypt(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
                   const uint8_t *iv, const uint8_t *aad, size_t aad_len)
{
    int n;
    n = aes_cbc_decrypt((aes_block *)in, (aes_block *)out,
                        len / sizeof(aes_block), (aes_block *)iv, ctx);
    return n * sizeof(aes_block);
}

static void decrypt_free(void **ctx)
{
    if (!*ctx)
        return;
    memset(*ctx, 0, sizeof(aes_decrypt_ctx));
    free(*ctx);
    *ctx = NULL;
}

struct secure_encr secure_ENCR_AES_CBC = {
    .id           = SECURE_ENCR_AES_CBC,
    .name         = "ENCR-AES-CBC",
    .encrypt_init = encrypt_init,
    .encrypt_free = encrypt_free,
    .encrypt      = encrypt,
    .decrypt_init = decrypt_init,
    .decrypt_free = decrypt_free,
    .decrypt      = decrypt,
    .min_keysize  = bytes(128),
    .max_keysize  = bytes(256),
    .blocksize    = AES_BLOCK_SIZE,
    .iv_len       = AES_BLOCK_SIZE,
};
