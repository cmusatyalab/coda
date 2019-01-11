/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 * bitmap7.c 
 * Created Feb 13, 1992	-- Puneet Kumar
 * Definition for the bitmap class 
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#include <setjmp.h>
#ifdef __cplusplus
}
#endif

#include "util.h"
#include "rvmlib.h"
#include "bitmap7.h"
extern char *hex(long, int = 0);

void *bitmap7::operator new(size_t size, int recable)
{
    bitmap7 *x = NULL;

    if (recable) {
        x = (bitmap7 *)rvmlib_rec_malloc(sizeof(bitmap7));
        CODA_ASSERT(x);
    } else {
        x = (bitmap7 *)malloc(sizeof(bitmap7));
        CODA_ASSERT(x);
    }

    /* GROSS HACK BELOW; if this object was born via new, we
       are confident malloced is set.  But if object was
       allocated on stack, malloced is undefined.  We rely
       on the unlikeliness of the undefined value being exactly
       BITMAP_VIANEW for correctness.  Bletch!!! This would be much
       easier and cleaner if delete() took extra args, just like new()
       Obviously, I disagree with Stroustrup's polemic on this topic on
       pg 66 of the CARM (1st edition, 1991).
       
       Satya, 2/95
       
    */
    x->malloced = BITMAP_VIANEW;
    return (x);
}

void bitmap7::operator delete(void *ptr)
{
    bitmap7 *x = (bitmap7 *)ptr;

    if (x->malloced == BITMAP_VIANEW) {
        if (x->recoverable)
            rvmlib_rec_free(ptr);
        else
            free(ptr);
    }
}

bitmap7::bitmap7(int inputmapsize, int recable)
{
    CODA_ASSERT(
        malloced !=
        BITMAP_NOTVIANEW); /* ensure malloced is undefined if via stack! */
    if (malloced != BITMAP_VIANEW)
        malloced = BITMAP_NOTVIANEW; /* infer I must be on the stack */
    /* From this point on, malloced is definitely defined */

    recoverable = recable;
    if (recoverable)
        rvmlib_set_range(this, sizeof(bitmap7));

    indexsize = inputmapsize;

    if (inputmapsize > 0) {
        /* make sure we allocate enough even if inputmapsize is not multiple of 8 */
        mapsize = (inputmapsize + 7) >> 3;
        if (recoverable) {
            map = (char *)rvmlib_rec_malloc(mapsize);
            CODA_ASSERT(map);
            rvmlib_set_range(map, mapsize);
        } else {
            map = new char[mapsize];
        }

        memset(map, 0, mapsize);
    } else {
        mapsize = 0;
        map     = NULL;
    }
}

bitmap7::~bitmap7()
{
    /* Temporary, until we discover if bitmaps are ever allocated on the stack
       Satya (6/5/95) */
    CODA_ASSERT(malloced == BITMAP_VIANEW);

    if (recoverable) {
        RVMLIB_REC_OBJECT(*this);
        if (map)
            rvmlib_rec_free(map);
    } else {
        if (map)
            delete[] map;
    }
    map     = NULL;
    mapsize = 0;
}

void bitmap7::Resize(int newsize)
{
    /* make sure we allocate enough even if newsize is not multiple of 8 */
    int newmapsize = (newsize + 7) >> 3;
    char *newmap   = NULL;

    if (!newmapsize) {
        newmapsize++;
    }

    if (recoverable) {
        newmap = (char *)rvmlib_rec_malloc(newmapsize);
        CODA_ASSERT(newmap);
        rvmlib_set_range(newmap, newmapsize);
    } else {
        newmap = new char[newmapsize];
        CODA_ASSERT(newmap);
    }

    memset(newmap, 0, newmapsize);

    if (map) {
        CODA_ASSERT(mapsize > 0);

        /* If zero size leave clean */
        if (newsize) {
            if (newmapsize < mapsize) { /* If it's shrinking */
                memcpy(newmap, map, newmapsize);
            } else { /* If it's growing */
                memcpy(newmap, map, mapsize);
            }
        }

        if (recoverable)
            rvmlib_rec_free(map);
        else
            delete[] map;
    }

    if (recoverable)
        rvmlib_set_range(this, sizeof(bitmap7));

    map       = newmap;
    mapsize   = newmapsize;
    indexsize = newsize;

    /* Clean the gap between actual wanted size 
     * and the map size when shrinking */
    for (int i = newsize; i < (newmapsize << 3); i++)
        FreeIndex(i);
}

void bitmap7::Grow(int newsize)
{
    if (newsize < (mapsize << 3))
        return;
    Resize(newsize);
}

int bitmap7::GetFreeIndex()
{
    int j = 0;
    if (!map)
        return (-1);
    for (int offset = 0; offset < mapsize; offset++) {
        if ((~map[offset]) & ALLOCMASK) {
            /* atleast one bit is zero */
            /* set the bit  and return index */
            unsigned char availbits = ~map[offset];
            for (j = 0; j < 8; j++) {
                if ((HIGHBIT >> j) & availbits) {
                    /* jth bit is available */
                    if (recoverable)
                        rvmlib_set_range(&map[offset], sizeof(char));
                    map[offset] |= (128 >> j);
                    break;
                }
            }
            CODA_ASSERT(j < 8);
            return ((offset << 3) + j);
        }
    }
    return (-1); /* no free slot */
}

