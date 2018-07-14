/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <stdlib.h>
#include "base64.h"

/* base 64 encoding/decoding to store the tokens in a convenient fileformat */
static char *b2e =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void coda_base64_encode(FILE *out, char *in, int len)
{
    int i;

    for (i = 0; i < len; i += 3) {
        if ((i % 54) == 0) fputc('\n', out);

        fputc(b2e[(in[i] >> 2) & 0x3f], out);

        if (i+1 < len)
            fputc(b2e[((in[i] << 4) & 0x30) | ((in[i+1] >> 4) & 0xf)], out);
        else {
            fputc(b2e[((in[i] << 4) & 0x30)], out);
            fputc('=', out);
            fputc('=', out);
            break;
        }

        if (i+2 < len) {
            fputc(b2e[((in[i+1] << 2) & 0x3c) | ((in[i+2] >> 6) & 0x3)], out);
            fputc(b2e[in[i+2] & 0x3f], out);
        } else {
            fputc(b2e[((in[i+1] << 2) & 0x3c)], out);
            fputc('=', out);
        }
    }
    fputc('\n', out);
}

void coda_base64_decode(FILE *in, char **out, int *len)
{
    int val = 0, s = 18, n = 0, c, done = 0;
        
    *len = 24; *out = malloc(*len);

    while((c = fgetc(in)) != EOF) {
        if (c == '\n' || c == '\r') continue;
        if (c != '=') {
            if      (c >= 'A' && c <= 'Z') c =  c - 'A';
            else if (c >= 'a' && c <= 'z') c = (c - 'a') + 26;
            else if (c >= '0' && c <= '9') c = (c - '0') + 52;
            else if (c == '+')             c = 62;
            else if (c == '/')             c = 63;
	    else continue;
            val = val | (c << s);
        } else
            done = (s / 6) + 1;

        if ((s -= 6) < 0 || done) {
            (*out)[n]   = (val >> 16) & 0xff;
            (*out)[n+1] = (val >> 8) & 0xff;
            (*out)[n+2] = val & 0xff;
            val = 0; s = 18;
            n += 3 - done;
            if (done) break;

            if (n == *len) {
                *len += 24; *out = realloc(*out, *len);
            }
        }
    }
    *len = n;
}

#ifdef TESTING
#include <sys/stat.h>
#include <stdio.h>

#define TESTFILE "/tmp/base64_test"

void main(int argc, char **argv)
{
    FILE *fp;
    struct stat sb;
    char *outbuf = "testing the encoding", *inbuf;
    int   outlen, inlen;

    if (argc > 1)
        outbuf = argv[1];
    outlen = strlen(outbuf);

    if (stat(TESTFILE, &sb) == 0) {
        unlink(TESTFILE);
    }

    fp = fopen(TESTFILE, "w");
    coda_base64_encode(fp, outbuf, outlen);
    fclose(fp);
    
    fp = fopen(TESTFILE, "r");
    coda_base64_decode(fp, &inbuf, &inlen);
    fclose(fp);

    if (outlen != inlen || memcmp(outbuf, inbuf, outlen) != 0)
        puts("Error encoding/decoding: ");

    puts(inbuf);
    putchar('\n');

    free(inbuf);
}
#endif

