/* BLURB lgpl

                           Coda File System
                              Release 5

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

#include <sys/types.h>

/*=====================================================================*/
/* stuff for packing/unpacking db values */

struct rwcdb_tuple {
    u_int32_t a;
    u_int32_t b;
};

static __inline__ void packints(char *buf, const u_int32_t a, const u_int32_t b)
{
    struct rwcdb_tuple *p = (struct rwcdb_tuple *)buf;
#if __BYTE_ORDER == __BIG_ENDIAN
    p->a = bswap32(a);
    p->b = bswap32(b);
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    p->a = a;
    p->b = b;
#else
#error "Need to know how to convert from native to Little Endian order"
#endif
}

static __inline__ void unpackints(char *buf, u_int32_t *a, u_int32_t *b)
{
    struct rwcdb_tuple *p = (struct rwcdb_tuple *)buf;
#if __BYTE_ORDER == __BIG_ENDIAN
    *a = bswap32(p->a);
    *b = bswap32(p->b);
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    *a = p->a;
    *b = p->b;
#else
#error "Need to know how to convert from Little Endian to native order"
#endif
}

#endif /* _RWCDB_PACK_H_ */
