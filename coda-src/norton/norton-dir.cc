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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/norton/norton-dir.cc,v 4.8 1998/10/08 17:23:05 rvb Exp $";
#endif /*_BLURB_*/




#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>    
#include <codadir.h>

#include <netinet/in.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>
#include <volutil.private.h>
#include <index.h>
#include <recov.h>
#include <camprivate.h>
#include <coda_globals.h>
#include <codadir.h>

#include <parser.h>
#include "norton.h"


struct NortonDirHandle {
	DirHandle   nor_dirh;
	VolumeId    nor_vol;
};


static int printentry(struct DirEntry *de, void *hook)
{
	int vnodenumber = ntohl(de->fid.dnf_vnode);
	int unique = ntohl(de->fid.dnf_unique);
	
	printf("    (0x%x 0x%x)\t%s\n", vnodenumber, unique, de->name);
	return 0;
}

PDCEntry SetDirHandle(int volid, int vnum, int unique) 
{ 
	PDCEntry dc;
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

    dc = DC_Get((PDirInode) vnode->inodeNumber);
    

    return dc;
}



void show_dir(int argc, char *argv[]) {
    int volid,
	vnode,
	unique;


    if ((argc != 5) ||
	(Parser_int(argv[2], &volid) != 1) ||
	(Parser_int(argv[3], &vnode) != 1) ||
	(Parser_int(argv[4], &unique) != 1)) {
	fprintf(stderr, "Usage: show directory <volid> <vnode> <unique> \n");
	return;
    }

    show_dir(volid, vnode, unique);
}


void 
show_dir(int volid, int vnum, int unique) 
{
    PDCEntry dc;
    int     vclass = vnodeIdToClass(vnum);

    if ( vclass != vLarge ) {
	printf("Not a directory fid!\n");
	return;
    }

    dc= SetDirHandle(volid, vnum, unique);
    if (!dc) {
	    return;
    }
    
    if (!DH_DirOK(DC_DC2DH(dc))) {
	fprintf (stderr, "WARNING: Bad Dir(0x%#08x.0x%x.0x%x)\n",
		 volid, vnum, unique);
    }

    DH_EnumerateDir(DC_DC2DH(dc), printentry, NULL);
}



// remove name from the given directory and mark its vnode in conflict
// if flag not null, decrease linkCount of directory vnode
static void 
delete_name(int volid, int vnum, int unique, char *name, int flag) 
{

    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    PDirHandle pdh;
    PDCEntry dc;
    PDirInode pdi;
    struct ViceFid fid;
    Error   error;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    int     vclass = vnodeIdToClass(vnum);
    int	    volindex;
    
    volindex = GetVolIndex(volid);
    if (volindex < 0) {
	    fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	    return;
    }

    if (vclass != vLarge) {
	    fprintf(stderr, "Not a directory (i.e. large) vnode.\n");
    }

    dc = SetDirHandle(volid, vnum, unique);
    if (!dc) {
	    printf("Cannot get handle for directory\n");
	    return;
    }
    pdh = DC_DC2DH(dc);
    assert(pdh);

    RVMLIB_BEGIN_TRANSACTION(restore)
	    
    if (ExtractVnode(&error, volindex, vclass, (VnodeId)vnodeindex,
		     (Unique_t)unique, vnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x 0x%x 0x%x\n", volid, vnum,
		unique);
	rvmlib_abort(VFAIL);
	return;
    }

    error = DH_Delete(pdh, name);
    if (error) {
	fprintf(stderr, "ERROR: DH_Delete() returns %d, aborting\n", error);
	rvmlib_abort(VFAIL);
	return;
    }

    if ( ! DH_DirOK(pdh) ) {
	    printf("WARNING: directory not OK!\n");
    }

    if ( flag ) {
	vnode->linkCount--;
    }

    // ignore changing the length for now
    pdi = DI_DhToDi(dc);
    
    if ( pdi != (PDirInode) vnode->inodeNumber ) {
	    printf("WARNING: directory inode has changed!\n");
	    vnode->inodeNumber = (long unsigned int)pdi;
    }

    // mark the vnode with inconsistent flag
    SetIncon(vnode->versionvector);

    if (error = ReplaceVnode(volindex, vclass, (VnodeId)vnodeindex,
			     (Unique_t)unique, vnode)) {
	fprintf(stderr, "ERROR: ReplaceVnode returns %d, aborting\n", error);
	rvmlib_abort(VFAIL);
	return;
    }
	    
    RVMLIB_END_TRANSACTION(flush, &error);

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
	(Parser_int(argv[2], &volid) != 1) ||
	(Parser_int(argv[3], &vnode) != 1) ||
	(Parser_int(argv[4], &unique) != 1) ||
	(Parser_int(argv[6], &flag) != 1)) {
	fprintf(stderr, "Usage: delete name <parent_volid> ");
	fprintf(stderr, "<parent_vnode> <parent_unique> <name> <flag>\n"); 
	return;
    }

    delete_name(volid, vnode, unique, argv[5], flag);
}

