/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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
    unsigned recoverable:8;	/* is this bitmap recoverable */
    unsigned malloced:8;	/* was bitmap allocated via new? */
    int mapsize;		/* 1/8 size of array of elements */
    char *map;			/* bitmap showing status of the elements */
    
    inline void SetValue(int index, int value);

  public:
    void *operator new (size_t, int = 0);
    void operator delete(void *);
    bitmap(int = 0, int = 0);
    ~bitmap();
    void Resize(int);		/* resize the index to a new size */
    void Grow(int);		/* grow the index to a new size */
    int GetFreeIndex();		/* get an index that is not in use and mark it */
    void FreeIndex(int);	/* free a particular index */
    void SetIndex(int);		/* mark a particular index as being used */
    int Value(int);		/* get the value at a particular index */
    int Count();		/* count the number of 1's in the bitmap */
    int Size();			// how many entries in bitmap 
    void purge();		// delete the map 
    void operator =(bitmap& b); // copy bitmaps 
    int operator !=(bitmap& b); // test for inequality
    void print();
    void print(FILE *);
    void print(int);
};
#endif /* _BITMAP_H_ */
