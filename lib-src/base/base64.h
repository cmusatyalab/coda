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

#ifndef _BASE64_H_
#define _BASE64_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

void base64_encode(FILE *out, char *in, int len);
void base64_decode(FILE *in, char **out, int *len);

#ifdef __cplusplus
}
#endif __cplusplus


#endif _BASE64_H_
