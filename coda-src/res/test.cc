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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/res/Attic/test.cc,v 4.1 1997/01/08 21:50:05 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <libc.h>
#include <stdio.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "logalloc.h"

void DumpVolResLog(PMemMgr *m, int j) {
    printf("Bitmpasize = %d\n", m->bitmapSize);
    printf("Bitmap looks like\n0x");
    for (int i = 0; i < m->bitmapSize; i++) 
	printf("%x ", m->bitmap[i]);
}

void main() {
    PMemMgr *pm;
    pm = new PMemMgr(8, 1);
    
    char *arr[16];
    for (int i = 0; i < 12 ; i++) {
	arr[i] = (char *)pm->NewMem();
	sprintf(arr[i], "%d\0", i);
    }

    for (i = 0; i < 12; i++) 
	printf("number %d is %s\n", i, arr[i]);

    pm->FreeMem(arr[10]);
    DumpVolResLog(pm, 1);
}
    
