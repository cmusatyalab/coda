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





#ifndef _REC_VARL_H
#define _REC_VARL_H 1
/*
 * varl.h
 * declaration of the variable length class
 *	created 02/20/92 -- Puneet Kumar
 *
 */

typedef int recvarl_length_t;  // to allow sizeof() in recvarl::new()
 
class recvarl {
  public:
    recvarl_length_t length; 	/* end of the class */
    unsigned long vfld[1];	/* beginning of variable length part */

    void *operator new(size_t, int); /* the real new */
    void *operator new(size_t);    /* dummy to keep g++ happy */
    void operator delete(void *, size_t);
    recvarl(int); 
    ~recvarl();
    int size();			/* return sizeof(varl) for a particular  class instance */
    void *end();		/* return pointer past end of block */
    void destroy();
};
#endif /* _REC_VARL_H */
