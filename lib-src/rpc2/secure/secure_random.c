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

#include <sys/types.h>
#include <sys/times.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include <rpc2/secure.h>
#include "aes.h"
#include "grunt.h"

/*
 * Strong pseudo random number generator using AES as a mixing function.
 *
 * Based on,
 * - Digital Signatures Using Revisible Public Key Cryptography for the
 *   Financial Services Industry (rDSA), ANSI X9.31-1988, September 1998.
 * - NIST-Recommended Random Number Generator Based on ANSI X9.31 Appendix
 *   A.2.4 Using the 3-Key Triple DES and AES Algorithms, 31 January 2005.
 */

#define RND_KEY_BITS 128 /* or 192, 256... */
#define INITIAL_SEED_LENGTH (AES_BLOCK_SIZE + bytes(RND_KEY_BITS))

static aes_encrypt_ctx context;
static uint8_t pool[AES_BLOCK_SIZE];
static uint8_t last[AES_BLOCK_SIZE];
static uint32_t counter;

static void prng_get_bytes(uint8_t *random, size_t len);

static void prng_init(const uint8_t s[INITIAL_SEED_LENGTH])
{
    uint8_t tmp[AES_BLOCK_SIZE];

    memcpy(pool, s, AES_BLOCK_SIZE);
    aes_encrypt_key(s + AES_BLOCK_SIZE, RND_KEY_BITS, &context);

    /* discard the first block of random data */
    prng_get_bytes(tmp, AES_BLOCK_SIZE);
}

static void prng_free(void)
{
    memset(&context, 0, sizeof(aes_encrypt_ctx));
    memset(pool, 0, AES_BLOCK_SIZE);
    memset(last, 0, AES_BLOCK_SIZE);
    counter = 0;
}

static void prng_get_bytes(uint8_t *random, size_t len)
{
    uint8_t tmp[AES_BLOCK_SIZE], *I, *prev = last;
    struct {
	struct timeval tv;
	uint32_t uninitialized_stack_value;
	uint32_t counter;
    } init;
    int nblocks = (len + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;

    /* Mix some entropy into the pool */
    gettimeofday(&init.tv, NULL);
    init.counter = counter++;
    I = (uint8_t *)&init;
    aes_encrypt(I, I, &context);

    while (nblocks--) {
	xor128(pool, I);

	if (!nblocks && len != AES_BLOCK_SIZE) {
	    aes_encrypt(pool, tmp, &context);
	    memcpy(random, tmp, len);
	    random = tmp;
	} else
	    aes_encrypt(pool, random, &context);

	/* reseed the pool, mix in the random value */
	xor128(I, random);
	aes_encrypt(I, pool, &context);

	/* we must never return consecutive identical blocks */
	assert(memcmp(prev, random, AES_BLOCK_SIZE) != 0);

	prev = random;
	random += AES_BLOCK_SIZE;
	len -= AES_BLOCK_SIZE;
    }
    if (prev != last)
	memcpy(last, prev, AES_BLOCK_SIZE);
}

/* we need to find between 32 and 48 bytes of entropy to seed our PRNG
 * depending on the value of RNG_KEY_BITS */
static void get_initial_seed(uint8_t *ptr, size_t len)
{
    int fd;

    /* about 8 bytes from the current time */
    if (len >= sizeof(struct timeval)) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	memcpy(ptr, &tv, sizeof(struct timeval));
	ptr += sizeof(struct timeval);
	len -= sizeof(struct timeval);
    }

    /* try to get the rest from /dev/random */
    fd = open("/dev/random", O_RDONLY);
    if (fd != -1) {
	ssize_t n = read(fd, ptr, len);
	if (n > 0) {
	    ptr += n;
	    len -= n;
	}
	close(fd);
	/* we should be done now, but fall through just in case... */
    }

    /* we can get about 20 bytes from times(). I assume these would be rather
     * non-random since we probably just started, on the other hand, the
     * returned ticks value should be clock ticks since system boot, which
     * might be more random depending on whether we just rebooted or not. */
    if (len >= sizeof(clock_t) + sizeof(struct tms)) {
	struct tms tms;
	clock_t ticks = times(&tms);
	memcpy(ptr, &ticks, sizeof(clock_t));
	ptr += sizeof(clock_t);
	memcpy(ptr, &tms, sizeof(struct tms));
	ptr += sizeof(struct tms);
	len -= sizeof(struct tms) + sizeof(clock_t);
    }

    /* mix in the process id, probably not so random right after boot either */
    if (len >= sizeof(pid_t)) {
	pid_t pid = getpid();
	memcpy(ptr, &pid, sizeof(pid_t));
	ptr += sizeof(pid_t);
	len -= sizeof(pid_t);
    }

    /* we _really_ should be done by now, but just in case someone changed
     * RND_KEY_BITS.. Supposedly the top-8 bits in the random() result are
     * 'more random', which is why we use (random()*255)/RAND_MAX */
    if (len) {
	srandom(time(NULL));
	while (len--)
	    *(ptr++) = (uint8_t)(((double)random() * 255) / (double)RAND_MAX);
    }

    /* /dev/random is probably the most randomized source, but just in case
     * it is malfunctioning still get the gettimeofday value first. If
     * /dev/random doesn't exist we fall back on several more predictable
     * sources. The first 16 bytes are used to seed the pool, the remaining
     * RND_KEY_BITS initialize the AES-key for the mixing function. */

    /* other possible sources? getrusage(), system memory usage? checksum of
     * /proc/{interrupts,meminfo,slabinfo,vmstat}?
     * Windows might want to use CryptGenRandom() and GlobalMemoryStatus()
     */
}

/* here are the exported functions */
void secure_random_init(void)
{
    uint8_t initial_seed[INITIAL_SEED_LENGTH];

    if (counter != 0) return; /* we're already initialized */

    get_initial_seed(initial_seed, INITIAL_SEED_LENGTH);

    /* initialize the RNG */
    prng_init(initial_seed);
}

void secure_random_release(void)
{
    prng_free();
}

void secure_random_bytes(uint8_t *random, size_t len)
{
    prng_get_bytes(random, len);
}

