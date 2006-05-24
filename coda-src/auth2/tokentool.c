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
#include "codatoken.h"

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
    int   len;
    ClearToken 		 ctoken;
    EncryptedSecretToken estoken;
    RPC2_EncryptionKey   token;
    uint8_t auth2key[AUTH2KEYSIZE];

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
    memset(token, 0, sizeof(RPC2_KEYSIZE));
    strncpy((char *)token, tokenkey, RPC2_KEYSIZE);

    getauth2key(token, RPC2_KEYSIZE, auth2key);

    /* strip newline from filename */
    len = strlen(filename);
    if (filename[len-1] == '\n')
	filename[len-1] = '\0';

    /* construct the token */
    generate_CodaToken(auth2key, viceid, (int)(3600.0 * duration),
		       &ctoken, estoken);

    /* write the token to a tokenfile */
    WriteTokenToFile(filename, &ctoken, estoken);

    free(tokenkey);
    free(filename);
    return 0;
}
