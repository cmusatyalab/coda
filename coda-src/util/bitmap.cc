/* BLURB gpl

                           Coda File System
                              Release 6

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
 * bitmap.c 
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
#include "bitmap.h"
extern char* hex(long, int =0);

void *bitmap::operator new(size_t size, int recable)
{
    bitmap *x = NULL;

    if (recable) {
        x = (bitmap *)rvmlib_rec_malloc(sizeof(bitmap));
        CODA_ASSERT(x);
    }
    else {
        x = (bitmap *) malloc(sizeof(bitmap));
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
    return(x);
}

void bitmap::operator delete(void *ptr)
{
    bitmap * x = (bitmap *) ptr;

    if (x->malloced == BITMAP_VIANEW) {
        if (x->recoverable)
            rvmlib_rec_free(ptr);
        else
            free(ptr);
    }
}

bitmap::bitmap(int inputmapsize, int recable)
{

    CODA_ASSERT(malloced != BITMAP_NOTVIANEW); /* ensure malloced is undefined if via stack! */
    if (malloced != BITMAP_VIANEW) malloced = BITMAP_NOTVIANEW; /* infer I must be on the stack */
    /* From this point on, malloced is definitely defined */

    recoverable = recable;
    if (recoverable)
        rvmlib_set_range(this, sizeof(bitmap));
        
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
        map = NULL;
    }
}

bitmap::~bitmap()
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
    map = NULL;
    mapsize = 0;
}

void bitmap::Resize(int newsize)
{
    /* make sure we allocate enough even if newsize is not multiple of 8 */
    int newmapsize = (newsize + 7) >> 3;
    char *newmap = NULL;

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
            if (newmapsize < mapsize)
            { /* If it's shrinking */
                memcpy(newmap, map, newmapsize);
            } else
            { /* If it's growing */
                memcpy(newmap, map, mapsize);
            }
        }

        if (recoverable)
            rvmlib_rec_free(map);
        else
            delete[] map;
    }

    if (recoverable)
        rvmlib_set_range(this, sizeof(bitmap));

    mapsize = newmapsize;
    map = newmap;
}

void bitmap::Grow(int newsize)
{
    if (newsize < (mapsize << 3)) return;
    Resize(newsize);
}

int bitmap::GetFreeIndex()
{
    int j = 0;
    if (!map) return(-1);
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
            return((offset << 3) + j);
        }
    }
    return(-1);		/* no free slot */
}

void bitmap::SetValue(int index, int value)
{
    int offset = index >> 3;	/* the byte offset into bitmap */
    int bitoffset = index & 7;
    int mask = (1 << (7 - bitoffset));
    CODA_ASSERT(offset < mapsize);

    if (!map) return;

    if (recoverable) rvmlib_set_range(&map[offset], sizeof(char));

    /* make sure bit is not set */
    if (value) {
        map[offset] |= mask;
    } else  {
        map[offset] &= ~mask;
    }
}

void bitmap::CopyRange(int start, int len, bitmap& b)
{
    int bit_end = len < 0 ? mapsize - 1 : start + len;
    int byte_start = start;
    int byte_end = bit_end;

    if (!map) return;

    byte_start = (byte_start + 0x7) & ~0x7;
    byte_end = byte_end & ~0x7;

    if (byte_start >= byte_end) {
        for (int i = start; i < bit_end; i++) {
            SetValue(i, Value(i));
        }
        return;
    }

    byte_start = byte_start >> 3;
    byte_end = byte_end >> 3;

    memcpy(&b.map[byte_start], &map[byte_start], byte_end - byte_start);

    for (int i = start; i < (byte_start << 3); i++) {
        b.SetValue(i, Value(i));
    }

    for (int i = (byte_end << 3); i < bit_end; i++) {
        b.SetValue(i, Value(i));
    }
}

void bitmap::SetRangeValue(int start, int len, int value)
{
    int bit_end = len < 0 ? mapsize - 1 : start + len;
    int byte_start = start;
    int byte_end = bit_end;

    if (!map) return;

    byte_start = (byte_start + 0x7) & ~0x7;
    byte_end = byte_end & ~0x7;

    if (byte_start >= byte_end) {
        for (int i = start; i < bit_end; i++) {
            SetValue(i, value);
        }
        return;
    }

    byte_start = byte_start >> 3;
    byte_end = byte_end >> 3;

    memset(&map[byte_start], value, byte_end - byte_start);

    for (int i = start; i < (byte_start << 3); i++) {
        SetValue(i, value);
    }

    for (int i = (byte_end << 3); i < bit_end; i++) {
        SetValue(i, value);
    }
}

void bitmap::SetIndex(int index)
{
    SetValue(index, 1);
}

void bitmap::SetRange(int start, int len)
{
    SetRangeValue(start, len, 1);
}

void bitmap::FreeIndex(int index)
{
    SetValue(index, 0);
}

void bitmap::FreeRange(int start, int len)
{
    SetRangeValue(start, len, 0);
}

int bitmap::Value(int index)
{
    int offset = index >> 3;
    int bitoffset = index & 7;
    
    if (offset >= mapsize)
        return -1;

    if (!map) return 0;

    return(map[offset] & (1 << (7 - bitoffset)));
}

int bitmap::Count()
{
    int count = 0;
    if (!map) return 0;
    for (int i = 0; i < mapsize; i++) 
        for (int j = 0; j < 8; j++)
            if (map[i] & (1 << j))
                count++;
    return(count);
}

int bitmap::Size()
{
    if (!map) return 0;
    return (mapsize << 3);
}

void bitmap::purge()
{
    if (recoverable) {
        RVMLIB_REC_OBJECT(*this);
        rvmlib_rec_free(map);
    } else {
        if (map)
            delete[] map;
    }
    map = NULL;
    mapsize = 0;
}

void bitmap::operator=(bitmap& b)
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

int bitmap::operator!=(bitmap& b)
{
    if (mapsize != b.mapsize)
        return (1);

    if (!map) return (1);
    if (!b.map) return (1);

    for (int i = 0; i < mapsize; i++)
        if (map[i] != b.map[i]) return(1);

    return(0);
}

void bitmap::print()
{
    print(stderr);
}

void bitmap::print(FILE *fp)
{
    print(fileno(fp));
}

void bitmap::print(int fd)
{
    char buf[512];
    sprintf(buf, "mapsize %d\n map:\n", mapsize);
    write(fd, buf, strlen(buf));
    for (int i = 0; i < mapsize; i++) {
        unsigned long l = (unsigned long) map[i];
        l = l & 0x000000ff;
        sprintf(buf, "0x%lx ",l);
        write(fd, buf, strlen(buf));
    }
    sprintf(buf, "\n");
    write(fd, buf, strlen(buf));
}

#ifdef notdef
ostream &operator<<(ostream& s, bitmap *b)
{
    s << "mapsize = " << b->mapsize << '\n';
    s << "bitmap = ";
    for (int i = 0; i < b->mapsize; i++) {
        long l = (long)(b->map[i]);
        s << hex(l, 2);
    }
    s << '\n';
    return(s);
}
#endif
