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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "coda_string.h"
#include <base64.h>
#include "tokenfile.h"

static void export(ClearToken *cToken)
{
    cToken->AuthHandle     = htonl(cToken->AuthHandle);
    cToken->ViceId         = htonl(cToken->ViceId);
    cToken->BeginTimestamp = htonl(cToken->BeginTimestamp);
    cToken->EndTimestamp   = htonl(cToken->EndTimestamp);
}

static void import(ClearToken *cToken)
{
    cToken->AuthHandle     = ntohl(cToken->AuthHandle);
    cToken->ViceId         = ntohl(cToken->ViceId);
    cToken->BeginTimestamp = ntohl(cToken->BeginTimestamp);
    cToken->EndTimestamp   = ntohl(cToken->EndTimestamp);
}

void WriteTokenToFile(char *filename, ClearToken *cToken,
                      EncryptedSecretToken sToken)
{
    FILE *f;
    char *buf;
    int len;

    len = sizeof(ClearToken) + sizeof(EncryptedSecretToken);
    buf = malloc(len);
    export(cToken);
    memcpy(buf, (char *)cToken, sizeof(ClearToken));
    memcpy(buf + sizeof(ClearToken), sToken, sizeof(EncryptedSecretToken));
    import(cToken);

    umask(0177);
    f = fopen(filename, "w");
    fputs("*** Coda Token ***", f);
    coda_base64_encode(f, buf, len);
    fclose(f);
    free(buf);
}

void ReadTokenFromFile(char *filename, ClearToken *cToken,
                       EncryptedSecretToken sToken)
{
    FILE *f;
    char *buf = NULL;
    signed char c;
    int len;

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s.\n", filename);
	exit(EXIT_FAILURE);
    }

    /* skip the first line */
    while((c = fgetc(f)) != EOF && c != '\n' && c != '\r') /* skip */;
    coda_base64_decode(f, &buf, &len);
    fclose(f);

    if (len != sizeof(ClearToken) + sizeof(EncryptedSecretToken)) {
        fprintf(stderr, "Corrupted token file?\n");
        free(buf);
        exit(EXIT_FAILURE);
    }
    memcpy((char *)cToken, buf, sizeof(ClearToken));
    memcpy(sToken, buf + sizeof(ClearToken), sizeof(EncryptedSecretToken));
    import(cToken);
    free(buf);
}

