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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/venus/binding.h,v 1.1.1.1 1996/11/22 19:11:41 rvb Exp";
#endif /*_BLURB_*/







/*
 *    Generic associative data structure.
 */

#ifndef _BINDING_H_
#define _BINDING_H_ 1


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from util */
#include <dlist.h>


class binding;


class binding {
  public:
#ifdef	VENUSDEBUG
    static int allocs;
    static int deallocs;
#endif	VENUSDEBUG

    dlink binder_handle;
    void *binder;
    dlink bindee_handle;
    void *bindee;

    binding();
    binding(binding& b) { abort(); }	/* not supported! */
    operator=(binding&) { abort(); }	/* not supported! */
    ~binding();

    void print() { print(stdout); }
    void print(FILE *fp) { fflush(fp); print(fileno(fp)); }
    void print(int);
};

#endif	not _BINDING_H_
