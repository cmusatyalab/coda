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
