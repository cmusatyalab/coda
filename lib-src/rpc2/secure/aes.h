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

/* Wrapper around the rijndael (v3.0) reference implementation */

#ifndef _AES_H_
#define _AES_H_

#include <stdint.h>
#include "rijndael-alg-fst.h"

#define AES_MAXROUNDS	MAXNR
#define AES_BLOCK_SIZE  16

typedef struct {
    uint32_t context[4*(AES_MAXROUNDS+1)];
    uint32_t rounds;
} aes_context;

#define aes_encrypt_ctx aes_context
#define aes_decrypt_ctx aes_context

static inline int aes_init(void) { return 0; }

static inline int aes_encrypt_key(const uint8_t *key, int keylen,
				  aes_encrypt_ctx *ctx)
{
    ctx->rounds = (keylen == 128) ? 10 : (keylen == 192) ? 12 : 14;
    return rijndaelKeySetupEnc(ctx->context, key, keylen);
}

static inline int aes_decrypt_key(const uint8_t *key, int keylen,
				  aes_decrypt_ctx *ctx)
{
    ctx->rounds = (keylen == 128) ? 10 : (keylen == 192) ? 12 : 14;
    return rijndaelKeySetupDec(ctx->context, key, keylen);
}

static inline int aes_encrypt(const uint8_t in[AES_BLOCK_SIZE],
			       uint8_t out[AES_BLOCK_SIZE],
			       const aes_context *ctx)
{
    rijndaelEncrypt(ctx->context, ctx->rounds, in, out);
    return 0;
}

static inline int aes_decrypt(const uint8_t in[AES_BLOCK_SIZE],
			       uint8_t out[AES_BLOCK_SIZE],
			       const aes_context *ctx)
{
    rijndaelDecrypt(ctx->context, ctx->rounds, in, out);
    return 0;
}

#endif /* _AES_H_ */

