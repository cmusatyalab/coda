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
    /* Initialize and run the AES test vectors */
    secure_aes_init(verbose);

    /* Initialize and test the PRNG */
    secure_random_init(verbose);
}

void secure_release(void)
{
    secure_random_release();
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

