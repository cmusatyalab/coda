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

#include <stdio.h>
#include <mach/boolean.h>
    
#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>

#include "norton.h"

void PrintVV(vv_t *vv) {
    int i;
    
    printf("{[");
    for (i = 0; i < VSG_MEMBERS; i++)
        printf(" %d", (&(vv->Versions.Site0))[i]);
    printf(" ] [ %d %d ] [ %#x ]}\n",
             vv->StoreId.Host, vv->StoreId.Uniquifier, vv->Flags);
}



void print_volume(VolHead * vol) {
    printf("    Id: %x  \tName: %s \tParent: %x\n",
	   vol->header.id,
	   vol->data.volumeInfo->name,
	   vol->header.parent);
    printf("    GoupId: %x \tPartition: %s\n",
	   vol->data.volumeInfo->groupId,
	   vol->data.volumeInfo->partition);
    printf("    Version Vector: ");
    PrintVV(&vol->data.volumeInfo->versionvector);
    printf("\n    \t\tNumber vnodes	Number Lists	Lists\n");
    printf("    \t\t-------------	------------	----------\n");
    printf("    small\t%13d\t%12d\t0x%8x\n",
	   vol->data.nsmallvnodes,
	   vol->data.nsmallLists,
	   vol->data.smallVnodeLists);
    printf("    large\t%13d\t%12d\t0x%8x\n",
	   vol->data.nlargevnodes,
	   vol->data.nlargeLists,
	   vol->data.largeVnodeLists);
}
