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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/norton/norton-vnode.cc,v 1.3 1997/01/07 18:40:54 rvb Exp";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <strings.h>
#ifdef	__MACH__
#include <mach/boolean.h>
#endif
    
#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <ops.h>
#include <camprivate.h>
#include <coda_globals.h>

#include "parser.h"
#include "norton.h"



void PrintVnodeDiskObject(VnodeDiskObject *vnode)
{
    printf("    type = ");
    switch(vnode->type) {
      case vNull: 	 printf("null     ");	break;
      case vFile: 	 printf("file     ");	break;
      case vDirectory:   printf("directory");	break;
      case vSymlink: 	 printf("symlink  ");	break;
    }
    printf("\tcloned = %u\tmode = %o\tlinks = %u\n",
	   vnode->cloned, vnode->modeBits, vnode->linkCount);
    printf("    length = %u\tunique = %u\tversion = %u\tinode = %u\n",
	   vnode->length, vnode->uniquifier, vnode->dataVersion,
	   vnode->inodeNumber); 
    printf("    vv = ");
    PrintVV(&vnode->versionvector);
    printf("    volindex = %d\tmodtime = %u\tauthor = %u\towner = %u\n",
	vnode->vol_index, vnode->unixModifyTime, vnode->author, vnode->owner);
    printf("    parent = 0x%x.0x%x\tmagic = 0x%x\n    servermodtime = %u\n",
	   vnode->vparent, vnode->uparent, vnode->vnodeMagic,
	   vnode->serverModifyTime); 
}



void show_vnode(int argc, char *argv[]) {
    int volid,
	vnode,
	unique;


    if ((argc != 5) ||
	(parse_int(argv[2], &volid) != 1) ||
	(parse_int(argv[4], &unique) != 1)) {
	fprintf(stderr, "Usage: show vnode <volid> [<vnode> | ?] <unique> \n");
	return;
    }

    if (parse_int(argv[3], &vnode) == 1) {
	show_vnode(volid, vnode, unique);
    }
    else if (*argv[3] == '?') {
	show_vnode(volid, unique);
    }
    else {
	fprintf(stderr, "Usage: show vnode <volid> [<vnode> | ?] <unique> \n");
    }
}


void show_vnode(int volid, int uniquifier) {
    Error   error;
    VnodeId vnodeindex;
    int     vclass = vSmall;  /* HACK FIX THIS! */
    int	    volindex;
    char    buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    VolHead *vol;
    
    volindex = GetVolIndex(volid);
    if (volindex < 0) {
	fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	return;
    }

    vol = GetVol(volid);
    if (vol == NULL) {
	fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	return;
    }
	
    for (vnodeindex=0; vnodeindex < vol->data.nsmallLists; vnodeindex++) {
	if (ExtractVnode(&error, volindex, vclass, (VnodeId)vnodeindex,
			 (Unique_t)uniquifier, vnode) < 0) {
	    continue;
	}
	    

	printf("    vnode number: 0x%x\tvnode index: 0x%x\n", 2*(vnodeindex+1), vnodeindex);
	PrintVnodeDiskObject(vnode);
	if (vnode->log) {
	    printf("\n    Vnode Resolution Log:\n");
	    PrintLog(vnode->log, stdout);
	}
    }
}


void show_vnode(int volid, int vnum, int uniquifier) {
    Error   error;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    int     vclass = vnodeIdToClass(vnum);
    int	    volindex;
    char    buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    
    
    volindex = GetVolIndex(volid);
    if (volindex < 0) {
	fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	return;
    }

    if (ExtractVnode(&error, volindex, vclass, (VnodeId)vnodeindex,
		     (Unique_t)uniquifier, vnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, vnum,
		uniquifier);
	return;
    }

    printf("    vnode number: 0x%x\tvnode index: 0x%x\n", vnum, vnodeindex);
    PrintVnodeDiskObject(vnode);
    if (vnode->log) {
	printf("\n    Vnode Resolution Log:\n");
	PrintLog(vnode->log, stdout);
    }
}


void show_free(int argc, char *argv[]) {
    struct VnodeDiskObject **free_list;
    int	nvnodes,
	i;
    
    if (argc != 3) {
	fprintf(stderr, "Usage: show free <large> | <small>\n");
	return;
    }

    if (strncasecmp("small", argv[2], (int)strlen(argv[2])) == 0) {
	free_list = CAMLIB_REC(SmallVnodeFreeList);
	nvnodes = CAMLIB_REC(SmallVnodeIndex);
	printf("    There are %d small vnodes in the free list\n", nvnodes);
    } else if (strncasecmp("large", argv[2], (int)strlen(argv[2])) == 0) {
	free_list = CAMLIB_REC(LargeVnodeFreeList);
	nvnodes = CAMLIB_REC(LargeVnodeIndex);
	printf("    There are %d large vnodes in the free list\n", nvnodes);	
    } else {
	fprintf(stderr, "Usage: show free <large> | <small>\n");
	return;
    }

    for (i = 0; i < nvnodes; i++) {
	/* We should check if the vnode is != 0 and only print then */
	if (*(free_list + i)) {
	    printf("---------------------------\n");
	    PrintVnodeDiskObject(*(free_list + i));
	}
    }
}    
