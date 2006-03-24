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

#include <stdlib.h>
#include <string.h>

#include <rpc2/secure.h>
#include "aes.h"
#include "grunt.h"

#define ICV_LEN bytes(96)

struct aes_xcbc_state {
    aes_encrypt_ctx C1;
    uint8_t K2[AES_BLOCK_SIZE];
    uint8_t K3[AES_BLOCK_SIZE];
};

static int init(void **ctx, const uint8_t *key)
{
    struct aes_xcbc_state *state;
    uint8_t tmp[AES_BLOCK_SIZE];
    aes_encrypt_ctx c;

    state = malloc(sizeof(struct aes_xcbc_state));
    if (!state) return -1;

    aes_encrypt_key(key, 128, &c);

    memset(tmp, 0x01, AES_BLOCK_SIZE);
    aes_encrypt(tmp, tmp, &c);
    aes_encrypt_key(tmp, AES_BLOCK_SIZE * 8, &state->C1);

    memset(tmp, 0x02, AES_BLOCK_SIZE);
    aes_encrypt(tmp, state->K2, &c);

    memset(tmp, 0x03, AES_BLOCK_SIZE);
    aes_encrypt(tmp, state->K3, &c);

    memset(tmp, 0, AES_BLOCK_SIZE);
    memset(&c, 0, sizeof(aes_encrypt_ctx));

    *ctx = state;
    return 0;
}

static void release(void **ctx)
{
    struct aes_xcbc_state *state = *ctx;
    memset(&state->C1, 0, sizeof(aes_encrypt_ctx));
    memset(&state->K2, 0, AES_BLOCK_SIZE);
    memset(&state->K3, 0, AES_BLOCK_SIZE);
    free(state);
    *ctx = NULL;
}

static void auth(void *ctx, const uint8_t *buf, size_t len, uint8_t *icv)
{
    struct aes_xcbc_state *state = ctx;
    uint8_t iv[AES_BLOCK_SIZE], tmp[AES_BLOCK_SIZE];
    size_t nblocks = (len + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;

    memset(iv, 0, AES_BLOCK_SIZE);

    while(nblocks-- > 1) {
	xor128(iv, buf);
	aes_encrypt(iv, iv, &state->C1);
	buf += AES_BLOCK_SIZE;
	len -= AES_BLOCK_SIZE;
    }

    if (len == AES_BLOCK_SIZE) {
	xor128(iv, buf);
	xor128(iv, &state->K2);
    } else {
	memcpy(tmp, buf, len);
	tmp[len++] = 0x80;
	if (len != AES_BLOCK_SIZE)
	    memset(tmp + len, 0, AES_BLOCK_SIZE - len);

	xor128(iv, tmp);
	xor128(iv, &state->K3);
    }
    aes_encrypt(iv, iv, &state->C1);
    memcpy(icv, iv, ICV_LEN);
}

struct secure_auth secure_auth_aes_xcbc_mac_96 = {
    .id = SECURE_AUTH_AES_XCBC_96,
    .name = "AUTH-AES-XCBC-MAC-96",
    .init = init,
    .release = release,
    .func = auth,
    .keysize = AES_BLOCK_SIZE,
    .icv_len = ICV_LEN,
};

