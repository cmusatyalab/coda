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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/util/testbitmap.cc,v 4.1 97/01/08 21:51:11 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
   
#ifdef __cplusplus
}
#endif __cplusplus

#include "bitmap.h"

int RvmType = 3;

main() {
    char *c;
    c = new char[100];
    delete[100] c;
    bitmap *b = new bitmap(32);
    delete(b);
    printf("Just deleted b\n");
    b = new bitmap(32);
    b->print();
    
    for (int i = 0; i < 32; i++) {
	printf("Allocating %d\n", i);
	b->SetIndex(i);
	if (i != 0 && (i % 2 != 0) ) b->FreeIndex(i - 1);
	b->print();
    }

    b->Grow(48);

    int alloc = b->GetFreeIndex();
    printf("Allocated index %d\n", alloc);
    b->print();

    while ((alloc = b->GetFreeIndex()) != -1) {
	printf("got one more index %d\n", alloc);
	b->print();
    }
    
    printf("deleting b\n");
    delete b;
}
