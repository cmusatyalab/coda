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

#include <stdlib.h>
#include "base64.h"

/* base 64 encoding/decoding to store the tokens in a convenient fileformat */
static char *b2e =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode(FILE *out, char *in, int len)
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

void base64_decode(FILE *in, char **out, int *len)
{
    int val = 0, s = 18, n = 0, c, done = 0;
        
    *len = 24; *out = malloc(*len);

    while((c = fgetc(in)) != EOF) {
        if (c != '=') {
            if      (c >= 'A' && c <= 'Z') c =  c - 'A';
            else if (c >= 'a' && c <= 'z') c = (c - 'a') + 26;
            else if (c >= '0' && c <= '9') c = (c - '0') + 52;
            else if (c == '+')             c = 62;
            else if (c == '/')             c = 63;
            val = val | (c << s);
        } else
            done = 1;

        if ((s -= 6) < 0) {
            (*out)[n]   = (val >> 16) & 0xff;
            (*out)[n+1] = (val >> 8) & 0xff;
            (*out)[n+2] = val & 0xff;
            val = 0; s = 18;
            if (done) break;

            if ((n += 3) == *len) {
                *len += 24; *out = realloc(*out, *len);
            }
        }
    }
    *len = n;
}
