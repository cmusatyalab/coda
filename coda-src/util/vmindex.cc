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





#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

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

long vmindex_iterator::operator()() {
    long rval = -1;
    if (ind && ind->indices && (current_ind < ind->count)) {
	rval = ind->indices[current_ind];
	current_ind++;
    }
    return(rval);
}

    
