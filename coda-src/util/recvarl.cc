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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/util/recvarl.cc,v 4.2 1997/02/26 16:03:07 rvb Exp $";
#endif /*_BLURB_*/





/*
 * recvarl.c 
 *	definition of variable length class
 */
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <string.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <setjmp.h>
    
#ifdef __cplusplus
}
#endif __cplusplus

#include "util.h"
#include "rvmlib.h"
#include "recvarl.h"

void *recvarl::operator new(size_t size, int recvsize) {
    recvarl *r = 0;
    r = (recvarl *)rvmlib_rec_malloc(recvsize + sizeof(recvarl_length_t));
    assert(r);
    return(r);
}

void *recvarl::operator new(size_t size) {
    assert(0); /* dummy definition of new() to pacify g++; should never get here*/
    return(0);
}

void recvarl::operator delete(void *deadobj, size_t size) {
    assert(0); /* destructor should never let control get here */
}


recvarl::recvarl(int recvarlsize) {
    rvmlib_set_range(this, recvarlsize + sizeof(recvarl_length_t));
    length = recvarlsize;
    char *c = (char *)&(this->vfld[0]);
    bzero(c, recvarlsize);
}

/* the destructor should never be called 
   because it is possible to call destructor only
   with delete - which calls the c++ delete first */
recvarl::~recvarl() {
    assert(0);
}

int recvarl::size() {		/* return size of particular instance of varl class */
    return(length + sizeof(length));
}

void *recvarl::end() {
    return((char *)this + length + sizeof(length));
}


/* not sure if this will work */
void recvarl::destroy() {
    assert(this);
    rvmlib_rec_free(this);
//  this = 0;   Assignment to this no longer allowed; we lose some safety..
}
