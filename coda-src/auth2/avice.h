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
                           none currently
#*/

#ifndef _AVICE_H_
#define _AVICE_H_

long GetKeysFromToken(RPC2_Integer *AuthenticationType,
		      RPC2_CountedBS *cIdent,
                      RPC2_EncryptionKey hKey,
                      RPC2_EncryptionKey sKey);

void SetServerKeys(RPC2_EncryptionKey serverKey1, 
		   RPC2_EncryptionKey serverKey2);

#endif /* _AVICE_H_ */

