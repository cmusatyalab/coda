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

#ifndef _GETSECRET_H_
#define _GETSECRET_H_

#include <rpc2/rpc2.h>

/* Read a tokenfile and return an RPC2 encryption key. */
int GetSecret(char *tokenfile, RPC2_EncryptionKey key);

#endif /* _GETSECRET_H_ */

