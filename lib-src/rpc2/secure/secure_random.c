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

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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

/* You can choose to use the '/dev/random' device which blocks until
 * enough entropy is available. This is more secure, and typically not
 * an issue for long running processes like venus and codasrv. We only
 * read between 32 and 48 bytes when the application is starting.
 *
 * However it can be a problem for servers that frequenly run
 * applications like volutil or clog,
 * - A machine monitoring a large number of Coda server with rpc2ping
 *   and/or volutil interpreted such stalls as failures.
 * - During Amanda backups we run volutil several times per backed up
 *   volume and easily exceed the time the Amanda server waits for a
 *   reply from the client (Coda server) resulting in failed backups.
 * - When we authenticate users with pam_coda we run clog on every
 *   authentication attempt, as a result an ssh scan drains the entropy
 *   pool. This was very noticeable when using pam-based authentication
 *   with Apache.
 */
/* #define RANDOM_DEVICE "/dev/random" */
#define RANDOM_DEVICE "/dev/urandom"

static aes_encrypt_ctx context;
static aes_block pool;
static aes_block last;
static uint32_t counter;

static void prng_get_bytes(uint8_t *random, size_t len);

static void prng_init(const uint8_t s[INITIAL_SEED_LENGTH])
{
    aes_block tmp;

    memcpy(pool.u8, s, sizeof(aes_block));
    aes_encrypt_key(s + AES_BLOCK_SIZE, RND_KEY_BITS, &context);

    /* discard the first block of random data */
    prng_get_bytes(tmp.u8, sizeof(aes_block));
}

static void prng_free(void)
{
    memset(&context, 0, sizeof(aes_encrypt_ctx));
    memset(pool.u8, 0, sizeof(aes_block));
    memset(last.u8, 0, sizeof(aes_block));
    counter = 0;
}

