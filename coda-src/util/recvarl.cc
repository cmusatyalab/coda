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





/*
 * recvarl.c 
 *	definition of variable length class
 */
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

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
#endif __cplusplus

#include "util.h"
#include "rvmlib.h"
#include "recvarl.h"

void *recvarl::operator new(size_t size, int recvsize) {
    recvarl *r = 0;
    r = (recvarl *)rvmlib_rec_malloc(recvsize + sizeof(recvarl_length_t));
    CODA_ASSERT(r);
    return(r);
}

void *recvarl::operator new(size_t size) {
    CODA_ASSERT(0); /* dummy definition of new() to pacify g++; should never get here*/
    // return(0);
}

void recvarl::operator delete(void *deadobj, size_t size) {
    CODA_ASSERT(0); /* destructor should never let control get here */
}


recvarl::recvarl(int recvarlsize) {
    rvmlib_set_range(this, recvarlsize + sizeof(recvarl_length_t));
    length = recvarlsize;
    char *c = (char *)&(this->vfld[0]);
    memset(c, 0, recvarlsize);
}

/* the destructor should never be called 
   because it is possible to call destructor only
   with delete - which calls the c++ delete first */
recvarl::~recvarl() {
    CODA_ASSERT(0);
}

int recvarl::size() {		/* return size of particular instance of varl class */
    return(length + sizeof(length));
}

void *recvarl::end() {
    return((char *)this + length + sizeof(length));
}


/* not sure if this will work */
void recvarl::destroy() {
    CODA_ASSERT(this);
    rvmlib_rec_free(this);
//  this = 0;   Assignment to this no longer allowed; we lose some safety..
}
