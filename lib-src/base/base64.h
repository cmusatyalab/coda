/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _BASE64_H_
#define _BASE64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

void coda_base64_encode(FILE *out, char *in, int len);
void coda_base64_decode(FILE *in, char **out, int *len);

#ifdef __cplusplus
}
#endif


#endif /* _BASE64_H_ */
