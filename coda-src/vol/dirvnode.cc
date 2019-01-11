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

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <util.h>
#include <rvmlib.h>
#include <codadir.h>
#include <vice.h>
#ifdef __cplusplus
}
#endif

#include "cvnode.h"
#include "volume.h"

/* 
   This file manages the directory handle cache in conjunction 
   with Vnodes.

   The Directory Handle cache is controlled by the Vnode cache. 
   When directory content is needed, VN_SetDirHandle must be called. 
   When the Vnode goes away is calls DC_Drop to eliminate the cache.
   It could call DC_Put but presently the calls VN_SetDirHandle is not
   paired with a corresponding VN_PutDirHandle call. 

   The reference counting done by DC is therefore ignored:
   i.e. VN_SetDirHandle can be called at any time and need not te
   matched by equally many "Puts", when VN_PutDirHandle is called the
   cache entry will go away.

   This would be a good thing to fix, since it would potentially enhance 
   performance.

*/

/* copies DirHandle data  into pages in recoverable storage */
/* Called from within a transaction */
int VN_DCommit(Vnode *vnp)
{
    PDirInode pdi = vnp->disk.node.dirNode;
    PDCEntry pdce = vnp->dh;

    if (!vnp || (vnp->disk.type != vDirectory) || !pdce) {
        DLog(29, "VN_DCommit: Vnode or dh not allocated/not a directory");
        return 0;
    }

    if (vnp->delete_me) {
        /* directory was deleted */
        DLog(29, "VN_DCommit: deleted directory, vnode = %x", vnp->vnodeNumber);
        vnp->disk.node.dirNode = NULL;
        /* if this vnode was just cloned, there won't be a pdi upon removal */
        if (pdi)
            DI_Dec(pdi);
    } else if (vnp->changed) {
        /* directory was modified - commit the pages */
        DLog(29, "VN_DCommit: Commiting pages for dir vnode = %x",
             vnp->vnodeNumber);
        /* copy the VM pages into RVM */
        DI_DhToDi(pdce);
        /* CODA_ASSERT the directory inode now exists... */
        CODA_ASSERT(DC_DC2DI(pdce));
        /* rehash just in case it is new */
        DC_Rehash(pdce);
        vnp->disk.node.dirNode = DC_DC2DI(pdce);
    }
    return 0;
}

/* Eliminate the VM Data of the directory */
int VN_DAbort(Vnode *vnp)
{
    if (!vnp || (vnp->disk.type != vDirectory) || !vnp->dh) {
        DLog(29, "DAbort: Vnode not allocated, not a directory or no handle");
        return (0);
    }

    DH_FreeData(DC_DC2DH(vnp->dh));
    DC_SetDirty(vnp->dh, 0);

    /* if this was a new directory, clean up further */
    if (!vnp->disk.node.dirNode) {
        vnp->dh_refc = 0;
        DC_Drop(vnp->dh);
        vnp->dh = NULL;
    }

    return (0);
}

/* 
   this hashes the Directory Handle and copies inode pages into the DH. 
   dc_count exceeds the vn->dh_refc if a vnode clone exists in VM.
*/
PDirHandle VN_SetDirHandle(struct Vnode *vn)
{
    PDCEntry pdce = NULL;

    /* three cases:
       - new not previously seen: node.dirNode=0, dh=0
       - new, not yet in RVM: node.dirNode = 0, dh!=0
       - not new, already in RVM: node.dirNode != 0
     */

    if (!vn->disk.node.dirNode && !vn->dh) {
        pdce        = DC_New();
        vn->dh      = pdce;
        vn->dh_refc = 1;
        SLog(5, "VN_GetDirHandle NEW %x.%x: cnt %d\n", vn->vnodeNumber,
             vn->disk.uniquifier, DC_Count(pdce));
    } else if (vn->disk.node.dirNode) {
        pdce   = DC_Get(vn->disk.node.dirNode);
        vn->dh = pdce;
        vn->dh_refc++;
        SLog(5, "VN_GetDirHandle for %x.%x: cnt %d, vn_cnt %d\n",
             vn->vnodeNumber, vn->disk.uniquifier, DC_Count(pdce), vn->dh_refc);
    } else {
        pdce = vn->dh;
        DC_SetCount(pdce, DC_Count(pdce) + 1);
        vn->dh_refc++;
        SLog(5, "VN_GetDirHandle NEW-seen %x.%x: cnt %d, vn_ct %d\n",
             vn->vnodeNumber, vn->disk.uniquifier, DC_Count(pdce), vn->dh_refc);
    }

    CODA_ASSERT((vn->dh_refc > 0) && (DC_Count(pdce) >= vn->dh_refc));
    return DC_DC2DH(pdce);
}

