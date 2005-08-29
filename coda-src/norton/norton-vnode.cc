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
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"
    
#ifdef __cplusplus
}
#endif

#include <cvnode.h>
#include <volume.h>
#include <index.h>
#include <recov.h>
#include <ops.h>
#include <camprivate.h>
#include <coda_globals.h>

#include <parser.h>
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
    printf("    length = %u\tunique = %08x\tversion = %u\tinode = %u\n",
	   vnode->length, vnode->uniquifier, vnode->dataVersion,
	   vnode->inodeNumber); 
    printf("    vv = ");
    PrintVV(&vnode->versionvector);
    printf("    volindex = %d\tmodtime = %u\tauthor = %u\towner = %u\n",
	vnode->vol_index, vnode->unixModifyTime, vnode->author, vnode->owner);
    printf("    parent = %08x.%08x\tmagic = %x\n    servermodtime = %u\n",
	   vnode->vparent, vnode->uparent, vnode->vnodeMagic,
	   vnode->serverModifyTime); 
}



void show_vnode(VolumeId volid, Unique_t uniquifier)
{
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
	if (ExtractVnode(&error, volindex, vclass, vnodeindex, uniquifier, vnode) < 0) {
	    continue;
	}
	    

	printf("    vnode number: %08x\tvnode index: %u\n", 2*(vnodeindex+1), vnodeindex);
	PrintVnodeDiskObject(vnode);
	if (vnode->log) {
	    printf("\n    Vnode Resolution Log:\n");
	    PrintLog(vnode->log, stdout);
	}
    }
}


void show_vnode(VolumeId volid, VnodeId vnum, Unique_t uniquifier)
{
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

    if (ExtractVnode(&error, volindex, vclass, vnodeindex, uniquifier, vnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, vnum,
		uniquifier);
	return;
    }

    printf("    vnode number: %08x\tvnode index: %u\n", vnum, vnodeindex);
    PrintVnodeDiskObject(vnode);
    if (vnode->log) {
	printf("\n    Vnode Resolution Log:\n");
	PrintLog(vnode->log, stdout);
    }
}

void show_vnode(int argc, char *argv[])
{
    unsigned int volid,
	vnode,
	unique;

    if ((argc != 5) ||
	(Parser_uint(argv[2], &volid) != 1) ||
	(Parser_uint(argv[4], &unique) != 1)) {
	fprintf(stderr, "Usage: show vnode <volid> [<vnode> | ?] <unique> \n");
	return;
    }

    if (Parser_uint(argv[3], &vnode) == 1) {
	show_vnode((VolumeId)volid, (VnodeId)vnode, (Unique_t)unique);
    }
    else if (*argv[3] == '?') {
	show_vnode((VolumeId)volid, (VnodeId)unique);
    }
    else {
	fprintf(stderr, "Usage: show vnode <volid> [<vnode> | ?] <unique> \n");
    }
}



void show_free(int argc, char *argv[])
{
    char zerobuf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *zerovn = (VnodeDiskObject *)zerobuf, **free_list;
    int	nvnodes, vnsize, i, j;
    memset(zerovn, 0, SIZEOF_LARGEDISKVNODE);
    
    if (argc < 3) {
	fprintf(stderr, "Usage: show free <large> | <small>\n");
	return;
    }

    if (strncasecmp("small", argv[2], (int)strlen(argv[2])) == 0) {
	free_list = SRV_RVM(SmallVnodeFreeList);
	nvnodes = SRV_RVM(SmallVnodeIndex);
	vnsize = SIZEOF_SMALLDISKVNODE;
	printf("    There are %d small vnodes in the free list\n", nvnodes);
    } else if (strncasecmp("large", argv[2], (int)strlen(argv[2])) == 0) {
	free_list = SRV_RVM(LargeVnodeFreeList);
	nvnodes = SRV_RVM(LargeVnodeIndex);
	vnsize = SIZEOF_LARGEDISKVNODE;
	printf("    There are %d large vnodes in the free list\n", nvnodes);	
    } else {
	fprintf(stderr, "Usage: show free <large> | <small> [-clear]\n");
	return;
    }

    for (i = 0; i < nvnodes; i++) {
	if (!free_list[i]) {
	    printf("---------------------------\n");
	    printf("NULL entry at index %d\n", i);
	    continue;
	}
	if (memcmp(free_list[i], zerovn, vnsize) != 0) {
	    printf("---------------------------\n");
	    printf("Non-zero vnode object at index %d\n", i);
	    PrintVnodeDiskObject(free_list[i]);
#if 0
    /* This code can clear non-zero VNodes in the freelist. However...
     * If the vnode is possibly used by an existing volume this will
     * corrupt the volume and it would have been better to unlink the
     * vnode from the freelist (and allocate a new empty one to fill the
     * spot. So it is only usable in the case where we have an obvious
     * singly bit or byte corruption.
     *
     * i.e. This is dangerous juju! --JH
     */
	    if (clear) {
		rvm_return_t status;
		printf("\tclearing entry!!!\n");
		rvmlib_begin_transaction(restore);
		rvmlib_modify_bytes(free_list[i], zerovn, vnsize);
		rvmlib_end_transaction(flush, &status);
	    }
#endif
	}
	for (j = i+1; j < nvnodes; j++) {
	    if (free_list[i] == free_list[j]) {
		printf("---------------------------\n");
		printf("Vnode object at index %d is also in the freelist at "
		       "index %d\n", i, j);
	    }
	}
    }
}


