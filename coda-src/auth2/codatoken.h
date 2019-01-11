/* BLURB gpl

			   Coda File System
			      Release 6

	    Copyright (c) 2006 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

			Additional copyrights

#*/

#ifndef _CODATOKEN_H_
#define _CODATOKEN_H_

#include <stdint.h>
#include "auth2.h"

#define AUTH2KEYSIZE 48

int getauth2key(uint8_t *token, size_t token_size,
                uint8_t auth2key[AUTH2KEYSIZE]);
int generate_CodaToken(uint8_t auth2key[AUTH2KEYSIZE], uint32_t viceid,
                       uint32_t lifetime, ClearToken *ctoken,
                       EncryptedSecretToken estoken);
int validate_CodaToken(uint8_t auth2key[AUTH2KEYSIZE],
                       EncryptedSecretToken estoken, uint32_t *viceid,
                       time_t *endtime, RPC2_EncryptionKey *sessionkey);

#endif /* _CODATOKEN_H_ */
