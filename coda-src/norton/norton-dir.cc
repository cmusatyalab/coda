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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <codadir.h>

#include <netinet/in.h>

#ifdef __cplusplus
}
#endif

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
    DirHandle nor_dirh;
    VolumeId nor_vol;
};

static int testVnodeExists(VolumeId volid, VnodeId vnum, Unique_t unique)
{
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    VnodeId vnodeindex     = vnodeIdToBitNumber(vnum);
    int vclass             = vnodeIdToClass(vnum);
    int volindex;

    volindex = GetVolIndex(volid);
    if (volindex < 0) {
        return 0;
    }

    if (ExtractVnode(volindex, vclass, vnodeindex, unique, vnode) < 0)
        return 0;

    return 1;
}

static int printentry(struct DirEntry *de, void *hook)
{
    int vnodenumber = ntohl(de->fid.dnf_vnode);
    int unique      = ntohl(de->fid.dnf_unique);
    int exists      = 0;
    int volid       = *(int *)hook;

    exists = testVnodeExists(volid, vnodenumber, unique);

    printf("  %c (0x%x 0x%x)\t%s\n", exists ? ' ' : '?', vnodenumber, unique,
           de->name);
    return 0;
}

PDCEntry SetDirHandle(int volid, int vnum, int unique)
{
    PDCEntry dc;
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    VolHead *vol;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    int vclass         = vnodeIdToClass(vnum);
    int volindex;
    struct stat status;

    CODA_ASSERT(vclass == vLarge);

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

    if (ExtractVnode(volindex, vclass, vnodeindex, unique, vnode) < 0) {
        fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, vnum,
                unique);
        return 0;
    }

    dc = DC_Get(vnode->node.dirNode);

    return dc;
}

void show_dir(VolumeId volid, VnodeId vnum, Unique_t unique)
{
    PDCEntry dc;
    int vclass = vnodeIdToClass(vnum);

    if (vclass != vLarge) {
        printf("Not a directory fid!\n");
        return;
    }

    dc = SetDirHandle(volid, vnum, unique);
    if (!dc) {
        return;
    }

    if (!DH_DirOK(DC_DC2DH(dc))) {
        fprintf(stderr, "WARNING: Bad Dir(0x%#08x.0x%x.0x%x)\n", volid, vnum,
                unique);
    }

    DH_EnumerateDir(DC_DC2DH(dc), printentry, &volid);
}

void show_dir(int argc, char *argv[])
{
    unsigned int volid, vnode, unique;

    if ((argc != 5) || (Parser_uint(argv[2], &volid) != 1) ||
        (Parser_uint(argv[3], &vnode) != 1) ||
        (Parser_uint(argv[4], &unique) != 1)) {
        fprintf(stderr, "Usage: show directory <volid> <vnode> <unique> \n");
        return;
    }

    show_dir((VolumeId)volid, (VnodeId)vnode, (Unique_t)unique);
}