void bitmap7::SetValue(int index, int value)
{
    int offset    = index >> 3; /* the byte offset into bitmap */
    int bitoffset = index & 7;
    int mask      = (1 << (7 - bitoffset));
    CODA_ASSERT(offset < mapsize);

    if (!map)
        return;

    if (recoverable)
        rvmlib_set_range(&map[offset], sizeof(char));

    /* make sure bit is not set */
    if (value) {
        map[offset] |= mask;
    } else {
        map[offset] &= ~mask;
    }
}

void bitmap7::CopyRange(int start, int len, bitmap7 &b)
{
    int end_bit    = len < 0 ? indexsize : start + len;
    int start_byte = (start + 0x7) >> 3;
    int end_byte   = end_bit >> 3;
    int bulk_len   = end_byte - start_byte;

    if (!map)
        return;

    if (bulk_len <= 0) {
        for (int i = start; i < end_bit; i++) {
            b.SetValue(i, Value(i));
        }
        return;
    }

    /* Copy all the bytes in between */
    memcpy(&b.map[start_byte], &map[start_byte], bulk_len);

    /* Copy the values from before the first copied byte */
    for (int i = start; i & 0x7; i++) {
        b.SetValue(i, Value(i));
    }

    /* Copy the values from after the last copied byte */
    for (int i = (end_byte << 3); i < end_bit; i++) {
        b.SetValue(i, Value(i));
    }
}

void bitmap7::SetRangeValue(int start, int len, int value)
{
    int end_bit        = len < 0 ? indexsize : start + len;
    int start_byte     = (start + 0x7) >> 3;
    int end_byte       = end_bit >> 3;
    int bulk_len       = end_byte - start_byte;
    uint8_t bulk_value = value ? 0xFF : 0;

    if (!map)
        return;

    if (bulk_len <= 0) {
        for (int i = start; i < end_bit; i++) {
            SetValue(i, value & 0x1);
        }
        return;
    }

    /* Copy all the bytes in between */
    memset(&map[start_byte], bulk_value, bulk_len);

    /* Copy the values from before the first copied byte */
    for (int i = start; i & 0x7; i++) {
        SetValue(i, value & 0x1);
    }

    /* Copy the values from after the last copied byte */
    for (int i = (end_byte << 3); i < end_bit; i++) {
        SetValue(i, value & 0x1);
    }
}

void bitmap7::SetIndex(int index)
{
    SetValue(index, 1);
}

void bitmap7::SetRange(int start, int len)
{
    SetRangeValue(start, len, 1);
}

void bitmap7::FreeIndex(int index)
{
    SetValue(index, 0);
}

void bitmap7::FreeRange(int start, int len)
{
    SetRangeValue(start, len, 0);
}

int bitmap7::Value(int index)
{
    int offset    = index >> 3;
    int bitoffset = index & 7;

    if (offset >= mapsize)
        return -1;

    if (!map)
        return 0;

    return (map[offset] & (1 << (7 - bitoffset)));
}

int bitmap7::Count()
{
    int count = 0;
    if (!map)
        return 0;
    for (int i = 0; i < mapsize; i++)
        for (int j = 0; j < 8; j++)
            if (map[i] & (1 << j))
                count++;
    return (count);
}

int bitmap7::Size()
{
    if (!map)
        return 0;
    return (mapsize << 3);
}

void bitmap7::purge()
{
    if (recoverable) {
        RVMLIB_REC_OBJECT(*this);
        rvmlib_rec_free(map);
    } else {
        if (map)
            delete[] map;
    }
    map     = NULL;
    mapsize = 0;
}

void bitmap7::operator=(bitmap7 &b)
{
    if (mapsize != b.mapsize) {
        /* deallocate existing map entry */
        if (map) {
            if (recoverable)
                rvmlib_rec_free(map);
            else
                delete[] map;
        }

        mapsize = b.mapsize;

        /* allocate new map */
        if (recoverable) {
            RVMLIB_REC_OBJECT(*this);
            map = (char *)rvmlib_rec_malloc(mapsize);
            CODA_ASSERT(map);
            rvmlib_set_range(map, mapsize);
        } else {
            map = new char[mapsize];
            CODA_ASSERT(map);
        }

    } else {
        /* use space of old map itself */
        if (recoverable)
            rvmlib_set_range(map, mapsize);
    }

    memcpy(map, b.map, mapsize);
}

int bitmap7::operator!=(bitmap7 &b)
{
    if (mapsize != b.mapsize)
        return (1);

    if (!map)
        return (1);
    if (!b.map)
        return (1);

    for (int i = 0; i < mapsize; i++)
        if (map[i] != b.map[i])
            return (1);

    return (0);
}

void bitmap7::print()
{
    print(stderr);
}

void bitmap7::print(FILE *fp)
{
    print(fileno(fp));
}

void bitmap7::print(int fd)
{
    char buf[512];
    sprintf(buf, "mapsize %d\n map:\n", mapsize);
    write(fd, buf, strlen(buf));
    for (int i = 0; i < mapsize; i++) {
        unsigned long l = (unsigned long)map[i];
        l               = l & 0x000000ff;
        sprintf(buf, "0x%lx ", l);
        write(fd, buf, strlen(buf));
    }
    sprintf(buf, "\n");
    write(fd, buf, strlen(buf));
}

#ifdef notdef
ostream &operator<<(ostream &s, bitmap7 *b)
{
    s << "mapsize = " << b->mapsize << '\n';
    s << "bitmap = ";
    for (int i = 0; i < b->mapsize; i++) {
        long l = (long)(b->map[i]);
        s << hex(l, 2);
    }
    s << '\n';
    return (s);
}
#endif
