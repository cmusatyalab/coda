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

/*
    tokentool.c -- generate Coda-tokens on the fly
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "coda_string.h"
#include <rpc2/rpc2.h>
#include "auth2.h"
#include "tokenfile.h"

static int read_int(char *question)
{
    char input_str[80+1];

    while (1) {
        if (feof(stdin)) exit(-1);

	fprintf(stdout, question);
	fflush(stdout);

	fgets(input_str, 80, stdin);
	fflush(stdin);
	if (input_str[0] >= '0' && input_str[0] <= '9')
	    break;

	fprintf(stdout, "*** Not a number?");
	continue;
    }
    return atoi(input_str);
}

static float read_float(char *question)
{
    char input_str[80+1];

    while (1) {
        if (feof(stdin)) exit(-1);

	fprintf(stdout, question);
	fflush(stdout);

	fgets(input_str, 80, stdin);
	fflush(stdin);
	if (input_str[0] >= '0' && input_str[0] <= '9')
	    break;

	fprintf(stdout, "*** Not a number?");
	continue;
    }
    return atof(input_str);
}

char *read_string(char *question)
{
    char *resp = (char *)malloc(80+1);

    memset(resp, 0, 80);
    fprintf(stdout, question);
    fflush(stdout);
    fgets(resp, 80, stdin);
    fflush(stdin);

    return resp;
}

int main(int argc, char **argv)
{
    int   viceid;
    float duration;
    char *tokenkey;
    char *filename;
    int   i, len;
    RPC2_EncryptionKey   authkey;
    ClearToken 		 ctoken;
    SecretToken		 stoken;
    EncryptedSecretToken estoken;

    rpc2_InitRandom();

    /* query user for important information */
    viceid   = read_int   ("ViceID                   ? ");
    duration = read_float ("Token validity (hours)   ? ");
    tokenkey = read_string("Shared secret (auth2.tk) ? ");
    filename = read_string("Output token file name   ? ");

    if (filename[0] == '\0') {
	free(tokenkey);
	free(filename);
	exit(-1);
    }

    /* truncate the shared secret */
    memset(authkey, 0, sizeof(RPC2_KEYSIZE));
    strncpy(authkey, tokenkey, RPC2_KEYSIZE);

    /* strip newline from filename */
    len = strlen(filename);
    if (filename[len-1] == '\n')
	filename[len-1] = '\0';

    /* construct the clear token */
    ctoken.ViceId       = viceid;
    ctoken.EndTimestamp = time(0) + (int)(60.0 * 60.0 * duration);

    ctoken.AuthHandle     = -1;
    ctoken.BeginTimestamp = 0;
    for (i = 0; i < RPC2_KEYSIZE; i++)
	ctoken.HandShakeKey[i] = rpc2_NextRandom(NULL) & 0xff;

    /* then build secret token */
    stoken.AuthHandle     = htonl(ctoken.AuthHandle);
    stoken.ViceId         = htonl(ctoken.ViceId);
    stoken.BeginTimestamp = htonl(ctoken.BeginTimestamp);
    stoken.EndTimestamp   = htonl(ctoken.EndTimestamp);
    memcpy(stoken.HandShakeKey, ctoken.HandShakeKey, RPC2_KEYSIZE);

    memset(stoken.MagicString, '\0', sizeof(AuthMagic));
    strncpy((char *)stoken.MagicString, (char *)AUTH_MAGICVALUE,
	    sizeof(AuthMagic));
    stoken.Noise1         = rpc2_NextRandom(NULL);
    stoken.Noise2         = rpc2_NextRandom(NULL);
    stoken.Noise3         = rpc2_NextRandom(NULL);
    stoken.Noise4         = rpc2_NextRandom(NULL);

    /* encrypt the secret token */
    rpc2_Encrypt((char *)&stoken, (char *)estoken, sizeof(SecretToken),
		 (char *)authkey, RPC2_XOR);

    /* write the token to a tokenfile */
    WriteTokenToFile(filename, &ctoken, estoken);

    free(tokenkey);
    free(filename);
    return 0;
}
