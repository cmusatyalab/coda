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

#include <rpc2/secure.h>
#include "aes.h"
#include "grunt.h"

/* key derivation function, given a relatively short user key, we expand it to
 * the requested amount of keying material.
 * Only use to get a connection setup key. Once the connection has been
 * established, use a session key generated from the PRNG */
int secure_setup_key(const uint8_t rpc2key[8], uint32_t unique,
		     uint8_t *key, size_t keylen)
{
    aes_encrypt_ctx ctx;
    uint8_t block[AES_BLOCK_SIZE];
    uint32_t ctr = 0;
    int blocks = keylen / AES_BLOCK_SIZE;

    int32(block)[0] = htonl(ctr++);
    memcpy(block+4, rpc2key, 8);
    int32(block)[3] = htonl(unique);
    aes_encrypt_key(block, sizeof(block) * 8, &ctx);

    while (blocks--) {
	int32(block)[0] = htonl(ctr++);
	aes_encrypt(block, key, &ctx);
	key += AES_BLOCK_SIZE;
	keylen -= AES_BLOCK_SIZE;
    }
    if (keylen) {
	int32(block)[0] = htonl(ctr++);
	aes_encrypt(block, block, &ctx);
	memcpy(key, block, keylen);
    }
    memset(&ctx, 0, sizeof(aes_encrypt_ctx));
    memset(block, 0, sizeof(block));
}

int secure_setup_encrypt(struct security_association *sa,
			 const struct secure_auth *authenticate,
			 const struct secure_encr *encrypt,
			 const uint8_t *key, size_t len)
{
    int rc, min_keysize = encrypt ? encrypt->min_keysize : 0;

    /* clear any existing decryption/validation state */
    if (sa->authenticate) {
	sa->authenticate->auth_free(&sa->authenticate_context);
	sa->authenticate = NULL;
    }

    if (sa->encrypt) {
	sa->encrypt->encrypt_free(&sa->encrypt_context);
	sa->encrypt = NULL;
    }

    /* intialize new state */
    if (authenticate) {
	rc = authenticate->auth_init(&sa->authenticate_context, key, len);
	if (rc) return -1;

	/* if we have enough key material, keep authentication and decryption
	 * keys separate, otherwise we just have to reuse the same key data */
	if (len >= authenticate->keysize + min_keysize)
	{
	    key += authenticate->keysize;
	    len -= authenticate->keysize;
	}
    }

    if (encrypt) {
	rc = encrypt->encrypt_init(&sa->encrypt_context, key, len);
	if (rc) {
	    if (authenticate)
		authenticate->auth_free(&sa->authenticate_context);
	    return -1;
	}
    }

    sa->authenticate = authenticate;
    sa->encrypt = encrypt;
    return 0;
}

int secure_setup_decrypt(struct security_association *sa,
			 const struct secure_auth *validate,
			 const struct secure_encr *decrypt,
			 const uint8_t *key, size_t len)
{
    int rc, min_keysize = decrypt ? decrypt->min_keysize : 0;

    /* clear any existing decryption/validation state */
    if (sa->validate) {
	sa->validate->auth_free(&sa->validate_context);
	sa->validate = NULL;
    }

    if (sa->decrypt) {
	sa->decrypt->decrypt_free(&sa->decrypt_context);
	sa->decrypt = NULL;
    }

    /* intialize new state */
    if (validate) {
	rc = validate->auth_init(&sa->validate_context, key, len);
	if (rc) return -1;

	/* if we have enough key material, keep authentication and decryption
	 * keys separate, otherwise we just have to reuse the same key data */
	if (len >= validate->keysize + min_keysize)
	{
	    key += validate->keysize;
	    len -= validate->keysize;
	}
    }

    if (decrypt) {
	rc = decrypt->decrypt_init(&sa->decrypt_context, key, len);
	if (rc) {
	    if (validate)
		validate->auth_free(&sa->validate_context);
	    return -1;
	}
    }

    sa->validate = validate;
    sa->decrypt = decrypt;
    secure_random_bytes(&sa->send_iv, sizeof(sa->send_iv));
    return 0;
}

