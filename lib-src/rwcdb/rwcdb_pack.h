/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
*/

#ifndef _RWCDB_PACK_H_
#define _RWCDB_PACK_H_

#include <config.h>
#include <sys/types.h>

/* really bad hack, but this should work for 'most' systems :) */
#ifndef __BYTE_ORDER
#define __BIG_ENDIAN    4321
#define __LITTLE_ENDIAN 1234
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

/* Indirect thanks to Alan Barrett for figuring out which 'standard' headers
 * are used to define the 'standard' byte swapping functions. */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SWAP_IN(x)  (x)
#define SWAP_OUT(x) (x)
#elif HAVE_BYTESWAP_H
#include <byteswap.h>
#define SWAP_IN(x)  bswap_32(x)
#define SWAP_OUT(x) bswap_32(x)
#elif HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#define SWAP_IN(x)  bswap32(x)
#define SWAP_OUT(x) bswap32(x)
#elif HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#define SWAP_IN(x)  le32toh(x)
#define SWAP_OUT(x) htole32(x)
#else
#error "Need to know how to convert between native and little endian byteorder"
#endif

/*=====================================================================*/
/* stuff for packing/unpacking db values */

struct rwcdb_tuple {
    u_int32_t a;
    u_int32_t b;
};

static __inline__ void packints(char *buf, const u_int32_t a, const u_int32_t b)
{
    struct rwcdb_tuple *p = (struct rwcdb_tuple *)buf;
    p->a = SWAP_OUT(a);
    p->b = SWAP_OUT(b);
}

static __inline__ void unpackints(char *buf, u_int32_t *a, u_int32_t *b)
{
    struct rwcdb_tuple *p = (struct rwcdb_tuple *)buf;
    *a = SWAP_IN(p->a);
    *b = SWAP_IN(p->b);
}

#endif /* _RWCDB_PACK_H_ */
