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

/* Common routines for the updateclnt/srv/fetch programs */

#ifndef _UPDATECOMMON_H_
#define _UPDATECOMMON_H_

#include <rpc2/rpc2.h>

int GetSecret(char *tokenfile, RPC2_EncryptionKey key);

#endif /* _UPDATECOMMON_H_ */