static void prng_get_bytes(uint8_t *random, size_t len)
{
    aes_block I, *prev  = &last;
    aes_block tmp, *out = (aes_block *)random;
    int nblocks = (len + sizeof(aes_block) - 1) / sizeof(aes_block);

    /* Mix some entropy into the pool */
    gettimeofday((struct timeval *)&I.u64[0], NULL);
    I.u32[3] = counter++;
    aes_encrypt(&I, &I, &context);

    len %= sizeof(aes_block);
    while (nblocks--) {
        xor128(&pool, &I);

        if (!nblocks && len) {
            aes_encrypt(&pool, &tmp, &context);
            memcpy(out->u8, tmp.u8, len);
            out = &tmp;
        } else
            aes_encrypt(&pool, out, &context);

        /* reseed the pool, mix in the random value */
        xor128(&I, out);
        aes_encrypt(&I, &pool, &context);

        /* we must never return consecutive identical blocks */
        assert(memcmp(prev->u8, out->u8, sizeof(aes_block)) != 0);

        prev = out++;
    }
    if (prev != &last)
        memcpy(last.u8, prev->u8, sizeof(aes_block));
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
    fd = open(RANDOM_DEVICE, O_RDONLY);
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

/* Statistical random number generator tests defined in
 * FIPS 140-1 - 4.11.1 Power-Up Tests
 *
 *  A single bit stream of 20,000 consecutive bits of output from the
 *  generator is subjected to each of the following tests. If any of the
 *  tests fail, then the module shall enter an error state.
 *
 * The Monobit Test
 *  1. Count the number of ones in the 20,000 bit stream. Denote this
 *     quantity by X.
 *  2. The test is passed if 9,654 < X < 10,346
 *
 * The Poker Test
 *  1. Divide the 20,000 bit stream into 5,000 contiguous 4 bit
 *     segments. Count and store the number of occurrences of each of
 *     the 16 possible 4 bit values. Denote f(i) as the number of each 4
 *     bit value i where 0 < i < 15.
 *  2. Evaluate the following: X = (16/5000) * (Sum[f(i)]^2)-5000
 *  3. The test is passed if 1.03 < X < 57.4
 *
 * The Runs Test
 *  1. A run is defined as a maximal sequence of consecutive bits of
 *     either all ones or all zeros, which is part of the 20,000 bit
 *     sample stream. The incidences of runs (for both consecutive zeros
 *     and consecutive ones) of all lengths ( 1) in the sample stream
 *     should be counted and stored.
 *  2. The test is passed if the number of runs that occur (of lengths 1
 *     through 6) is each within the corresponding interval specified
 *     below. This must hold for both the zeros and ones; that is, all
 *     12 counts must lie in the specified interval. For the purpose of
 *     this test, runs of greater than 6 are considered to be of length 6.
 *       Length of Run			    Required Interval
 *	     1					2,267-2,733
 *	     2					1,079-1,421
 *	     3					502-748
 *	     4					223-402
 *	     5					90-223
 *	     6+					90-223
 *
 * The Long Run Test
 *  1. A long run is defined to be a run of length 34 or more (of either
 *     zeros or ones).
 *  2. On the sample of 20,000 bits, the test is passed if there are NO
 *     long runs.
 */

#define TESTSIZE (20000 / (sizeof(uint32_t) * 8))

static void check_random(int verbose)
{
    uint32_t data[TESTSIZE], val;
    unsigned int i, j, idx;
    int fail, failed = 0;
    int ones, f[16], run, odd, longrun;

    secure_random_bytes(data, sizeof(data));

    /* the tests do not define the 'endianess' of the stream, so
   * I assume little endian */

    /* Monobit Test */
    for (ones = 0, i = 0; i < TESTSIZE; i++) {
        val = data[i];
        while (val) {
            if (val & 1)
                ones++;
            val >>= 1;
        }
    }

    fail = (ones <= 9654 || ones >= 10346);
    failed += fail;
    if (fail || verbose)
        fprintf(stderr, "PRNG monobit test:              %s\n",
                fail ? "FAILED" : "PASSED");

    /* Poker Test */
    memset(f, 0, sizeof(f));
    for (i = 0; i < TESTSIZE; i++) {
        for (j = 0; j < 32; j += 4) {
            idx = (data[i] >> j) & 0xf;
            f[idx]++;
        }
    }
    for (val = 0, i = 0; i < 16; i++)
        val += f[i] * f[i];
    assert((val & 0xf0000000) == 0);
    val <<= 4;

    fail = (val <= 25005150 || val >= 25287000);
    failed += fail;
    if (fail || verbose)
        fprintf(stderr, "PRNG poker test:                %s\n",
                fail ? "FAILED" : "PASSED");

    /* Runs Test */
    memset(f, 0, sizeof(f));
    odd = run = longrun = 0;
    for (i = 0; i < TESTSIZE; i++) {
        val = data[i];
        for (j = 0; j < 32; j++) {
            if (odd ^ (val & 1)) {
                if (run) {
                    if (run > longrun)
                        longrun = run;
                    if (run > 6)
                        run = 6;
                    idx = run - 1 + (odd ? 6 : 0);
                    f[idx]++;
                }
                odd = val & 1;
                run = 0;
            }
            run++;
            val >>= 1;
        }
    }
    if (run > longrun)
        longrun = run;
    if (run > 6)
        run = 6;
    idx = run - 1 + (odd ? 6 : 0);
    f[idx]++;

    fail = (f[0] <= 2267 || f[0] >= 2733 || f[6] <= 2267 || f[6] >= 2733 ||
            f[1] <= 1079 || f[1] >= 1421 || f[7] <= 1079 || f[7] >= 1421 ||
            f[2] <= 502 || f[2] >= 748 || f[8] <= 502 || f[8] >= 748 ||
            f[3] <= 223 || f[3] >= 402 || f[9] <= 223 || f[9] >= 402 ||
            f[4] <= 90 || f[4] >= 223 || f[10] <= 90 || f[10] >= 223 ||
            f[5] <= 90 || f[5] >= 223 || f[11] <= 90 || f[11] >= 223);
    failed += fail;
    if (fail || verbose)
        fprintf(stderr, "PRNG runs test:                 %s\n",
                fail ? "FAILED" : "PASSED");

    /* Long Run Test */
    fail = (longrun >= 34);
    failed += fail;
    if (fail || verbose)
        fprintf(stderr, "PRNG long run test:             %s\n",
                fail ? "FAILED" : "PASSED");

    if (failed)
        abort();
}

/* initialization only called from secure_init */
void secure_random_init(int verbose)
{
    uint8_t initial_seed[INITIAL_SEED_LENGTH];

    if (counter != 0)
        return; /* we're already initialized */

    get_initial_seed(initial_seed, INITIAL_SEED_LENGTH);

    /* initialize the RNG */
    prng_init(initial_seed);

    check_random(verbose);
}

void secure_random_release(void)
{
    prng_free();
}

/* this is really the only exported function */
void secure_random_bytes(void *random, size_t len)
{
    prng_get_bytes((uint8_t *)random, len);
}
