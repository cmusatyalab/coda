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
#define KEYSIZE AES_BLOCK_SIZE

struct aes_xcbc_state {
    aes_encrypt_ctx C1;
    aes_block K2;
    aes_block K3;
};

int aes_xcbc_mac_init(void **ctx, const uint8_t *key, size_t len)
{
    struct aes_xcbc_state *state;
    aes_block tmp;
    aes_encrypt_ctx c;

    if (len < KEYSIZE) return -1;

    state = malloc(sizeof(struct aes_xcbc_state));
    if (!state) return -1;

    aes_encrypt_key(key, KEYSIZE * 8, &c);

    memset(tmp, 0x01, sizeof(aes_block));
    aes_encrypt(tmp, tmp, &c);
    aes_encrypt_key((uint8_t *)tmp, sizeof(aes_block) * 8, &state->C1);

    memset(tmp, 0x02, sizeof(aes_block));
    aes_encrypt(tmp, state->K2, &c);

    memset(tmp, 0x03, sizeof(aes_block));
    aes_encrypt(tmp, state->K3, &c);

    memset(tmp, 0, sizeof(aes_block));
    memset(&c, 0, sizeof(aes_encrypt_ctx));

    *ctx = state;
    return 0;
}

void aes_xcbc_mac_release(void **ctx)
{
    struct aes_xcbc_state *state = *ctx;
    if (!*ctx) return;
    memset(&state->C1, 0, sizeof(aes_encrypt_ctx));
    memset(state->K2, 0, sizeof(aes_block));
    memset(state->K3, 0, sizeof(aes_block));
    free(state);
    *ctx = NULL;
}

void aes_xcbc_mac_128(void *ctx, const uint8_t *buf, size_t len, aes_block mac)
{
    struct aes_xcbc_state *state = ctx;
    size_t nblocks = (len + sizeof(aes_block) - 1) / sizeof(aes_block);
    aes_block tmp;

    memset(mac, 0, sizeof(aes_block));

    while(nblocks-- > 1) {
	xor128(mac, (uint64_t *)buf);
	aes_encrypt(mac, mac, &state->C1);
	buf += sizeof(aes_block);
	len -= sizeof(aes_block);
    }

    if (len == sizeof(aes_block)) {
	xor128(mac, (uint64_t *)buf);
	xor128(mac, state->K2);
    } else {
	memcpy(tmp, buf, len);
	((uint8_t *)tmp)[len++] = 0x80;
	if (len != sizeof(aes_block))
	    memset((uint8_t *)tmp + len, 0, sizeof(aes_block) - len);

	xor128(mac, tmp);
	xor128(mac, state->K3);
    }
    aes_encrypt(mac, mac, &state->C1);
}

static void aes_xcbc_mac_96(void *ctx, const uint8_t *buf, size_t len,
			    uint8_t *icv)
{
    aes_block mac;
    aes_xcbc_mac_128(ctx, buf, len, mac);
    memcpy(icv, mac, ICV_LEN);
}

struct secure_auth secure_AUTH_AES_XCBC_MAC_96 = {
    .id = SECURE_AUTH_AES_XCBC_96,
    .name = "AUTH-AES-XCBC-MAC-96",
    .auth_init = aes_xcbc_mac_init,
    .auth_free = aes_xcbc_mac_release,
    .auth = aes_xcbc_mac_96,
    .keysize = KEYSIZE,
    .icv_len = ICV_LEN,
};

