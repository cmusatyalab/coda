/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _CODA_HASH_H_
#define _CODA_HASH_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>

#if defined(HAVE_OPENSSL_MD5_H)
#include <openssl/md5.h>
#elif defined(HAVE_MD5_H)
#include <md5.h>
#else

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

typedef struct MD5Context {
    u_int32_t state[4];   /* state (ABCD) */
    u_int32_t count[2];   /* number of bits, modulo 2^64 (lsb first) */
    unsigned char buffer[64];     /* input buffer */
} MD5_CTX;

void MD5_Init(MD5_CTX *);
void MD5_Update(MD5_CTX *, const unsigned char *, unsigned int);
void MD5_Final(unsigned char [16], MD5_CTX *);

#endif

#if defined(HAVE_OPENSSL_SHA_H)
#include <openssl/sha.h>
#else

#define SHA_DIGEST_LENGTH 20

typedef struct SHAContext {
    u_int32_t count;
    u_int32_t state[5];
    unsigned char buffer[64];
} SHA_CTX;

void SHA1_Init(SHA_CTX *ctx);
void SHA1_Update(SHA_CTX *ctx, const unsigned char *buf, unsigned int len);
void SHA1_Final(unsigned char sha[SHA_DIGEST_LENGTH], SHA_CTX *ctx);

#endif

#ifdef __cplusplus
}
#endif

#endif  /* _CODA_HASH_H_ */

