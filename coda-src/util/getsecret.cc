/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __cplusplus
}
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <util.h>
#include <unistd.h>
#include <stdio.h>
#include <coda_hash.h>
#include <coda_string.h>
#include "getsecret.h"

void HashSecret(unsigned char *secret, int len, RPC2_EncryptionKey key)
{
    MD5_CTX md5ctxt;
    unsigned char digest[16];

    MD5_Init(&md5ctxt);
    MD5_Update(&md5ctxt, secret, len);
    MD5_Final(digest, &md5ctxt);

    memcpy(key, digest, RPC2_KEYSIZE);
}

int GetSecret(char *file, RPC2_EncryptionKey key, struct secret_state *state)
{
    struct stat statbuf;
    time_t now;
    int fd, n;
    unsigned char buf[512];

    /* check if the cached key we have is still valid */
    n = stat(file, &statbuf);
    if (n < 0)
	return -1;

    if (statbuf.st_mtime != state->mtime)
    {
	now = time(NULL);
	fd = open(file, O_RDONLY);
	if (fd < 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "Could not open %s", file);
	    return -1;
	}

	/* better be safe than sorry, make sure the buffer is padded with 0 */
	memset(buf, 0, 512);
	n = read(fd, buf, 512);
	if (n < 0) {
	    LogMsg(0, SrvDebugLevel, stdout, "Could not read %s", file);
	    close(fd);
	    return -1;
	}
	close(fd);

	HashSecret(buf, n, state->key);

	/* update mtime after when we read the cached key, but not if the
	 * change very recent otherwise we could miss a following update
	 * if it occurs in the same second as well. */
	if (statbuf.st_mtime != now)
	    state->mtime = statbuf.st_mtime;
    }
    memcpy(key, state->key, RPC2_KEYSIZE);

    return 0;
}

void GenerateSecret(RPC2_EncryptionKey key)
{
    int i;
    for (i = 0; i < RPC2_KEYSIZE; i++)
        key[i] = rpc2_NextRandom(NULL) & 0xff;
}

