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

static int init_encrypt(void **ctx, const uint8_t *key, size_t len)
{
    *ctx = malloc(sizeof(aes_encrypt_ctx));
    if (!*ctx) return 0;

    if      (len >= bytes(256)) len = 256;
    else if (len >= bytes(192)) len = 192;
    else if (len >= bytes(128)) len = 128;
    else goto err_out;

    if (aes_encrypt_key(key, len, *ctx) == 0)
	return 1;

err_out:
    free(*ctx);
    *ctx = NULL;
    return 0;
}

static int encrypt(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
		   const uint8_t *iv)
{
    return aes_cbc_encrypt(in, out, len, iv, ctx);
}

static void release_encrypt(void **ctx)
{
    memset(*ctx, 0, sizeof(aes_encrypt_ctx));
    free(*ctx);
    *ctx = NULL;
}


static int init_decrypt(void **ctx, const uint8_t *key, size_t len)
{
    *ctx = malloc(sizeof(aes_decrypt_ctx));
    if (!*ctx) return 0;

    if      (len >= bytes(256)) len = 256;
    else if (len >= bytes(192)) len = 192;
    else if (len >= bytes(128)) len = 128;
    else goto err_out;

    if (aes_decrypt_key(key, len, *ctx) == 0)
	return 1;

err_out:
    free(*ctx);
    *ctx = NULL;
    return 0;
}

static int decrypt(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
		   const uint8_t *iv)
{
    return aes_cbc_decrypt(in, out, len, iv, ctx);
}

static void release_decrypt(void **ctx)
{
    memset(*ctx, 0, sizeof(aes_decrypt_ctx));
    free(*ctx);
    *ctx = NULL;
}


struct secure_crypt encrypt_aes_cbc = {
    .id	         = SECURE_ENCR_AES_CBC,
    .name        = "ENCR-AES-CBC",
    .init        = init_encrypt,
    .release     = release_encrypt,
    .func        = encrypt,
    .min_keysize = bytes(128),
    .max_keysize = bytes(256),
    .blocksize   = AES_BLOCK_SIZE,
    .iv_len      = AES_BLOCK_SIZE,
};

struct secure_crypt decrypt_aes_cbc = {
    .id	         = SECURE_ENCR_AES_CBC,
    .name        = "ENCR-AES-CBC",
    .init        = init_decrypt,
    .release     = release_decrypt,
    .func        = decrypt,
    .min_keysize = bytes(128),
    .max_keysize = bytes(256),
    .blocksize   = AES_BLOCK_SIZE,
    .iv_len      = AES_BLOCK_SIZE,
};

