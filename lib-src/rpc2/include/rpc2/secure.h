/* BLURB lgpl

			Coda File System
			    Release 6

	  Copyright (c) 2005-2006 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
#*/

#ifndef _RPC2_SECURE_H_
#define _RPC2_SECURE_H_

#include <stdint.h>

/* initialize */
void secure_init(int verbose);
void secure_release(void);

/* cryptographically strong deterministic pseudo random number generator */
void secure_random_bytes(uint8_t *buf, size_t len);

#endif /* _RPC2_SECURE_H_ */

