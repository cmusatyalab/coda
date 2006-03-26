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

/* helpers and private functions */

#ifndef _GRUNT_H_
#define _GRUNT_H_

#include <stdint.h>
#include "aes.h"

#define bytes(bits)	((bits)/8)
#define int32(x)	((uint32_t *)(x))
#define xor128(out, in)	do { \
	int32(out)[0] ^= int32(in)[0]; \
	int32(out)[1] ^= int32(in)[1]; \
	int32(out)[2] ^= int32(in)[2]; \
	int32(out)[3] ^= int32(in)[3]; \
    } while(0)


/* private functions */
/* secure_aes.c */
void secure_aes_init(int verbose);
int aes_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t len,
		    const uint8_t *iv, aes_encrypt_ctx *ctx);
int aes_cbc_decrypt(const uint8_t *in, uint8_t *out, size_t len,
		    const uint8_t *iv, aes_decrypt_ctx *ctx);

/* secure_random.c */
void secure_random_init(int verbose);
void secure_random_release(void);

/* secure_init.c */
const struct secure_auth *secure_get_auth_byid(int id);
const struct secure_encr *secure_get_encr_byid(int id);

#endif /* _GRUNT_H_ */