/*
  VN_PutDirHandle: the Vnode is going away, clear the DC entry
 */
void VN_PutDirHandle(struct Vnode *vn)
{
    CODA_ASSERT(vn->dh);

    if (vn->dh) {
        SLog(5, "VN_PutDirHandle: %x.%x: cnt %d, vn_cnt %d\n", vn->vnodeNumber,
             vn->disk.uniquifier, DC_Count(vn->dh) - 1, vn->dh_refc - 1);
        DC_Put(vn->dh);
        vn->dh_refc--;
        CODA_ASSERT(DC_Count(vn->dh) >= 0);
        if (vn->dh_refc == 0)
            vn->dh = 0;
    }
}

/* Drop DirHandle */
void VN_DropDirHandle(struct Vnode *vn)
{
    if (vn->dh) {
        SLog(5, "VN_DropDirHandle for %x.%x: cnt %d, vn_cnt %d\n",
             vn->vnodeNumber, vn->disk.uniquifier, DC_Count(vn->dh),
             vn->dh_refc);
        DC_Drop(vn->dh);
    }
    vn->dh_refc = 0;
    vn->dh      = NULL;
}

/*
   - directories: set the disk.inode field to 0 and 
     create a dcentry with the _old_ contents. 
   NOTES:
   - afterwards the vptr->dh  will have VM data, 
     but no RVM data.
   - the reference counts of the vnode and the cache entries
     are split appropriately.

*/
void VN_CopyOnWrite(struct Vnode *vn)
{
    PDCEntry pdce;
    PDCEntry oldpdce;
    PDirHeader pdirh;
    PDirHandle pdh;
    int others_count;

    CODA_ASSERT(vn->disk.node.dirNode);
    /* pin it */
    pdh = VN_SetDirHandle(vn);
    CODA_ASSERT(pdh);
    oldpdce      = DC_DH2DC(pdh);
    others_count = DC_Count(oldpdce) - vn->dh_refc;

    /* no one else has a reference to this directory -
       merely prepare for cloning the RVM directory inode */
    if (!others_count) {
        DC_SetCowpdi(oldpdce, vn->disk.node.dirNode);
        DC_SetRefcount(oldpdce, 1);
        DC_SetDirty(oldpdce, 1);
    }

    /* other vnodes have a reference to this directory -
       prepare for cloning the RVM directory inode _and_
       clone the VM directory cache entry now */
    if (others_count) {
        pdce = DC_New();
        CODA_ASSERT(pdce);

        /* get pointer to VM data */
        pdirh = (PDirHeader)malloc(DH_Length(pdh));
        CODA_ASSERT(pdirh);
        memcpy(pdirh, DH_Data(pdh), DH_Length(pdh));

        /* set up the copied directory */
        vn->dh = pdce;
        DC_SetDirh(pdce, pdirh);
        DC_SetCowpdi(pdce, vn->disk.node.dirNode);
        DC_SetDirty(pdce, 1);
        DC_SetCount(pdce, vn->dh_refc);

        /* subtract our references to the oldpdce */
        DC_SetCount(oldpdce, others_count);
    }

    vn->disk.node.dirNode = NULL;
    vn->disk.cloned       = 0;
    VN_PutDirHandle(vn);

    SLog(5, "VN_CopyOnWrite: New other_count: %d dh_refc %d", others_count,
         vn->dh_refc);
}
