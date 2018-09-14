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

#ifndef _BITMAP_H_
#define _BITMAP_H_ 1
/*
 * bitmap.h
 * Created Feb 13, 1992	-- Puneet Kumar 
 * Declaration of a bitmap class 
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef __cplusplus
}
#endif

#define ALLOCMASK	255
#define HIGHBIT		128
/* Bitmap for keeping status of N elements of an array.
 * The class keeps an array of size = 1/8th N
 * Each elements status is stored in a bit
 *	1 --> that element is not free
 *	0 --> that element is free
 */

/* Values of bitmap::malloced; must be 8 bits so can't use enums */
#define BITMAP_NOTVIANEW 193  /* must be on stack */
#define BITMAP_VIANEW 221     /* on heap, via operator new */

class bitmap {
//  friend ostream& operator<<(ostream& s, bitmap *b);  
    uint8_t recoverable;	/* is this bitmap recoverable */
    uint8_t malloced = BITMAP_NOTVIANEW; /* was bitmap allocated via new? */
    int mapsize;		/* 1/8 size of array of elements */
    char *map;			/* bitmap showing status of the elements */

    /**
     * Set the value of a bit at an index
     *
     * @param index  index of the bit to be set 
     * @param value  value to set to bit
     */
    inline void SetValue(int index, int value);

    /**
     * Set the value of bit range
     *
     * @param start  start of the range 
     * @param len    length of the range 
     * @param value  value to be set
     */
    void SetRangeValue(int start, int len, int value);

public:
    /**
     * New operator overloading
     *
     * @param size     memory size in bytes
     * @param recable  recoverable flag (RVM persistent)
     *
     */
    void *operator new (size_t size, int recable = 0);

    /**
     * Constuctor
     *
     * @param inputmapsize  size of the bitmap being created
     * @param recable       recoverable flag (RVM persistent)
     */
    bitmap(int inputmapsize = 0, int recable = 0);

    /**
     * Destructor
     */
    ~bitmap();

    /**
     * Resize the bitmap to a new size
     *
     * @param newsize  bitmap's new size
     */
    void Resize(int newsize);

    /**
     * Grow the bitmap to a new size 
     *
     * @param newsize  bitmap's new size
     */
    void Grow(int newsize);

    /**
     * Get an index that is not in use and mark it
     *
     * @return index obtained
     */
    int GetFreeIndex();

    /**
     * Unset the bit at a particualr index
     *
     * @param index  index of the bit to be unset
     */
    void FreeIndex(int index);

    /**
     * Unset all the bits at a particualr range
     *
     * @param start  start of the range
     * @param len    length of the range
     */
    void FreeRange(int start, int len);

    /**
     * Set the bit at a particualr index
     *
     * @param index  index of the bit to be set
     */
    void SetIndex(int index);

    /**
     * Set all the bits at a particualr range
     *
     * @param start  start of the range
     * @param len    length of the range
     */
    void SetRange(int start, int len);

    /**
     * Copy a range from one bitmap to another
     *
     * @param start  start of the range
     * @param len    length of the range
     * @param b      output bitmap
     */
    void CopyRange(int start, int len, bitmap& b);

    /**
     * Get the bit's value at a particular index
     *
     * @param index  index of the bit to be read
     * @return bit's value
     */
    int Value(int index);

    /**
     * Count the number of 1's in the bitmap
     *
     * @return number of bits set to 1
     */
    int Count();

    /**
     * Obtain the size of the bitmap
     *
     * @return size of the bitmap
     */
    int Size();

    /**
     * Delete the map 
     */
    void purge();

    /**
     * Deep copy the entire bitmap
     *
     * @param b  input bitmap
     */
    void operator =(bitmap& b);

    /**
     * Test for inequality
     *
     * @param b  input bitmap
     * @return 0 if both bitmaps are unequal and 1 otherwise
     */
    int operator !=(bitmap& b); // test for inequality

    /**
     * Print the bitmap's content to the stderr
     */
    void print();

    /**
     * Print the bitmap's content to a file
     *
     * @param fp  pointer to the file struct of the file to be written to
     */
    void print(FILE *fp);

    /**
     * Print the bitmap's content to a file given a file descriptor
     *
     * @param fd  file descriptor of the file to be written to
     */
    void print(int fd);
};

#endif /* _BITMAP_H_ */