// remove name from the given directory and mark its vnode in conflict
// if flag not null, decrease linkCount of directory vnode
static void delete_name(VolumeId volid, VnodeId vnum, Unique_t unique,
                        char *name, int flag)
{
    char buf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode = (VnodeDiskObject *)buf;
    PDirHandle pdh;
    PDCEntry dc;
    PDirInode pdi;
    rvm_return_t status;
    VnodeId vnodeindex = vnodeIdToBitNumber(vnum);
    int vclass         = vnodeIdToClass(vnum);
    int volindex;
    int error;

    CODA_ASSERT(vclass == vLarge);

    volindex = GetVolIndex(volid);
    if (volindex < 0) {
        fprintf(stderr, "Unable to get volume 0x%x\n", volid);
        return;
    }

    dc = SetDirHandle(volid, vnum, unique);
    if (!dc) {
        printf("Cannot get handle for directory\n");
        return;
    }
    pdh = DC_DC2DH(dc);
    CODA_ASSERT(pdh);

    rvmlib_begin_transaction(restore);

    if (ExtractVnode(volindex, vclass, vnodeindex, unique, vnode) < 0) {
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

    if (!DH_DirOK(pdh)) {
        printf("WARNING: directory not OK!\n");
    }

    if (flag) {
        vnode->linkCount--;
    }

    // ignore changing the length for now
    DI_DhToDi(dc);
    pdi = DC_DC2DI(dc);

    if (pdi != vnode->node.dirNode) {
        printf("WARNING: directory inode has changed!\n");
        vnode->node.dirNode = pdi;
    }

    // mark the vnode with inconsistent flag
    SetIncon(vnode->versionvector);

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

void delete_name(int argc, char *argv[])
{
    unsigned int volid, vnode, unique, flag = 0;

    if ((argc != 7) || (Parser_uint(argv[2], &volid) != 1) ||
        (Parser_uint(argv[3], &vnode) != 1) ||
        (Parser_uint(argv[4], &unique) != 1) ||
        (Parser_uint(argv[6], &flag) != 1)) {
        fprintf(stderr, "Usage: delete name <parent_volid> ");
        fprintf(stderr, "<parent_vnode> <parent_unique> <name> <flag>\n");
        return;
    }

    delete_name(volid, vnode, unique, argv[5], flag);
}

static void create_name(VolumeId volid, VnodeId vnum, Unique_t unique,
                        char *name, VnodeId cvnum, Unique_t cunique)
{
    /* remove name from the given directory and mark its vnode in conflict */
    char buf[SIZEOF_LARGEDISKVNODE];
    char cbuf[SIZEOF_LARGEDISKVNODE];
    VnodeDiskObject *vnode  = (VnodeDiskObject *)buf;
    VnodeDiskObject *cvnode = (VnodeDiskObject *)cbuf;
    PDirHandle pdh;
    PDCEntry dc;
    PDirInode pdi;
    struct ViceFid vfid;
    rvm_return_t status;
    VnodeId vnodeindex  = vnodeIdToBitNumber(vnum);
    VnodeId cvnodeindex = vnodeIdToBitNumber(cvnum);
    int vclass          = vnodeIdToClass(vnum);
    int cvclass         = vnodeIdToClass(cvnum);
    int volindex;
    int error;

    CODA_ASSERT(vclass == vLarge);

    volindex = GetVolIndex(volid);
    if (volindex < 0) {
        fprintf(stderr, "Unable to get volume 0x%x\n", volid);
        return;
    }

    dc = SetDirHandle(volid, vnum, unique);
    if (!dc) {
        printf("Cannot get handle for directory\n");
        return;
    }
    pdh = DC_DC2DH(dc);
    CODA_ASSERT(pdh);

    rvmlib_begin_transaction(restore);

    if (ExtractVnode(volindex, vclass, vnodeindex, unique, vnode) < 0) {
        fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, vnum,
                unique);
        rvmlib_abort(VFAIL);
        return;
    }

    if (ExtractVnode(volindex, cvclass, cvnodeindex, cunique, cvnode) < 0) {
        fprintf(stderr, "Unable to get vnode 0x%x.0x%x.0x%x\n", volid, cvnum,
                cunique);
        rvmlib_abort(VFAIL);
        return;
    }

    vfid.Vnode  = cvnum;
    vfid.Unique = cunique;
    error       = DH_Create(pdh, name, &vfid);
    if (error) {
        fprintf(stderr, "ERROR: Create() returns %d, aborting\n", error);
        rvmlib_abort(VFAIL);
        return;
    }

    if (!DH_DirOK(pdh)) {
        printf("WARNING: directory not OK!\n");
    }

    // if child is directory increase linkCount of parent
    if (cvclass == vLarge) {
        vnode->linkCount++;
    }

    // adjust parent of child
    cvnode->vparent = vnum;
    cvnode->uparent = unique;

    DI_DhToDi(dc);
    pdi = DC_DC2DI(dc);

    if (pdi != vnode->node.dirNode) {
        printf("WARNING: directory inode has changed!\n");
        vnode->node.dirNode = pdi;
    }

    // mark the vnode with inconsistent flag
    SetIncon(vnode->versionvector);

    error = ReplaceVnode(volindex, vclass, vnodeindex, unique, vnode);
    if (error) {
        fprintf(stderr, "ReplaceVnode returns %d, for parent, aborting\n",
                error);
        rvmlib_abort(VFAIL);
        return;
    }

    error = ReplaceVnode(volindex, cvclass, cvnodeindex, cunique, cvnode);
    if (error) {
        fprintf(stderr, "ReplaceVnode returns %d, for child, abort\n", error);
        rvmlib_abort(VFAIL);
        return;
    }

    rvmlib_end_transaction(flush, &(status));

    if (status) {
        fprintf(stderr, "ERROR: Transaction aborted with status %d\n", status);
    }
}

void sh_create_name(int argc, char **argv)
{
    unsigned int volid, vnode, unique;
    unsigned int cvnode, cunique;

    if ((argc != 8) || (Parser_uint(argv[2], &volid) != 1) ||
        (Parser_uint(argv[3], &vnode) != 1) ||
        (Parser_uint(argv[4], &unique) != 1) ||
        (Parser_uint(argv[6], &cvnode) != 1) ||
        (Parser_uint(argv[7], &cunique) != 1)) {
        fprintf(stderr, "Usage: create name <parent_volid> ");
        fprintf(stderr, "<parent_vnode> <parent_unique> <name>");
        fprintf(stderr, " <newvnode> <newunique>\n");
        return;
    }
    create_name(volid, vnode, unique, argv[5], cvnode, cunique);
}
