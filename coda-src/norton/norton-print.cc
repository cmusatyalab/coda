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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/norton/norton-print.cc,v 1.1 1996/11/22 19:14:55 braam Exp $";
#endif /*_BLURB_*/




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
