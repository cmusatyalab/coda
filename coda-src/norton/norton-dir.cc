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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/norton/norton-dir.cc,v 4.2 1997/10/15 15:53:01 braam Exp $";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <strings.h>
#include <errno.h>
#ifdef	__MACH__
#include <mach/boolean.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>    
#ifdef __cplusplus
}
#endif __cplusplus

#include <coda_dir.h>
#include <cvnode.h>
#include <volume.h>
#include <volutil.private.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>
#include <rvmdir.h>

#include "parser.h"
#include "norton.h"

PRIVATE void printentry(struct DirSummary *dir, char *name, VnodeId
			vnodeNumber, Unique_t unique) {
    printf("    (0x%x 0x%x)\t%s\n", vnodeNumber, unique, name);
}

PRIVATE int SetDirHandle(DirHandle *dir, int volid, int vnum, int unique) { 
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    VolHead *vol;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    int     vclass = vnodeIdToClass(vnum);
    int	    volindex;
    Error   error;
    struct stat status;
    
    volindex = GetVolIndex(volid);
    if (volindex < 0) {
	fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	return 0;
    }

    vol = GetVol(volid);
    if (!vol) {
	fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	return 0;
    }

    if (stat(vol->data.volumeInfo->partition, &status) == -1) {
	char buf[80];

	sprintf(buf, "Getting status of %s", vol->data.volumeInfo->partition);
	perror(buf);
    }

    if (ExtractVnode(&error, volindex, vclass, (VnodeId)vnodeindex,
		     (Unique_t)unique, vnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, vnum,
		unique);
	return 0;
    }


    SetSalvageDirHandle(dir, volid, status.st_dev, (int)vnode->inodeNumber);
    dir->vnode = vnum;
    dir->unique = unique;

    return(1);
}



void show_dir(int argc, char *argv[]) {
    int volid,
	vnode,
	unique;


    if ((argc != 5) ||
	(parse_int(argv[2], &volid) != 1) ||
	(parse_int(argv[3], &vnode) != 1) ||
	(parse_int(argv[4], &unique) != 1)) {
	fprintf(stderr, "Usage: show directory <volid> <vnode> <unique> \n");
	return;
    }

    show_dir(volid, vnode, unique);
}


void 
show_dir(int volid, int vnum, int unique) 
{
    DirHandle dir;
    int     vclass = vnodeIdToClass(vnum);

    if ( vclass != vLarge ) {
	printf("Not a directory fid!\n");
	return;
    }

    if (!SetDirHandle(&dir, volid, vnum, unique)) {
	return;
    }
    
    if (!DirOK((long *)&dir)) {
	fprintf (stderr, "WARNING: Bad Dir(0x%#08x.0x%x.0x%x)\n",
		 volid, vnum, unique);
    }

    EnumerateDir((long *)&dir, (int (*)(void *...))printentry, (long)&dir);
}



// remove name from the given directory and mark its vnode in conflict
// if flag not null, decrease linkCount of directory vnode
PRIVATE void 
delete_name(int volid, int vnum, int unique, char *name, int flag) 
{
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    DirHandle dirh;
    struct VFid fid;
    Error   error;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    int     vclass = vnodeIdToClass(vnum);
    int	    volindex;
    
    volindex = GetVolIndex(volid);
    if (volindex < 0) {
	fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	return;
    }

    if (!SetDirHandle(&dirh, volid, vnum, unique)) {
	return;
    }
    dirh.cacheCheck = 1; // XXX hack 

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	    
    if (ExtractVnode(&error, volindex, vclass, (VnodeId)vnodeindex,
		     (Unique_t)unique, vnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, vnum,
		unique);
	CAMLIB_ABORT(VFAIL);
	return;
    }


    if ((error = Delete((long *)&dirh, name)) != 0) {
	fprintf(stderr, "ERROR: Delete() returns %d, aborting\n", error);
	CAMLIB_ABORT(VFAIL);
	return;
    }
    
    // flush the buffer pages
    DFlush();

    if ( flag ) {
	vnode->linkCount--;
    }

    // ignore changing the length for now
    fid.volume = dirh.volume;
    fid.vnode = dirh.vnode;
    fid.vunique = dirh.unique;
    ICommit(&fid, (long *)vnode->inodeNumber);

    // mark the vnode with inconsistent flag
    SetIncon(vnode->versionvector);

    if (error = ReplaceVnode(volindex, vclass, (VnodeId)vnodeindex,
			     (Unique_t)unique, vnode)) {
	fprintf(stderr, "ERROR: ReplaceVnode returns %d, aborting\n", error);
	CAMLIB_ABORT(VFAIL);
	return;
    }
	    
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, error)

    if (error) {
	fprintf(stderr, "ERROR: Transaction aborted with status %d\n",
		error);
    }
}


