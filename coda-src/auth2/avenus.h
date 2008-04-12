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
                           none currently
#*/

#ifndef _AVENUS_H_
#define _AVENUS_H_

int U_DeleteLocalTokens(const char *realm);
int U_GetLocalTokens(ClearToken *cToken, EncryptedSecretToken sToken,
		     const char *realm);
int U_SetLocalTokens(int setPag, ClearToken *cToken,
		     EncryptedSecretToken sToken, const char *realm);

#endif /* _AVENUS_H_ */