// remove name from the given directory and mark its vnode in conflict
// if flag not null, decrease linkCount of directory vnode
static void 
setcount(int volid, int vnum, int unique, int count) 
{

    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    Error   error;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    int     vclass = vnodeIdToClass(vnum);
    int	    volindex;
    rvm_return_t status;
    
    volindex = GetVolIndex(volid);
    if (volindex < 0) {
	    fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	    return;
    }

    rvmlib_begin_transaction(restore);
	    
    if (ExtractVnode(&error, volindex, vclass, vnodeindex, unique, vnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x 0x%x 0x%x\n", volid, vnum,
		unique);
	rvmlib_abort(VFAIL);
	return;
    }

    vnode->linkCount = count;

    error = ReplaceVnode(volindex, vclass, vnodeindex, unique, vnode);
    if (error) {
	fprintf(stderr, "ERROR: ReplaceVnode returns %d, aborting\n", error);
	rvmlib_abort(VFAIL);
	return;
    }
	    
    rvmlib_end_transaction(flush, &status);

    if (status) {
	fprintf(stderr, "ERROR: Transaction aborted with status %d\n", error);
    }
}


void set_linkcount(int argc, char *argv[]) {

    unsigned int volid, vnode, unique, count;

    if ((argc != 6) ||
	(Parser_uint(argv[2], &volid) != 1) ||
	(Parser_uint(argv[3], &vnode) != 1) ||
	(Parser_uint(argv[4], &unique) != 1) ||
	(Parser_uint(argv[5], &count) != 1)) {
	fprintf(stderr, "Usage: set linkcount <parent_volid> ");
	fprintf(stderr, "<parent_vnode> <parent_unique> <count>\n"); 
	return;
    }

    if ( count < 0 ) {
	    fprintf(stderr, "count must be positive!\n");
	    return ;
    }

    setcount(volid, vnode, unique, count);
}



#if 0
// delete the RVM held vnode
static void 
delete_smallvnode(int volid, int vnum, int unique)
{
    char buf[SIZEOF_SMALLDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    Error   error;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    int     vclass = vnodeIdToClass(vnum);
    int	    volindex;
    
    volindex = GetVolIndex(volid);
    if (volindex < 0) {
	fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	return;
    }

    rvmlib_begin_transaction(restore)
	    
    if (ExtractVnode(&error, volindex, vclass, vnodeindex, unique, vnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, vnum,
		unique);
	rvmlib_abort(VFAIL);
	return;
    }


    if (error = ReplaceVnode(volindex, vclass, vnodeindex, unique, vnode)) {
	fprintf(stderr, "ERROR: ReplaceVnode returns %d, aborting\n", error);
	rvmlib_abort(VFAIL);
	return;
    }
	    
    rvmlib_end_transaction(flush, &(error));

    if (error) {
	fprintf(stderr, "ERROR: Transaction aborted with status %d\n",
		error);
    }
}
#endif
