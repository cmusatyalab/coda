/* BLURB lgpl
			Coda File System
			    Release 6

	 Copyright (c) 2006-2016 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
#*/

#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include <rpc2/secure.h>
#include "aes.h"
#include "grunt.h"

#ifndef LOG_AUTHPRIV
#define LOG_AUTHPRIV LOG_AUTH
#endif

/* max formatted address length = hex ipv6 addr + decimal port + "[]:\0" */
#define MAXADDR (32 + 5 + 4)

/* Authentication algorithms. */
extern struct secure_auth
    secure_AUTH_NONE,
    secure_AUTH_AES_XCBC_MAC_96;

static const struct secure_auth *alg_auth[] = {
    &secure_AUTH_NONE,
    &secure_AUTH_AES_XCBC_MAC_96,
    NULL
};

/* Encryption algorithms. */
extern struct secure_encr
    secure_ENCR_NULL,
    secure_ENCR_AES_CBC,
    secure_ENCR_AES_CCM_8,
    secure_ENCR_AES_CCM_12,
    secure_ENCR_AES_CCM_16;

static const struct secure_encr *alg_encr[] = {
    &secure_ENCR_NULL,
    &secure_ENCR_AES_CBC,
    &secure_ENCR_AES_CCM_8,
    &secure_ENCR_AES_CCM_12,
    &secure_ENCR_AES_CCM_16,
    NULL
};

void secure_init(int verbose)
{
    /* Should we pass argv[0] down, or leave the openlog up to the caller? */
    openlog("RPC2", LOG_PID, LOG_AUTHPRIV);

    /* Initialize and run the AES test vectors */
    secure_aes_init(verbose);

    /* Initialize and test the PRNG */
    secure_random_init(verbose);

    /* Run the PBKDF timing test */
    secure_pbkdf_init(verbose);
}

void secure_release(void)
{
    secure_random_release();
    closelog();
}

const struct secure_auth *secure_get_auth_byid(int id)
{
    int i = 0;
    while (alg_auth[i] && id != alg_auth[i]->id) i++;
    return alg_auth[i];
}

const struct secure_encr *secure_get_encr_byid(int id)
{
    int i = 0;
    while (alg_encr[i] && alg_encr[i]->id != id) i++;
    return alg_encr[i];
}

/* format_addr is surprisingly similar to RPC2_formataddrinfo, but this way we
 * don't introduce a dependency on RPC2 (and this formats sockaddr) */
static void format_addr(const struct sockaddr *sa, char *buf, size_t blen)
{
    int n, port = 0;
    void *addr = NULL;
    char *p = buf;

    blen--;
    if (!sa) {
	strncpy(buf, "(missing address)", blen);
	buf[blen] = '\0';
	return;
    }

    switch (sa->sa_family) {
    case PF_INET:
	addr = &((struct sockaddr_in *)sa)->sin_addr;
	port = ((struct sockaddr_in *)sa)->sin_port;
	break;

#ifdef PF_INET6
    case PF_INET6:
	addr = &((struct sockaddr_in6 *)sa)->sin6_addr;
	port = ((struct sockaddr_in6 *)sa)->sin6_port;
	*(p++) = '[';
	break;
#endif
    }

    /* format address */
    if (!addr || !inet_ntop(sa->sa_family, addr, p, buf + blen - p)) {
	strncpy(buf, "(untranslatable address)", blen);
	p = buf;
    }
    buf[blen] = '\0';

    /* append port */
    n = strlen(buf);
    if (port)
	snprintf(buf + n, blen - n, "%s:%u", p != buf ? "]" : "", ntohs(port));
    buf[blen] = '\0';
}

/* RFC 2406 -- The audit log entry ... SHOULD include the SPI value, date/time
 * received, Source Address, Destination Address, Sequence Number, and (in
 * IPv6) the Flow ID. */
/* I don't actually have the destination address and ipv6 flow id readily
 * available, so this will have to do. */
void secure_audit(const char *event, uint32_t spi, uint32_t seq,
		  const struct sockaddr *src)
{
    char src_buf[MAXADDR];

    format_addr(src, src_buf, sizeof(src_buf));

    syslog(LOG_AUTHPRIV | LOG_NOTICE, "%s: spi %x, seq %d, src %s\n",
	   event, spi, seq, src_buf);
}

/* Constant time comparison, returns true when both buffers are identical */
int secure_compare(const void *user_data, size_t user_len,
                    const void *secret, size_t secret_len)
{
    volatile const char *cmp, *to = secret;
    size_t i;
    int different;

    /* Make sure we always compare 'secret_len' bytes */
    if (user_len == secret_len) {
        cmp = user_data;
        different = 0;
    }
    if (user_len != secret_len) {
        cmp = to;
        different = 1;
    }

    for (i = 0; i < secret_len; i++) {
        different |= cmp[i] ^ to[i];
    }

    return (different == 0);
}
