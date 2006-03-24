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

#include <string.h>
#include <rpc2/secure.h>

static int init(void **ctx, const uint8_t *key)
{
    return 1;
}

static void release(void **ctx)
{
    return;
}

static int crypt(void *ctx, const uint8_t *in, uint8_t *out, size_t len,
		 const uint8_t *iv)
{
    if (out != in)
	memcpy(out, in, len);
    return len;
}

struct secure_crypt secure_encr_NULL = {
    .id	     = SECURE_ENCR_NULL,
    .name    = "ENCR-NULL",
    .init    = init,
    .func    = crypt,
    .release = release,
};

