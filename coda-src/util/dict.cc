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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/util/dict.cc,v 4.5 1998/09/15 14:28:03 jaharkes Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <string.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "dict.h"


void dictionary::Add(assoc *Assoc) {
    /* Can't do the following sanity check, since Key may not be valid yet! */
//    CODA_ASSERT(Find(Assoc->Key()) == 0);

    insert((dlink *)Assoc);
}


void dictionary::Remove(assoc *Assoc) {
    CODA_ASSERT(remove((dlink *)Assoc) == (dlink *)Assoc);
}


assoc *dictionary::Find(assockey& Key) {
    assoc *a;
    dlist_iterator next(*this);
    while (a = (assoc *)next())
	if (Key == a->Key())
	    { a->Hold(); return(a); }

    return(0);
}


void dictionary::Put(assoc **Assoc) {
    if (*Assoc == 0) return;

    (*Assoc)->Release();

    *Assoc = 0;
}


void dictionary::Kill(assockey& Key) {
    assoc *a = Find(Key);
    if (a == 0) return;

    a->Suicide();
    Put(&a);
}


assoc::~assoc() {
    /* Derived class MUST provide a dtor! */
    CODA_ASSERT(0);
}


void assoc::Hold() {
    refcnt++;
}


void assoc::Release() {
    CODA_ASSERT(refcnt > 0);
    refcnt--;

    if (dying && refcnt == 0) {
	dict->Remove(this);
	delete this;		    /* this is the reason dtor is virtual! */
    }
}


void assoc::Suicide() {
    CODA_ASSERT(refcnt > 0);
    if (dying) return;

    dying = 1;
}


assocrefs::assocrefs(int InitialSize, int GrowSize) {
    max = InitialSize;
    count = 0;
    growsize = GrowSize;
    assocs = (max == 0 ? 0 : (assoc **)malloc(max * sizeof(assoc *)));
}


assocrefs::~assocrefs() {
    CODA_ASSERT(count == 0);

    if (assocs != 0)
	free(assocs);
}


/* ix of -1 indicates any available index */
void assocrefs::Attach(assoc *Assoc, int ix) {
    /* Grow array if necessary. */
    if (count == max || (ix != -1 && ix >= max)) {
	int ActualGrowSize = growsize;
	int NewMax = max + ActualGrowSize;
	if (ix != -1 && ix >= NewMax) {
	    ActualGrowSize = ix - max + growsize;
	    NewMax = max + ActualGrowSize;
	}

	if (assocs == 0) {
	    assocs = (assoc **)malloc(NewMax * sizeof(assoc *));
	    bzero((char *)assocs, NewMax * sizeof(assoc *));
	}
	else {
	    assocs = (assoc **)realloc(assocs, NewMax * sizeof(assoc *));
	    bzero((char *)assocs + max - ActualGrowSize, ActualGrowSize * sizeof(assoc *));
	}

	max = NewMax;
    }

    /* Stick a reference in the specified or the first free index. */
    if (ix == -1) {
	for (ix = 0; ix < max; ix++) {
	    if (assocs[ix] != 0) continue;

	    break;
	}
	CODA_ASSERT(ix < max);
    }
    else
	CODA_ASSERT(assocs[ix] == 0);
    count++;
    assocs[ix] = Assoc;
    Assoc->Hold();
}


/* Argument of 0 indicates ALL referenced assocs. */
void assocrefs::Detach(assoc *Assoc) {
    int DetachAll = (Assoc == 0);

    if (!DetachAll) {
	CODA_ASSERT(assocs != 0);
	CODA_ASSERT(count > 0);
    }

    for (int i = 0; i < max; i++)
	if (DetachAll || assocs[i] == Assoc) {
	    Assoc->Release();
	    assocs[i] = 0;
	    count--;
	    if (!DetachAll) return;
	}

    if (!DetachAll) CODA_ASSERT(0);
}


/* Argument of 0 indicates ALL referenced assocs. */
void assocrefs::Kill(assoc *Assoc) {
    int KillAll = (Assoc == 0);

    if (!KillAll) {
	CODA_ASSERT(assocs != 0);
	CODA_ASSERT(count > 0);
    }

    for (int i = 0; i < max; i++)
	if (KillAll || assocs[i] == Assoc) {
	    Assoc->Suicide();
	    Assoc->Release();
	    assocs[i] = 0;
	    count--;
	    if (!KillAll) return;
	}

    if (!KillAll) CODA_ASSERT(0);
}


int assocrefs::Index(assoc *Assoc) {
    for (int i = 0; i < max; i++)
	if (assocs[i] == Assoc) return(i);
    return(-1);
}


assocrefs_iterator::assocrefs_iterator(assocrefs& A) {
    a = &A;
    i = 0;
}


const assoc *assocrefs_iterator::operator()(int *ip) {
    if (ip) *ip = -1;

    if (i == -1) return(0);

    for (; i < a->max; i++) {
	if (a->assocs[i] == 0) continue;

	i++;
	if (ip) *ip = (i - 1);
	return(a->assocs[i - 1]);
    }

    i = -1;
    return(0);
}
