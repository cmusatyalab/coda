#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/bitmap.h,v 4.1 1997/01/08 21:51:02 rvb Exp $";
#endif /*_BLURB_*/





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


  public:
    void *operator new (size_t, int = 0);
    void operator delete(void *, size_t);
    bitmap(int = 0, int = 0);
    ~bitmap();
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
#endif _BITMAP_H_
