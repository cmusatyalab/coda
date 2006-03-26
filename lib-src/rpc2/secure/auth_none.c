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

static int init(void **ctx, const uint8_t *key)
{
    return 0;
}

static void release(void **ctx)
{
    return;
}

static void auth(void *ctx, const uint8_t *buf, size_t len, uint8_t *icv)
{
    return;
}


struct secure_auth secure_AUTH_NONE = {
    .id = SECURE_AUTH_NONE,
    .name = "AUTH-NONE",
    .auth_init = init,
    .auth_free = release,
    .auth = auth,
};