void delete_name(int argc, char *argv[]) {
    int volid,
	vnode,
	unique,
	flag = 0;

    if ((argc != 7) ||
	(parse_int(argv[2], &volid) != 1) ||
	(parse_int(argv[3], &vnode) != 1) ||
	(parse_int(argv[4], &unique) != 1) ||
	(parse_int(argv[6], &flag) != 1)) {
	fprintf(stderr, "Usage: delete name <parent_volid> ");
	fprintf(stderr, "<parent_vnode> <parent_unique> <name> <flag>\n"); 
	return;
    }

    delete_name(volid, vnode, unique, argv[5], flag);

}


PRIVATE void 
create_name(int volid, int vnum, int unique, char *name, int cvnum, 
	    int cunique) 
{
    /* remove name from the given directory and mark its vnode in conflict */
    char buf[SIZEOF_LARGEDISKVNODE];
    char cbuf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    struct VnodeDiskObject *cvnode = (struct VnodeDiskObject *)cbuf;
    DirHandle dirh;
    struct VFid fid;
    Error   error;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    VnodeId cvnodeindex = vnodeIdToBitNumber(cvnum);
    int     vclass = vnodeIdToClass(vnum);
    int     cvclass = vnodeIdToClass(cvnum);
    int	    volindex;
    long    vfid[2];
    
    volindex = GetVolIndex(volid);
    if (volindex < 0) {
	fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	return;
    }

    if (vclass != vLarge) {
	fprintf(stderr, "Not a directory (i.e. large) vnode.\n");
    }

    if (!SetDirHandle(&dirh, volid, vnum, unique)) {
	return;
    }
    dirh.cacheCheck = 1; // XXX hack 

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	    
    if (ExtractVnode(&error, volindex, vclass, (VnodeId)vnodeindex,
		     (Unique_t)unique, vnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, vnum,
		unique);
	CAMLIB_ABORT(VFAIL);
	return;
    }

    if (ExtractVnode(&error, volindex, cvclass, (VnodeId)cvnodeindex,
		     (Unique_t)cunique, cvnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, cvnum,
		cunique);
	CAMLIB_ABORT(VFAIL);
	return;
    }


    vfid[1] = cvnum;
    vfid[2] = cunique;
    if ((error = Create((long *)&dirh, name, vfid)) != 0) {
	fprintf(stderr, "ERROR: Create() returns %d, aborting\n", error);
	CAMLIB_ABORT(VFAIL);
	return;
    }

    // if child is directory increase linkCount of parent
    if ( cvclass == vLarge ) {
	vnode->linkCount++;
    }

    // adjust parent of child
    cvnode->vparent = vnum;
    cvnode->uparent = unique;
    
    DFlush();
    // 

    // ignore changing the length for now
    fid.volume = dirh.volume;
    fid.vnode = dirh.vnode;
    fid.vunique = dirh.unique;
    ICommit(&fid, (long *)vnode->inodeNumber);

    // mark the vnode with inconsistent flag
    SetIncon(vnode->versionvector);

    if (error = ReplaceVnode(volindex, vclass, (VnodeId)vnodeindex,
			     (Unique_t)unique, vnode)) {
	fprintf(stderr, "ReplaceVnode returns %d, for parent, aborting\n", 
		error);
	CAMLIB_ABORT(VFAIL);
	return;
    }

    if (error = ReplaceVnode(volindex, cvclass, (VnodeId)cvnodeindex,
			     (Unique_t)cunique, cvnode)) {
	fprintf(stderr, "ReplaceVnode returns %d, for child, abort\n", 
		error);
	CAMLIB_ABORT(VFAIL);
	return;
    }
	    
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, error)

    if (error) {
	fprintf(stderr, "ERROR: Transaction aborted with status %d\n",
		error);
    }
}

void sh_create_name(int argc, char **argv) {
    int volid, vnode, unique;
    int cvnode, cunique;
    

    if ((argc != 8) ||
	(parse_int(argv[2], &volid) != 1) ||
	(parse_int(argv[3], &vnode) != 1) ||
	(parse_int(argv[4], &unique) != 1) ||
	(parse_int(argv[6], &cvnode) != 1) ||
	(parse_int(argv[7], &cunique) != 1))  {
	fprintf(stderr, "Usage: create name <parent_volid> ");
	fprintf(stderr, "<parent_vnode> <parent_unique> <name>");
	fprintf(stderr, " <newvnode> <newunique>\n"); 
	return;
    }

    create_name(volid, vnode, unique, argv[5], cvnode, cunique);

}
