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





#ifndef _ARRLIST_H_
#define _ARRLIST_H_ 1
// arrlist: 
//	pointers to any type stored as an array 
class arrlist {
  public:
    void **list;
    int	maxsize;
    int cursize;

    int Grow(int =0);
    void init(int);
//  public:
    arrlist();
    arrlist(int);
    ~arrlist();
    void add(void *);
};

class arrlist_iterator {
    arrlist *alp;
    int	previndex;

  public:
    arrlist_iterator(arrlist *);
    ~arrlist_iterator();
    void *operator()();         /* Does *not* support safe deletion of
                                   currently returned entry.  See
                                   dlist.h for more explanation */
};

#endif /* _ARRLIST_H_ */
