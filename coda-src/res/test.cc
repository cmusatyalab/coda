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
#endif __cplusplus

#include <stdlib.h>
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
    
