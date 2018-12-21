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

/*
 * RFC 2898: PKCS #5: Password-Based Cryptography Specification Version 2.0
 *
 * Using AES-XCBC-PRF-128 as the pseudo random function.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <rpc2/secure.h>

#include "aes.h"
#include "grunt.h"

static void F(void *ctx, uint8_t *U0, size_t U0len, uint32_t i,
              size_t iterations, aes_block *prv)
{
    aes_block UN;
    size_t n;

    i = htonl(i);
    memcpy(U0 + U0len - sizeof(uint32_t), &i, sizeof(uint32_t));

    aes_xcbc_prf_128(ctx, U0, U0len, &UN);
    memcpy(prv->u8, UN.u8, AES_BLOCK_SIZE);

    for (n = 1; n < iterations; n++) {
        aes_xcbc_prf_128(ctx, UN.u8, sizeof(aes_block), &UN);
        xor128(prv, &UN);
    }
}

/* Password Based Key Derivation Function (PKCS #5 version 2) */
int secure_pbkdf(const uint8_t *password, size_t plen, const uint8_t *salt,
                 size_t slen, size_t iterations, uint8_t *key, size_t keylen)
{
    size_t U0len, nblocks = keylen / AES_BLOCK_SIZE;
    uint8_t *U0;
    uint32_t i = 1;
    void *ctx;

    U0len = slen + sizeof(uint32_t);
    U0    = malloc(U0len);
    if (!U0)
        return -1;

    if (aes_xcbc_prf_init(&ctx, password, plen)) {
        free(U0);
        return -1;
    }

    /* recommended minimum number of iterations is 1000 */
    if (iterations < 1000)
        iterations = 1000;

    memset(U0, 0, U0len);
    if (salt && slen)
        memcpy(U0, salt, slen);

    for (i = 1; i <= nblocks; i++) {
        F(ctx, U0, U0len, i, iterations, (aes_block *)key);
        key += AES_BLOCK_SIZE;
        keylen -= AES_BLOCK_SIZE;
    }
    if (keylen) {
        aes_block tmp;
        F(ctx, U0, U0len, i, iterations, &tmp);
        memcpy(key, tmp.u8, keylen);
        memset(tmp.u8, 0, sizeof(aes_block));
    }
    aes_xcbc_prf_release(&ctx);
    memset(U0, 0, U0len);
    free(U0);
    return 0;
}

void secure_pbkdf_init(int verbose)
{
    struct timeval begin, end;
    uint8_t password[8], salt[8], key[48];
    int operations = 0, runlength = verbose ? 1000000 : 100000;

    if (verbose)
        fprintf(stderr, "Password Based Key Derivation:  ");

    memset(key, 0, sizeof(key));
    memset(salt, 0, sizeof(salt));
    memset(password, 0, sizeof(password));

    gettimeofday(&begin, NULL);
    do {
        operations++;
        secure_pbkdf(password, sizeof(password), salt, sizeof(salt),
                     SECURE_PBKDF_ITERATIONS, key, sizeof(key));
        gettimeofday(&end, NULL);

        end.tv_sec -= begin.tv_sec;
        end.tv_usec += 1000000 * end.tv_sec;
        end.tv_usec -= begin.tv_usec;
        /* see how many iterations we can run in ~0.1 seconds. */
    } while (end.tv_usec < runlength);

    operations *= 1000000 / runlength;

    /* How can we possibly do a pass/fail test on this?
     *
     * Clearly the security is based on the assumption that there are no
     * shortcuts in the algorithm, and that someone has to revert to brute
     * force password guessing.
     *
     * Now if we assume that it is probable that someone has a 10x faster
     * implementation he can do close to 1313 operations per second on a
     * 3GHz P4 (my machine seems to do 131.3 ops/s) and has ~1000 machines
     * available and as such divides the keyspace by 2^10.
     *
     * If the password only consists of lowercase alpha characters, we have
     * 40-bits from an 8 character secret. And a full search can be done in
     * less than 10 days. But then again, lowercase only passwords have been
     * considered weak for several years now.
     *
     * With a random alpha-numeric (mixed case) 8 character password, we have
     * almost 48 bits and under the same assumptions it would take the attacker
     * close to 2500 days (a more than 6 years).
     *
     * A truly random 8 byte secret (i.e. old Coda tokens) increases the
     * time it takes to brute force the secret significantly (435,000 years).
     *
     * Of course computers are getting faster and cheaper. Maybe a hardware
     * based implementation is already 100x faster, and someone has enough
     * money to put a million of those in parallel.
     *
     * The best approach is probably to try to make our implementation as
     * optimized as possible (so someone cannot be a 10-100x faster), and keep
     * the cost high enough by increasing SECURE_PBKDF_ITERATIONS once in a
     * while.
     *
     * With 10000 iterations,
     *	600MHz PIII,		20 ops/s
     *	3.2GHz P4,		133 ops/s
     *	2.13GHz Core 2 6400,	233 ops/s
     */

    if (operations > 1000) /* i.e. > 1000 ops/s */
        fprintf(stderr, "WARNING: Password Based Key Derivation ");

    if (verbose || operations > 1000)
        fprintf(stderr, "%d ops/s\n", operations);
}
