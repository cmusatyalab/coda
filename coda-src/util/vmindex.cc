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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/util/vmindex.cc,v 4.1 1997/01/08 21:51:16 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#ifdef __MACH__
#include <libc.h>
#endif /* __MACH__ */

#include <stdio.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include "util.h"
#include "vmindex.h"

vmindex::vmindex(int sz) {
    if (sz > 0) {
	indices = new unsigned long[sz];
	CODA_ASSERT(indices);
	size = sz;
	count = 0;
    }
    else {
	sz = 0;
	indices = NULL;
	count = 0;
    }
}

vmindex::~vmindex() {
    if (indices) {
	delete[] indices;
    }
    indices = 0;
    count = size = 0;
}

void vmindex::add(unsigned long a) {
    if (count >= size) {
	/* grow index */
	int newsize;
	if (size) newsize = size * 2;
	else newsize = DEFAULTINDEXSIZE;
	unsigned long *newindex = new unsigned long[newsize];
	CODA_ASSERT(newindex);
	for (int i = 0; i < size; i++) 
	    newindex[i] = indices[i];
	delete[] indices;
	indices = newindex;
	size = newsize;
    }
    
    indices[count] = a;
    count++;
}

vmindex_iterator::vmindex_iterator(vmindex *i) {
     ind = i;
     current_ind = 0;
}

vmindex_iterator::~vmindex_iterator() {
    ind = 0;
}

unsigned long vmindex_iterator::operator()() {
    unsigned long rval = -1;
    if (ind && ind->indices && (current_ind < ind->count)) {
	rval = ind->indices[current_ind];
	current_ind++;
    }
    return(rval);
}

    
