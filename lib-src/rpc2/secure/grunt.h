/* BLURB lgpl
			Coda File System
			    Release 6

	    Copyright (c) 2006 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
#*/

/* helpers and private functions */

#ifndef _GRUNT_H_
#define _GRUNT_H_

#define bytes(bits)	((bits)/8)
#define int32(x)	((uint32_t *)(x))
#define xor128(out, in)	do { \
	int32(out)[0] ^= int32(in)[0]; \
	int32(out)[1] ^= int32(in)[1]; \
	int32(out)[2] ^= int32(in)[2]; \
	int32(out)[3] ^= int32(in)[3]; \
    } while(0)


/* private functions */
void secure_aes_init(int verbose);

/* these might be exported at some point */
void secure_random_init(int verbose);
void secure_random_release(void);

#endif /* _GRUNT_H_ */
