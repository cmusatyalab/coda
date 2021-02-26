/* BLURB gpl

                           Coda File System
                              Release 8

            Copyright (c) 2021 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static int is_ascii_printable(unsigned char c)
{
    return (c == '\t' || c == '\n' || c == '\r' || (c >= 0x20 && c <= 0x7e));
}

static int yaml_should_escape(unsigned char c)
{
    return (c == '\t' || c == '\n' || c == '\r' || c == '"' || c == '/' ||
            c == '\\');
}

static unsigned char hex_nibble(unsigned char v)
{
    assert(v < 16);

    if (v < 10)
        return v + '0';
    else
        return v - 10 + 'A';
}

char *yaml_encode_double_quoted_string(char *string)
{
    size_t len    = 0;
    int do_encode = 0;
    char *c;

    /* first check if we need to actually do anything... */
    for (c = string; *c != '\0'; c++) {
        if (!is_ascii_printable(*c) || yaml_should_escape(*c)) {
            do_encode = 1;
            len += 4; /* worst case */
        } else
            len++;
    }

    /* nothing to do, return copy of original string */
    if (!do_encode)
        return strdup(string);

    char *encoded    = malloc(len + 1);
    unsigned int idx = 0;

    for (c = string; *c != '\0'; c++) {
        if (!is_ascii_printable(*c) || yaml_should_escape(*c)) {
            encoded[idx++] = '\\';
            switch (*c) {
            /* special shorter versions of common control characters */
            case '\0': /* should never actually get this in a string */
                encoded[idx++] = '0';
                break;
            case '\a':
                encoded[idx++] = 'a';
                break;
            case '\b':
                encoded[idx++] = 'b';
                break;
            case '\t':
                encoded[idx++] = 't';
                break;
            case '\n':
                encoded[idx++] = 'n';
                break;
            case '\v':
                encoded[idx++] = 'v';
                break;
            case '\f':
                encoded[idx++] = 'f';
                break;
            case '\r':
                encoded[idx++] = 'r';
                break;
            case 0x1B:
                encoded[idx++] = 'e';
                break;
            case '"':
                encoded[idx++] = '"';
                break;
            case '/':
                encoded[idx++] = '/';
                break;
            case '\\':
                encoded[idx++] = '\\';
                break;
            /* if we don't have a shortcut, use hexadecimal '\xXX' */
            default:
                encoded[idx++] = 'x';
                encoded[idx++] = hex_nibble((*c >> 4) & 0x0F);
                encoded[idx++] = hex_nibble(*c & 0x0F);
                break;
            }
        } else {
            encoded[idx++] = *c;
        }
    }
    encoded[idx] = '\0';
    return encoded;
}
