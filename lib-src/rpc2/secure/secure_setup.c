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
#include <string.h>

#include <rpc2/secure.h>
#include "aes.h"
#include "grunt.h"

int secure_setup_encrypt(uint32_t secure_version,
			 struct security_association *sa,
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
	rc = authenticate->auth_init(secure_version, &sa->authenticate_context,
				     key, len);
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
	rc = encrypt->encrypt_init(secure_version, &sa->encrypt_context,
				   key, len);
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

int secure_setup_decrypt(uint32_t secure_version,
			 struct security_association *sa,
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
	rc = validate->auth_init(secure_version, &sa->validate_context,
				 key, len);
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
	rc = decrypt->decrypt_init(secure_version, &sa->decrypt_context,
				   key, len);
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

