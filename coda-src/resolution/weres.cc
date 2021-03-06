/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2021 Carnegie Mellon University
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <rpc2/rpc2.h>
#include <util.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif

#include <vcrcommon.h>
#include <volume.h>
#include <srv.h>
#include <vrdb.h>
#include <volume.h>
#include <inconsist.h>
#include <util.h>
#include <rescomm.h>
#include <resutil.h>
#include <res.h>
#include <lockqueue.h>

/* subordinate end of Weakly Equal resolution -
 *	each subordinate forces a  new vector passed to it
 */
long RS_ForceVV(RPC2_Handle RPCid, ViceFid *Fid, ViceVersionVector *VV,
                ViceStatus *statusp) EXCLUDES_TRANSACTION
{
    VV_Cmp_Result res;
    Vnode *vptr         = 0;
    Volume *volptr      = 0;
    VolumeId VSGVolnum  = Fid->Volume;
    rvm_return_t status = RVM_SUCCESS;
    int errorcode       = 0;
    conninfo *cip       = GetConnectionInfo(RPCid);

    if (cip == NULL) {
        SLog(0, "RS_ForceVV: Couldnt get conninfo");
        return (EINVAL);
    }

    if (!XlateVid(&Fid->Volume)) {
        SLog(0, "RS_ForceVV: Couldnt Xlate VSG %x", Fid->Volume);
        return (EINVAL);
    }

    /* get the object */
    errorcode = GetFsObj(Fid, &volptr, &vptr, WRITE_LOCK, NO_LOCK, 0, 0, 0);
    if (errorcode) {
        SLog(0, "RS_ForceVV: GetFsObj returns error %d for %s", errorcode,
             FID_(Fid));
        errorcode = EINVAL;
        goto FreeLocks;
    }

    /* make sure volume is locked by coordinator */
    if (V_VolLock(volptr).IPAddress != cip->GetRemoteHost()) {
        SLog(0, "RS_ForceVV: Volume of %s not locked by coordinator",
             FID_(Fid));
        errorcode = EWOULDBLOCK;
        goto FreeLocks;
    }

    SLog(9, "ForceVV: vector passed in is :");
    if (SrvDebugLevel >= 9)
        FPrintVV(stdout, VV);
    SLog(9, "ForceVV: vector in the vnode is :");
    if (SrvDebugLevel >= 9)
        FPrintVV(stdout, &Vnode_vv(vptr));

    /* check that the new version vector is >=  old vv */
    res = VV_Cmp(&Vnode_vv(vptr), VV);

    if (res == VV_DOM || res == VV_INC) {
        errorcode = EINCOMPATIBLE;
        SLog(0, "RS_ForceVV: Version Vectors are inconsistent");
        SLog(0, "RS_ForceVV: Vectors are: ");
        FPrintVV(stdout, VV);
        FPrintVV(stdout, &Vnode_vv(vptr));
        goto FreeLocks;
    }

    if (res == VV_EQ) {
        SLog(0, "RS_ForceVV: Forcing the old version vector on %s.", FID_(Fid));
    } else {
        CODA_ASSERT(res == VV_SUB);
        ViceVersionVector DiffVV = *VV;
        SubVVs(&DiffVV, &Vnode_vv(vptr));
        AddVVs(&Vnode_vv(vptr), &DiffVV);
        AddVVs(&V_versionvector(volptr), &DiffVV);
        CodaBreakCallBack(0, Fid, VSGVolnum);
    }

    /* if cop pending flag is set for this vnode, then clear it */
    if (COP2Pending(Vnode_vv(vptr))) {
        SLog(9, "ForceVV: Clearing COP2 pending flag");
        ClearCOP2Pending(Vnode_vv(vptr));
    }

    if (statusp && statusp->Date) {
        // need to set the owner/author/date/modebits
        // for now due to the rp2gen bug (can\'t pass NULL pointers for IN parameters)
        // make sure Date is nonzero to check that the status is valid
        vptr->disk.modeBits       = statusp->Mode;
        vptr->disk.author         = statusp->Author;
        vptr->disk.owner          = statusp->Owner;
        vptr->disk.unixModifyTime = statusp->Date;
    }

FreeLocks:
    rvmlib_begin_transaction(restore);
    /* release lock on vnode and put the volume */
    if (vptr) {
        Error filecode = 0;
        VPutVnode(&filecode, vptr);
        CODA_ASSERT(filecode == 0);
    }
    PutVolObj(&volptr, NO_LOCK);
    rvmlib_end_transaction(flush, &(status));
    SLog(9, "RS_ForceVV returns %d", errorcode);
    return (errorcode);
}
