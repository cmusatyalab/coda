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

#ifndef _TOKENFILE_H_
#define _TOKENFILE_H_

#include <auth2.h>

void WriteTokenToFile(char *filename, ClearToken *cToken,
		      EncryptedSecretToken sToken);
void ReadTokenFromFile(char *filename, ClearToken *cToken,
                       EncryptedSecretToken sToken);

#endif
