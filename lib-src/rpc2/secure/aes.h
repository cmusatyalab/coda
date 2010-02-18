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

typedef uint64_t aes_block[AES_BLOCK_SIZE/sizeof(uint64_t)];

typedef struct {
    uint32_t context[4*(AES_MAXROUNDS+1)];
    uint32_t rounds;
} aes_context;
#define aes_encrypt_ctx aes_context
#define aes_decrypt_ctx aes_context

/* Define this to the function used to setup tables during initialization */
/* #define AES_INIT_FUNC */

static inline int aes_encrypt_key(const uint8_t *key, int keylen,
				  aes_encrypt_ctx *ctx)
{
    ctx->rounds = rijndaelKeySetupEnc(ctx->context, key, keylen);
    return 0;
}

static inline int aes_decrypt_key(const uint8_t *key, int keylen,
				  aes_decrypt_ctx *ctx)
{
    ctx->rounds = rijndaelKeySetupDec(ctx->context, key, keylen);
    return 0;
}

static inline int aes_encrypt(const aes_block in, aes_block out,
			      const aes_encrypt_ctx *ctx)
{
    rijndaelEncrypt(ctx->context, ctx->rounds,
		    (uint8_t *)in, (uint8_t *)out);
    return 0;
}

static inline int aes_decrypt(const aes_block in, aes_block out,
			      const aes_decrypt_ctx *ctx)
{
    rijndaelDecrypt(ctx->context, ctx->rounds,
		    (const uint8_t *)in, (uint8_t *)out);
    return 0;
}

#endif /* _AES_H_ */

