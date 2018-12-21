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

/* Wrapper around the rijndael (AES) implementation by Mike Scott */
#ifndef _AES_H_
#define _AES_H_

#include <stdint.h>

#define AES_MAXROUNDS MAXNR
#define AES_BLOCK_SIZE 16

typedef struct {
    uint32_t context[120 * sizeof(uint32_t)];
    uint8_t Nk, Nb, Nr;
    uint32_t rounds;
} aes_context;
#define aes_encrypt_ctx aes_context
#define aes_decrypt_ctx aes_context

/* Define this to the function used to setup tables during initialization */
#define AES_INIT_FUNC gentables()

/* arghh. this code uses globals... */
extern int Nb, Nr, Nk;
extern unsigned int fkey[120], rkey[120];

static inline int aes_encrypt_key(const uint8_t *key, int keylen,
                                  aes_encrypt_ctx *ctx)
{
    ctx->rounds = (keylen == 128) ? 10 : (keylen == 192) ? 12 : 14;
    gkey(AES_BLOCK_SIZE / sizeof(uint32_t), keylen / 8 / sizeof(uint32_t), key);

    ctx->Nk = Nk;
    ctx->Nb = Nb;
    ctx->Nr = Nr;
    memcpy(ctx->context, fkey, 120 * sizeof(uint32_t));
    return 0;
}

static inline int aes_decrypt_key(const uint8_t *key, int keylen,
                                  aes_decrypt_ctx *ctx)
{
    ctx->rounds = (keylen == 128) ? 10 : (keylen == 192) ? 12 : 14;
    gkey(AES_BLOCK_SIZE / sizeof(uint32_t), keylen / 8 / sizeof(uint32_t), key);

    ctx->Nk = Nk;
    ctx->Nb = Nb;
    ctx->Nr = Nr;
    memcpy(ctx->context, rkey, 120 * sizeof(uint32_t));
    return 0;
}

static inline int aes_encrypt(const uint8_t in[AES_BLOCK_SIZE],
                              uint8_t out[AES_BLOCK_SIZE],
                              const aes_encrypt_ctx *ctx)
{
    Nk = ctx->Nk;
    Nb = ctx->Nb;
    Nr = ctx->Nr;
    memcpy(fkey, ctx->context, 120 * sizeof(uint32_t));

    /* and I guess it also only supports in-place encryption/decryption */
    if (out != in)
        memcpy(out, in, AES_BLOCK_SIZE);
    encrypt(out);
    return 0;
}

static inline int aes_decrypt(const uint8_t in[AES_BLOCK_SIZE],
                              uint8_t out[AES_BLOCK_SIZE],
                              const aes_decrypt_ctx *ctx)
{
    Nk = ctx->Nk;
    Nb = ctx->Nb;
    Nr = ctx->Nr;
    memcpy(rkey, ctx->context, 120 * sizeof(uint32_t));

    if (out != in)
        memcpy(out, in, AES_BLOCK_SIZE);
    decrypt(out);
    return 0;
}

#endif /* _AES_H_ */