static void 
create_name(int volid, int vnum, int unique, char *name, int cvnum, 
	    int cunique) 
{
    /* remove name from the given directory and mark its vnode in conflict */
    char buf[SIZEOF_LARGEDISKVNODE];
    char cbuf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    struct VnodeDiskObject *cvnode = (struct VnodeDiskObject *)cbuf;
    PDirHandle pdh;
    PDCEntry dc;
    PDirInode pdi;
    struct ViceFid vfid;
    Error   error;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    VnodeId cvnodeindex = vnodeIdToBitNumber(cvnum);
    int     vclass = vnodeIdToClass(vnum);
    int     cvclass = vnodeIdToClass(cvnum);
    int	    volindex;
    
    volindex = GetVolIndex(volid);
    if (volindex < 0) {
	fprintf(stderr, "Unable to get volume 0x%x\n", volid);
	return;
    }

    if (vclass != vLarge) {
	fprintf(stderr, "Not a directory (i.e. large) vnode.\n");
    }

    dc = SetDirHandle(volid, vnum, unique);
    if (!dc) {
	    printf("Cannot get handle for directory\n");
	    return;
    }
    pdh = DC_DC2DH(dc);
    assert(pdh);    

    RVMLIB_BEGIN_TRANSACTION(restore)
	    
    if (ExtractVnode(&error, volindex, vclass, (VnodeId)vnodeindex,
		     (Unique_t)unique, vnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, vnum,
		unique);
	rvmlib_abort(VFAIL);
	return;
    }

    if (ExtractVnode(&error, volindex, cvclass, (VnodeId)cvnodeindex,
		     (Unique_t)cunique, cvnode) < 0) {
	fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, cvnum,
		cunique);
	rvmlib_abort(VFAIL);
	return;
    }


    vfid.Vnode = cvnum;
    vfid.Unique = cunique;
    error = DH_Create(pdh, name, &vfid);
    if (error) {
	fprintf(stderr, "ERROR: Create() returns %d, aborting\n", error);
	rvmlib_abort(VFAIL);
	return;
    }

    if ( ! DH_DirOK(pdh) ) {
	    printf("WARNING: directory not OK!\n");
    }

    // if child is directory increase linkCount of parent
    if ( cvclass == vLarge ) {
	vnode->linkCount++;
    }

    // adjust parent of child
    cvnode->vparent = vnum;
    cvnode->uparent = unique;
    
    pdi = DI_DhToDi(dc);
    
    if ( pdi != (PDirInode) vnode->inodeNumber ) {
	    printf("WARNING: directory inode has changed!\n");
	    vnode->inodeNumber = (long unsigned int)pdi;
    }



    // mark the vnode with inconsistent flag
    SetIncon(vnode->versionvector);

    if (error = ReplaceVnode(volindex, vclass, (VnodeId)vnodeindex,
			     (Unique_t)unique, vnode)) {
	fprintf(stderr, "ReplaceVnode returns %d, for parent, aborting\n", 
		error);
	rvmlib_abort(VFAIL);
	return;
    }

    if (error = ReplaceVnode(volindex, cvclass, (VnodeId)cvnodeindex,
			     (Unique_t)cunique, cvnode)) {
	fprintf(stderr, "ReplaceVnode returns %d, for child, abort\n", 
		error);
	rvmlib_abort(VFAIL);
	return;
    }
	    
    RVMLIB_END_TRANSACTION(flush, &(error));

    if (error) {
	fprintf(stderr, "ERROR: Transaction aborted with status %d\n",
		error);
    }
}

void sh_create_name(int argc, char **argv) {
    int volid, vnode, unique;
    int cvnode, cunique;
    

    if ((argc != 8) ||
	(Parser_int(argv[2], &volid) != 1) ||
	(Parser_int(argv[3], &vnode) != 1) ||
	(Parser_int(argv[4], &unique) != 1) ||
	(Parser_int(argv[6], &cvnode) != 1) ||
	(Parser_int(argv[7], &cunique) != 1))  {
	fprintf(stderr, "Usage: create name <parent_volid> ");
	fprintf(stderr, "<parent_vnode> <parent_unique> <name>");
	fprintf(stderr, " <newvnode> <newunique>\n"); 
	return;
    }
    create_name(volid, vnode, unique, argv[5], cvnode, cunique);

}
