/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/





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
