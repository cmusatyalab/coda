/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#include <sys/types.h>
#include <fcntl.h>
#include <util.h>
#include <stdio.h>
#include <coda_md5.h>
#include <coda_string.h>
#include "updatecommon.h"

int GetSecret(char *tokenfile, RPC2_EncryptionKey key)
{
    int fd, n;
    unsigned char buf[512], digest[16];
    MD5_CTX md5ctxt;

    memset(key, 0, RPC2_KEYSIZE);

    fd = open(tokenfile, O_RDONLY);
    if (fd < 0) {
	LogMsg(0, SrvDebugLevel, stdout, "Could not open %s", tokenfile);
	return -1;
    }

    memset(buf, 0, 512);
    n = read(fd, buf, 512);
    if (n < 0) {
	LogMsg(0, SrvDebugLevel, stdout, "Could not read %s", tokenfile);
	close(fd);
	return -1;
    }
    close(fd);

    MD5_Init(&md5ctxt);
    MD5_Update(&md5ctxt, buf, n);
    MD5_Final(digest, &md5ctxt);

    memcpy(key, digest, RPC2_KEYSIZE);

    return 0;
}

