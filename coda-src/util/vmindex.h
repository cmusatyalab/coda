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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/util/vmindex.h,v 4.1 1997/01/08 21:51:16 rvb Exp $";
#endif /*_BLURB_*/





#ifndef _VMINDEX_H
#define _VMINDEX_H 1
/* vmindex.h
 * declaration of index class 
 *	This allows user to keep an array of int sized objects.
 *	This array can grow dynamically as more elements are added.
 *	index_iterator class is used to return the list of elements one at a time.
 */

#define DEFAULTINDEXSIZE 32
class vmindex {
    friend class vmindex_iterator;
    unsigned long *indices;
    int size;
    int count;

  public:
    vmindex(int sz = DEFAULTINDEXSIZE);
    ~vmindex();
    void add(unsigned long);
};

class vmindex_iterator {
    vmindex *ind;
    int current_ind;

  public:
    vmindex_iterator(vmindex*);
    ~vmindex_iterator();
    unsigned long operator()();	/* return next index */
};
#endif _VMINDEX_H
