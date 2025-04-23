/* BLURB gpl

                           Coda File System
                              Release 8

          Copyright (c) 1987-2025 Carnegie Mellon University
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

#include <unistd.h>
#include <stdlib.h>

#include <stdio.h>
#include <rpc2/rpc2.h>
#include <inodeops.h>
#include <codadir.h>
#include <util.h>

#ifdef __cplusplus
}
#endif

#include <olist.h>
#include <dlist.h>
#include <cvnode.h>
#include <vcrcommon.h>
#include <vlist.h>
#include <srv.h>
#include <vrdb.h>
#include <volume.h>
#include <res.h>
#include <operations.h>
#include <resutil.h>
#include <treeremove.h>
#include <timing.h>
#include <lockqueue.h>
#include "rsle.h"
#include "parselog.h"
#include "compops.h"
#include "ruconflict.h"
#include "ops.h"
#include "rvmrestiming.h"
#include "resstats.h"

#define ISCREATEOP(a)                                         \
    ((a) == RES_Create_OP || (a) == ResolveViceCreate_OP ||   \
     (a) == RES_MakeDir_OP || (a) == ResolveViceMakeDir_OP || \
     (a) == RES_SymLink_OP || (a) == ResolveViceSymLink_OP)

#define ISDELETEOP(a)                                       \
    ((a) == RES_Remove_OP || (a) == ResolveViceRemove_OP || \
     (a) == RES_RemoveDir_OP || (a) == ResolveViceRemoveDir_OP)

// subresphase3.c:
//	Subordinate side of Phase 3 of resolution
//		Log Distribution and Compensation
//		Subordinates parse logs, compute compensating operations
//		and perform the operations
// 		Subs return list of inconsistencies, if any, that arose.

// ********** Private Routines **********
static int FetchLog(RPC2_Handle, char **, int);
static int AddChildToList(dlist *, Volume *, Vnode *, VnodeId, Unique_t,
                          int = 0);
static int GatherFids(dlist *, Vnode *, Volume *, arrlist *);
static int AddRenameChildrenToList(dlist *, Volume *, Vnode *, rsle *);
static int SetPhase3DirStatus(ViceStatus *, ViceFid *, Volume *,
                              dlist *) EXCLUDES_TRANSACTION;
static int GetResObjs(arrlist *, ViceFid *, Volume **,
                      dlist *) EXCLUDES_TRANSACTION;
static int CheckSemPerformRes(arrlist *, Volume *, ViceFid *, dlist *, olist *,
                              dlist *, int *, DirFid *) EXCLUDES_TRANSACTION;
static int CheckRegularCompOp(rsle *, dlist *, vle *, ViceFid *, Volume *,
                              olist *);
static int PerformRegularCompOp(int, rsle *, dlist *, dlist *, olist *,
                                ViceFid *, vle *, Volume *, VolumeId,
                                int *) EXCLUDES_TRANSACTION;
static int CheckValidityResOp(rsle *, int, int, int, int, dlist *, ViceFid *,
                              olist *, conflictstats *, Volume *);
static int PerformResOp(rsle *, dlist *, olist *, vle *, Volume *, VolumeId,
                        int *) EXCLUDES_TRANSACTION;
static void PreProcessCompOps(arrlist *);
static int CmpFidOp(rsle **, rsle **);
static void UpdateStats(ViceFid *, conflictstats *) EXCLUDES_TRANSACTION;

/* XXX remember to take this out */
extern int CmpFid(ViceFid *fa, ViceFid *fb);

const int Yield_rp3GetResObjPeriod           = 8;
const int Yield_rp3GetResObjMask             = Yield_rp3GetResObjPeriod - 1;
const int Yield_rp3CollectFidPeriod          = 256;
const int Yield_rp3CollectFidMask            = Yield_rp3CollectFidPeriod - 1;
const int Yield_rp3CheckSemPerformRes_Period = 8;
const int Yield_rp3CheckSemPerformRes_Mask =
    Yield_rp3CheckSemPerformRes_Period - 1;

long RS_NewShipLogs(RPC2_Handle RPCid, ViceFid *Fid, RPC2_Integer size,
                    RPC2_Integer nentries, ViceStatus *status,
                    RPC2_BoundedBS *piggyinc, DirFid *HintFid,
                    SE_Descriptor *sed) EXCLUDES_TRANSACTION
{
    SLog(1, "RS_ShipLogs: Entering for Fid (%s)\n", FID_(Fid));

    PROBE(tpinfo, RecovSubP3Begin);
    int errorCode       = 0;
    Volume *volptr      = NULL;
    dlist *vlist        = new dlist((CFN)VLECmp);
    dlist *inclist      = new dlist((CFN)CompareIlinkEntry);
    olist *AllLogs      = NULL;
    rsle *AllLogEntries = NULL;
    arrlist *CompOps    = NULL;
    int nblocks         = 0;

    if (HintFid) {
        HintFid->Vnode  = 0;
        HintFid->Unique = 0;
    }

    // fetch log from coordinator
    char *buf = NULL;
    {
        if ((errorCode = FetchLog(RPCid, &buf, (int)size)))
            goto Exit;
        CODA_ASSERT(buf);
    }
    // parse log
    {
        ParseRemoteLogs(buf, (int)size, (int)nentries, &AllLogs,
                        &AllLogEntries);
        CODA_ASSERT(AllLogs && AllLogEntries);
        PollAndYield();
    }
    // Compute Compensating Operations
    {
        PROBE(tpinfo, RecovCompOpsBegin);
        CompOps = ComputeCompOps(AllLogs, Fid);
        PROBE(tpinfo, RecovCompOpsEnd);
        if (!CompOps) {
            SLog(0,
                 "RS_ShipLogs - Couldn't find common point with all sites\n");
            errorCode = EINCONS;

            // update res stats
            conflictstats conf;
            conf.wrap++;
            UpdateStats(Fid, &conf);

            goto Exit;
        }
        PollAndYield();
        PreProcessCompOps(CompOps);
        // print comp ops for debugging
        if (SrvDebugLevel > 10)
            PrintCompOps(CompOps);
        // PollAndYield();
    }
    // Get Objects needed during the resolution
    {
        if ((errorCode = GetResObjs(CompOps, Fid, &volptr, vlist))) {
            SLog(0, "RS_ShipLogs Error %d in Getting objs", errorCode);
            goto Exit;
        }
    }
    // Check Semantics and Perform compensating operations
    {
        PROBE(tpinfo, RecovPerformResOpBegin);
        errorCode = CheckSemPerformRes(CompOps, volptr, Fid, vlist, AllLogs,
                                       inclist, &nblocks, HintFid);
        if (errorCode) {
            SLog(0, "RS_ShipLogs: Error %d during CheckSemPerformRes",
                 errorCode);
            goto Exit;
        }
        PROBE(tpinfo, RecovPerformResOpEnd);
    }

    // Set Status of object and spool log record
    {
        if ((errorCode = SetPhase3DirStatus(status, Fid, volptr, vlist))) {
            SLog(0, "RS_ShipLogs: Error %d during set status\n", errorCode);
            goto Exit;
        }
    }
Exit:
    // Put Objects back to RVM
    /* Do not pass errorCode, this way we end up committing all updates we
     * managed to apply even when a later operation in the log failed. */
    PutObjects(0, volptr, NO_LOCK, vlist, nblocks, 1);

    // Return list of inconsistencies to coordinator
    {
        if (!errorCode)
            DlistToBS(inclist, piggyinc);
    }
    // cleanup
    {
        if (buf)
            free(buf);
        if (AllLogs)
            DeallocateRemoteLogs(AllLogs);
        if (AllLogEntries)
            delete[] AllLogEntries;
        if (CompOps)
            delete CompOps;
        CleanIncList(inclist);
        // vlist & volptr cleaned up in PutObjects
    }

    PROBE(tpinfo, RecovSubP3End);
    SLog(0, "RS_ShipLogs - returning %d", errorCode);
    return (errorCode);
}

long RS_ShipLogs(RPC2_Handle RPCid, ViceFid *Fid, RPC2_Integer size,
                 RPC2_Integer nentries, ViceStatus *status,
                 RPC2_BoundedBS *piggyinc,
                 SE_Descriptor *sed) EXCLUDES_TRANSACTION
{
    return RS_NewShipLogs(RPCid, Fid, size, nentries, status, piggyinc, NULL,
                          sed);
}

static int FetchLog(RPC2_Handle RPCid, char **buf, int size)
{
    int errorCode = 0;
    *buf          = (char *)malloc(size);
    CODA_ASSERT(*buf);

    SE_Descriptor sid;
    memset(&sid, 0, sizeof(SE_Descriptor));
    sid.Tag                                   = SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sid.Value.SmartFTPD.SeekOffset            = 0;
    sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
    sid.Value.SmartFTPD.Tag      = FILEINVM;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen    = size;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = size;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody   = (RPC2_ByteSeq)*buf;

    if ((errorCode = (int)RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
        SLog(0, "FetchLog: InitSE failed (%d)\n", errorCode);
        return (errorCode);
    }

    if ((errorCode = (int)RPC2_CheckSideEffect(
             RPCid, &sid, SE_AWAITLOCALSTATUS)) <= RPC2_ELIMIT) {
        SLog(0, "FetchLog: CheckSE failed (%d)\n", errorCode);
        if (errorCode == RPC2_SEFAIL1)
            errorCode = EIO;
    }
    return (errorCode);
}

static int GetResObjs(arrlist *ops, ViceFid *Fid, Volume **volptr, dlist *vlist)
{
    SLog(1, "Entering GetResObjs(0x%x.%x.%x)\n", Fid->Volume, Fid->Vnode,
         Fid->Unique);

    int errorCode = 0;
    Vnode *pvptr  = 0;

    /* translate fid */

    {
        if (!XlateVid(&Fid->Volume)) {
            SLog(0, "GetResObjs: Coudnt Xlate VSG %x", Fid->Volume);
            return (EINVAL);
        }
    }

    // get  parent directory vnode
    {
        SLog(9, "GetResObjs: Getting parent dir(%x.%x)", Fid->Vnode,
             Fid->Unique);
        if ((errorCode =
                 GetFsObj(Fid, volptr, &pvptr, READ_LOCK, NO_LOCK, 0, 0, 0)))
            goto Exit;
        AddVLE(*vlist, Fid);
    }

    /* gather all the child fids into vlist */
    {
        SLog(9, "GetResObjs: Gathering Fids for children");
        if ((errorCode = GatherFids(vlist, pvptr, *volptr, ops)))
            goto Exit;
    }

    /* put back parent directory's vnode */
    {
        SLog(9, "GetResObjs: Putting back parent vnode ");
        if (pvptr) {
            Error error = 0;
            VPutVnode(&error, pvptr);
            CODA_ASSERT(error == 0);
            pvptr = 0;
        }
    }
    /* get vnodes in fid order */
    {
        int count = 0;
        dlist_iterator next(*vlist);
        vle *v;
        while (!errorCode && (v = (vle *)next())) {
            SLog(9, "GetResObjects: acquiring %s", FID_(&v->fid));
            if (ISDIR(v->fid))
                v->d_inodemod = 1;
            errorCode = GetFsObj(&v->fid, volptr, &v->vptr, WRITE_LOCK, NO_LOCK,
                                 1, 0, v->d_inodemod);
            count++;
            if ((count & Yield_rp3GetResObjMask) == 0)
                PollAndYield();
        }
    }

Exit:
    if (pvptr) {
        SLog(9, "GetResObjs: ERROR condition - Putting back parent");
        Error error = 0;
        VPutVnode(&error, pvptr);
        CODA_ASSERT(error == 0);
        pvptr = 0;
    }

    SLog(1, "GetResObjs: returning(%d)", errorCode);
    return (errorCode);
}

// Add Fid of Child to vlist
//	Make sure child exists before adding to list
//	If Child's parent is different add that also to list
static int AddChildToList(dlist *vlist, Volume *volptr, Vnode *pvptr,
                          VnodeId vn, Unique_t un, int getsubtree)
{
    int errorCode = 0;
    ViceFid ChildFid, ParentFid;
    ChildFid.Volume = ParentFid.Volume = V_id(volptr);
    ChildFid.Vnode                     = vn;
    ChildFid.Unique                    = un;

    if (ObjectExists(V_volumeindex(volptr), ISDIR(ChildFid) ? vLarge : vSmall,
                     vnodeIdToBitNumber(vn), un, &ParentFid)) {
        if (getsubtree && ISDIR(ChildFid))
            errorCode = GetSubTree(&ChildFid, volptr, vlist);
        if (!errorCode) {
            AddVLE(*vlist, &ChildFid);
            if (ParentFid.Vnode != pvptr->vnodeNumber ||
                ParentFid.Unique != pvptr->disk.uniquifier)
                AddVLE(*vlist, &ParentFid);
        }
    }
    return (errorCode);
}

// Gather all fids involved in a rename
static int AddRenameChildrenToList(dlist *vlist, Volume *volptr, Vnode *pvtr,
                                   rsle *r)
{
    int errorCode = 0;
    ViceFid rnsrcFid; // rename source object
    ViceFid rntgtFid; // rename target object's fid
    ViceFid NewDFid; // target parent dir
    ViceFid OldDFid; // source dir
    ViceFid ndvpFid; // target parent dir vnode's parent
    ViceFid odvpFid; // source parent dir vnode's parent
    ViceFid svpFid; // source object vnode's parent
    ViceFid tvpFid; // target object vnode's parent

    // initialize all the fids
    OldDFid.Volume = NewDFid.Volume = rnsrcFid.Volume = rntgtFid.Volume =
        ndvpFid.Volume = odvpFid.Volume = svpFid.Volume = tvpFid.Volume =
            V_id(volptr);

    if (r->u.mv.type == SOURCE) {
        OldDFid.Vnode  = r->dvn;
        OldDFid.Unique = r->du;
        NewDFid.Vnode  = r->u.mv.otherdirv;
        NewDFid.Unique = r->u.mv.otherdiru;
    } else {
        NewDFid.Vnode  = r->dvn;
        NewDFid.Unique = r->du;
        OldDFid.Vnode  = r->u.mv.otherdirv;
        OldDFid.Unique = r->u.mv.otherdiru;
    }
    rnsrcFid.Vnode  = r->u.mv.svnode;
    rnsrcFid.Unique = r->u.mv.sunique;
    if (r->u.mv.tvnode) {
        rntgtFid.Vnode  = r->u.mv.tvnode;
        rntgtFid.Unique = r->u.mv.tunique;
    }

    // Get all the objets
    if (ObjectExists(V_volumeindex(volptr), vLarge,
                     vnodeIdToBitNumber(NewDFid.Vnode), NewDFid.Unique,
                     &ndvpFid)) {
        AddVLE(*vlist, &NewDFid);
        AddVLE(*vlist, &ndvpFid);
    }
    if (!FID_EQ(&NewDFid, &OldDFid) &&
        ObjectExists(V_volumeindex(volptr), vLarge,
                     vnodeIdToBitNumber(OldDFid.Vnode), OldDFid.Unique,
                     &odvpFid)) {
        AddVLE(*vlist, &OldDFid);
        AddVLE(*vlist, &odvpFid);
    }
    if (ObjectExists(V_volumeindex(volptr), ISDIR(rnsrcFid) ? vLarge : vSmall,
                     vnodeIdToBitNumber(rnsrcFid.Vnode), rnsrcFid.Unique,
                     &svpFid)) {
        AddVLE(*vlist, &rnsrcFid);
        if (svpFid.Vnode != OldDFid.Vnode || svpFid.Unique != OldDFid.Unique)
            AddVLE(*vlist, &svpFid);
    }
    if (r->u.mv.tvnode &&
        ObjectExists(V_volumeindex(volptr), ISDIR(rntgtFid) ? vLarge : vSmall,
                     vnodeIdToBitNumber(rntgtFid.Vnode), rntgtFid.Unique,
                     &tvpFid)) {
        if (!ISDIR(rntgtFid))
            AddVLE(*vlist, &rntgtFid);
        else {
            if ((errorCode = GetSubTree(&rntgtFid, volptr, vlist))) {
                SLog(0, "GetResObjs: error %d getting subtree", errorCode);
                return (errorCode);
            }
        }
        if (tvpFid.Vnode != NewDFid.Vnode || tvpFid.Unique != NewDFid.Unique)
            AddVLE(*vlist, &tvpFid);
    }
    return (errorCode);
}

static int GatherFids(dlist *vlist, Vnode *pvptr, Volume *volptr, arrlist *ops)
{
    SLog(1, "GatherFids: Entering for 0x%x.%x.%x\n", V_id(volptr),
         pvptr->vnodeNumber, pvptr->disk.uniquifier);
    int errorCode = 0;
    int count     = 0;
    arrlist_iterator next(ops);
    rsle *r;
    while ((r = (rsle *)next()) && !errorCode) {
        count++;
        if ((count & Yield_rp3CollectFidMask) == 0)
            PollAndYield();

        CODA_ASSERT(r->dvn == pvptr->vnodeNumber);
        CODA_ASSERT(r->du == pvptr->disk.uniquifier);
        switch (r->opcode) {
        case ResolveViceNewStore_OP:
        case RES_NewStore_OP:
        case ResolveViceSetVolumeStatus_OP:
        case RES_SetVolumeStatus_OP:
        case ResolveNULL_OP:
            break;
        case ResolveViceRename_OP:
        case RES_Rename_OP:
            errorCode = AddRenameChildrenToList(vlist, volptr, pvptr, r);
            break;
        case ResolveViceRemove_OP:
        case RES_Remove_OP:
        case ResolveViceCreate_OP:
        case RES_Create_OP:
        case ResolveViceSymLink_OP:
        case RES_SymLink_OP:
        case ResolveViceLink_OP:
        case RES_Link_OP:
        case ResolveViceMakeDir_OP:
        case RES_MakeDir_OP:
            ViceFid cfid;
            ExtractChildFidFromrsle(r, &cfid);
            errorCode =
                AddChildToList(vlist, volptr, pvptr, cfid.Vnode, cfid.Unique);
            break;
        case ResolveViceRemoveDir_OP:
        case RES_RemoveDir_OP:
            errorCode = AddChildToList(vlist, volptr, pvptr, r->u.rmdir.cvnode,
                                       r->u.rmdir.cunique, 1);
            break;
        default:
            SLog(0, "GatherFids: Unknown opcode\n", r->opcode);
            errorCode = EINVAL;
            break;
        }
    }
    return (errorCode);
}

/* CheckSemPerformRes:
 *	Given a list of compensating operations (rlog)
 *	Check if it is legal to perform all these operations
 *		Algorithm:
 *		Check if Name exists in parent directory
 *		Check if object exists -
 *			If so, is it in the same directory
 *		Look up table to see if operation not allowed
 *	Then perform the operation accordingly
 *
 *	Return values:
 *		functions return value indicates abort/commit
 *		result[] - array of codes for outcome of each operation
 *			PERFORMOP, NULLOP, MARKPARENTINC, MARKOBJINC
 *			CREATEINCOBJ
 *
 */
static int CheckSemPerformRes(arrlist *ops, Volume *volptr, ViceFid *dFid,
                              dlist *vlist, olist *AllLogs, dlist *inclist,
                              int *nblocks, DirFid *HintFid)
{
    int errorCode = 0;
    *nblocks      = 0;

    SLog(9, "Entering CheckSemPerformRes()");

    // Validate Parameters
    VolumeId VSGVolnum = V_id(volptr);
    vle *pv            = 0; // parent vlist entry
    {
        if (!ReverseXlateVid(&VSGVolnum)) {
            SLog(0, "CheckSemPerformRes: Couldnt RevXlateVid %x", VSGVolnum);
            errorCode = EINVAL;
            goto Exit;
        }

        pv = FindVLE(*vlist, dFid);
        CODA_ASSERT(pv && pv->vptr);
    }

    // check semantics and perform operations
    {
        int tblocks = 0;
        arrlist_iterator next(ops);
        rsle *r;
        for (int count = 0; !errorCode && (r = (rsle *)next()); count++) {
            // yield after every few operations
            if ((count & Yield_rp3CheckSemPerformRes_Mask) == 0)
                PollAndYield();

            if (r->opcode == ResolveNULL_OP)
                continue;

            // handle renames separately
            if (r->opcode == RES_Rename_OP ||
                r->opcode == ResolveViceRename_OP) {
                if (!(errorCode = CheckAndPerformRename(
                          r, volptr, VSGVolnum, dFid, vlist, AllLogs, inclist,
                          &tblocks, HintFid)))
                    *nblocks += tblocks;
                continue;
            }

            // regular operation check and perform
            {
                int result =
                    CheckRegularCompOp(r, vlist, pv, dFid, volptr, AllLogs);
                SLog(9, "CheckRegularCompOp returns %d\n", result);
                if (!(errorCode = PerformRegularCompOp(
                          result, r, vlist, inclist, AllLogs, dFid, pv, volptr,
                          VSGVolnum, &tblocks)))
                    *nblocks += tblocks;
            }
        }
    }
Exit:
    SLog(9, "CheckSemPerformRes: returning %d", errorCode);
    return (errorCode);
}

static int NameExistsInParent(rsle *r, Vnode *pvptr)
{
    int NameExists = FALSE;
    char *name     = ExtractNameFromrsle(r);
    ViceFid nfid;
    if (name) {
        PDirHandle dh;
        dh = VN_SetDirHandle(pvptr);
        if (DH_Lookup(dh, name, &nfid, CLU_CASE_SENSITIVE) == 0)
            NameExists = TRUE;
        VN_PutDirHandle(pvptr);
    }
    SLog(39, "NameExistsInParent: NameExists = %d", NameExists);
    return (NameExists);
}

static int DoesObjExist(rsle *r, dlist *vlist, VolumeId vid)
{
    ViceFid cfid;
    ExtractChildFidFromrsle(r, &cfid);
    cfid.Volume = vid;

    // if vnode exists - it will exist in vlist
    int ObjExists = FALSE;
    vle *cv       = FindVLE(*vlist, &cfid);
    if (cv && cv->vptr && !(cv->vptr->delete_me))
        ObjExists = TRUE;
    return (ObjExists);
}

static int IsParentPtrOk(rsle *r, dlist *vlist, ViceFid *dFid)
{
    int ParentPtrOk = TRUE;
    ViceFid cfid;
    ExtractChildFidFromrsle(r, &cfid);
    vle *cv = FindVLE(*vlist, &cfid);
    if (cv && cv->vptr &&
        ((cv->vptr->disk.vparent != dFid->Vnode) ||
         (cv->vptr->disk.uparent != dFid->Unique)))
        ParentPtrOk = FALSE;
    return (ParentPtrOk);
}
static int IsNameFidBindingOK(rsle *r, Vnode *pvptr)
{
    int rc     = FALSE;
    char *name = ExtractNameFromrsle(r);

    ViceFid cfid;
    ExtractChildFidFromrsle(r, &cfid);
    ViceFid nfid;
    if (name) {
        PDirHandle dh;
        dh = VN_SetDirHandle(pvptr);
        if ((DH_Lookup(dh, name, &nfid, CLU_CASE_SENSITIVE) == 0) &&
            (nfid.Vnode == cfid.Vnode) && (nfid.Unique == cfid.Unique))
            rc = TRUE;
        VN_PutDirHandle(pvptr);
    }
    SLog(39, "IsNameFidBindingOK: returns %d", rc);
    return (rc);
}

static int CheckRegularCompOp(rsle *r, dlist *vlist, vle *pv, ViceFid *pFid,
                              Volume *volptr, olist *AllLogs)
{
    // check the different flags first
    int NameExists       = NameExistsInParent(r, pv->vptr);
    int ObjExists        = DoesObjExist(r, vlist, V_id(volptr));
    int ParentPtrOk      = ObjExists ? IsParentPtrOk(r, vlist, pFid) : TRUE;
    int NameFidBindingOk = NameExists ? IsNameFidBindingOK(r, pv->vptr) : TRUE;
    return (CheckValidityResOp(r, NameExists, ObjExists, ParentPtrOk,
                               NameFidBindingOk, vlist, pFid, AllLogs,
                               &(V_VolLog(volptr)->vmrstats->conf), volptr));
}

/*
 * CheckValidityResOp:
 *	This implements the state machine for checking semantics
 *	The state is determined by :
 *		1. Object Exists? (OE)
 *		2. If Exists, then in the same directory? (ParentPtrOk)
 *		3. Name Exists in parent? (NE)
 *	The Result is one of:
 *	        All Ok - Perform the Operation (PERFORMOP)
 *		Mark Parent Inconsistent (MARKPARENTINC)
 *		Mark Object Inconsistent (MARKOBJINC)
 *		Create Inconsistent Object (CREATEINCOBJ)
 *		Null Op - dont do the operation (NULLOP)
 */
static int CheckValidityResOp(rsle *r, int NE, int OE, int ParentPtrOk,
                              int NameFidBindingOk, dlist *vlist, ViceFid *pFid,
                              olist *AllLogs, conflictstats *confstats,
                              Volume *volptr)
{
    switch (r->opcode) {
    case ResolveViceNewStore_OP:
    case RES_NewStore_OP:
        if (r->u.newst.type != STSTORE) {
            CODA_ASSERT(r->u.newst.type == ACLSTORE);
            SLog(0,
                 "CheckValidityResOp: Got an acl store operation in comp op");
            SLog(0, "Marking parent (0x%x.%x) inc\n", r->dvn, r->du);
            confstats->uu++;
            return (MARKPARENTINC);
        } else {
            SLog(
                0,
                "CheckValidityResOp: Got a newstore operation - performing it but algo must be worked on\n");
            CODA_ASSERT(r->u.newst.type == STSTORE);
            if (!OE)
                return MARKPARENTINC;
            return PERFORMOP;
        }

    case ResolveViceRemove_OP:
    case RES_Remove_OP:
        if (!NE) {
            if (OE) {
                SLog(0, "For a remove op, !NE(%s) and OE(0x%x.%x)",
                     ExtractNameFromrsle(r), r->u.rm.cvnode, r->u.rm.cunique);
                SLog(0, "Going to mark parent (0x%x.%x) in conflict\n",
                     pFid->Vnode, pFid->Unique);
                confstats->mv++;
                if (ParentPtrOk) {
                    return (MARKPARENTINC);
                } else {
                    return (MARKPARENTINC);
                }
            } else // object doesnt exist
                return (NULLOP);
        } else { // name exists
            if (OE) { // object exists
                if (!NameFidBindingOk) { // name corresponds to a different object
                    SLog(
                        0,
                        "Name (%s) and object exist but name/fid binding is bad\n",
                        ExtractNameFromrsle(r));
                    return (NULLOP);
                }
                if (ParentPtrOk) {
                    /* object exists in same parent */
                    if (RUConflict(r, vlist, AllLogs, pFid)) {
                        SLog(0, "Rm/Up conflict for %s marking inconsistent\n",
                             ExtractNameFromrsle(r));
                        confstats->ru++;
                        return (MARKOBJINC);
                    } else
                        return (PERFORMOP);
                } else {
                    // object exists in another directory
                    SLog(
                        0,
                        "%s to be rm'd in wrong directory-mark parent0x%x.%x inc\n",
                        ExtractNameFromrsle(r), pFid->Vnode, pFid->Unique);
                    confstats->mv++;
                    return (MARKPARENTINC);
                }
            } else {
                // object doesnt exist
                SLog(0, "%s to rm exists but !OE - mark parent 0x%x.%x inc\n",
                     ExtractNameFromrsle(r), pFid->Vnode, pFid->Unique);
                confstats->other++;
                return (MARKPARENTINC);
            }
        }
        break;
    case ResolveViceCreate_OP:
    case RES_Create_OP:
        if (!NE) {
            if (OE) {
                //CODA_ASSERT(0);	/* site participated in create cant get create again */
                /* Yet we're seeing this, maybe after failed reintegration? */
                return (MARKPARENTINC);
            } else // !OE
                return (PERFORMOP);
        } else { // name exists
            if (OE) {
                CODA_ASSERT(0);
            } else {
                SLog(
                    0,
                    "%s exists already N/N conflict, mark parent 0x%x.%x inc\n",
                    ExtractNameFromrsle(r), pFid->Vnode, pFid->Unique);
                confstats->nn++;
                return (MARKPARENTINC); // N/N Conflict
            }
        }
        break;
    case ResolveViceRename_OP:
    case RES_Rename_OP:
        CODA_ASSERT(0);
        break;
    case ResolveViceSymLink_OP:
    case RES_SymLink_OP:
        if (!NE) {
            if (OE) {
                CODA_ASSERT(0);
            } else
                return (PERFORMOP);
        } else { // name exists
            if (OE) {
                CODA_ASSERT(0);
            } else {
                SLog(
                    0,
                    "%s exists already, N/N conflict, mark parent 0x%x.%x inc\n",
                    ExtractNameFromrsle(r), pFid->Vnode, pFid->Unique);
                confstats->nn++;
                return (MARKPARENTINC);
            }
        }
        break;
    case ResolveViceLink_OP:
    case RES_Link_OP:
        if (!NE) {
            if (OE) {
                if (ParentPtrOk)
                    return (PERFORMOP);
                else { // rename happened on object
                    SLog(
                        0,
                        "Link impossible because 0x%x.%x exists in diff parent",
                        r->u.link.cvnode, r->u.link.cunique);
                    SLog(0, "Marking parent 0x%x.%x inc\n", pFid->Vnode,
                         pFid->Unique);
                    confstats->mv++;
                    return (MARKPARENTINC);
                }
            } else {
                // object doesnt exist - may have been removed
                SLog(0, "!OE(0x%x.%x) for link - creating inc object\n",
                     r->u.link.cvnode, r->u.link.cunique);
                confstats->ru++;
                return (CREATEINCOBJ);
            }
        } else { // name exists
            if (NameFidBindingOk) { // link already exists
                CODA_ASSERT(OE);
            } else { // name points at some other object.... name/name conflict?
                SLog(0, "LinkOP: NE but NFBinding is bad\n");
                return (MARKPARENTINC);
            }
            if (OE) {
                if (ParentPtrOk) {
                    return (NULLOP);
                } else { // slightly fishy here XXX
                    SLog(
                        0,
                        "For Link %s exists but 0x%x.%x exists in diff directory",
                        ExtractNameFromrsle(r), r->u.link.cvnode,
                        r->u.link.cunique);
                    SLog(0, "Marking parent (0x%x.%x) inconsistent\n",
                         pFid->Vnode, pFid->Unique);
                    confstats->mv++;
                    return (MARKPARENTINC);
                }
            } else { // not sure yet XXXX
                SLog(0, "!OE(0x%x.%x) for link %s\n", r->u.link.cvnode,
                     r->u.link.cunique, ExtractNameFromrsle(r));
                SLog(0, "Marking parent (0x%x.%x) inconsistent\n", pFid->Vnode,
                     pFid->Unique);
                confstats->nn++;
                return (MARKPARENTINC);
            }
        }
        break;
    case ResolveViceMakeDir_OP:
    case RES_MakeDir_OP:
        if (!NE) {
            if (OE) {
                CODA_ASSERT(0);
            } else
                /* object doesnt exist */
                return (PERFORMOP);
        } else { /* name exists */
            if (OE) {
                CODA_ASSERT(0);
            } else {
                SLog(0,
                     "N/N conflict for mkdir %s marking parent (0x%x.%x) inc\n",
                     ExtractNameFromrsle(r), pFid->Vnode, pFid->Unique);
                confstats->nn++;
                return (MARKPARENTINC);
            }
        }
        break;
    case ResolveViceRemoveDir_OP:
    case RES_RemoveDir_OP:
        if (!NE) {
            if (OE) {
                if (ParentPtrOk) {
                    SLog(0,
                         "For Rmdir %s was renamed - marking(0x%x.%x) inc \n",
                         ExtractNameFromrsle(r), r->u.rmdir.cvnode,
                         r->u.rmdir.cunique);
                    confstats->mv++;
                    return (MARKOBJINC);
                } else {
                    SLog(
                        0,
                        "For Rmdir %s moved to another dir- marking(0x%x.%x) inc \n",
                        ExtractNameFromrsle(r), pFid->Vnode, pFid->Unique);
                    confstats->mv++;
                    return (MARKPARENTINC);
                }
            } else
                return (NULLOP);
        } else { /* name exists */
            if (!NameFidBindingOk) {
                if (OE) {
                    SLog(0, "For Rmdir: NE, OE, but NFidBinding bad\n");
                    return (MARKPARENTINC);
                } else {
                    SLog(0, "For Rmdir: NE, !OE and NFidBinding is bad\n");
                    return (NULLOP);
                }
            }
            if (OE) {
                if (ParentPtrOk) {
                    if (RUConflict(r, vlist, AllLogs, pFid)) {
                        SLog(0, "For Rmdir(%s)0x%x.%x: r/u conflict\n",
                             ExtractNameFromrsle(r), r->u.rmdir.cvnode,
                             r->u.rmdir.cunique);
                        confstats->ru++;
                        return (MARKOBJINC);
                    } else
                        return (PERFORMOP);
                } else { // object exists in another directory
                    SLog(0, "For Rmdir %s has been renamed ",
                         ExtractNameFromrsle(r));
                    SLog(0, "Marking parent 0x%x.%x inc\n", pFid->Vnode,
                         pFid->Unique);
                    confstats->mv++;
                    return (MARKPARENTINC);
                }
            } else
                /* object doesnt exist */
                return (NULLOP);
        }
        break;

    case ResolveViceSetVolumeStatus_OP:
    case RES_SetVolumeStatus_OP:
        SLog(0, "Entering vicesetvolume_OP\n");
        r->print();
        if (V_maxquota(volptr) == r->u.sq.newquota)
            return (NULLOP);
        else if (V_maxquota(volptr) == r->u.sq.oldquota)
            return (PERFORMOP);
        else {
            SLog(0, "U/U confilict for quota in volume 0x%x\n", V_id(volptr));
            //	  return(NULLOP);
            confstats->uu++;
            return (MARKPARENTINC);
        }
    default:
        CODA_ASSERT(0);
    }
    return 0;
}

static int PerformRegularCompOp(int result, rsle *rp, dlist *vlist,
                                dlist *inclist, olist *AllLogs, ViceFid *dFid,
                                vle *pv, Volume *volptr, VolumeId VSGVolnum,
                                int *nblocks)
{
    int tblocks   = 0;
    int errorCode = 0;
    ViceFid cFid;
    vle *cv;
    char *name;
    int vntype;

    switch (result) {
    case PERFORMOP:
        SLog(9, "PerformRegularCompOp: Going to Perform Op");
        errorCode =
            PerformResOp(rp, vlist, AllLogs, pv, volptr, VSGVolnum, &tblocks);
        break;
    case NULLOP:
        SLog(9, "PerformRegularCompOp: NULL Operation - ignore");
        break;
    case MARKPARENTINC:
        SLog(9, "PerformRegularCompOp: Marking Parent Inc");
        MarkObjInc(dFid, pv->vptr);
        AddILE(*inclist, ".", dFid->Vnode, dFid->Unique, dFid->Vnode,
               dFid->Unique, (long)vDirectory);
        break;
    case MARKOBJINC:
        SLog(9, "CheckSemPerformRes: Marking Object Inc");
        ExtractChildFidFromrsle(rp, &cFid);
        cFid.Volume = V_id(volptr);
        cv          = FindVLE(*vlist, &cFid);
        if (!cv || !cv->vptr) {
            SLog(0, "MARKOBJINC: couldnt get obj(0x%x.%x)vnode pointer",
                 cFid.Vnode, cFid.Unique);
            errorCode = EINVAL;
        } else {
            MarkObjInc(&cFid, cv->vptr);
            name = ExtractNameFromrsle(rp);
            AddILE(*inclist, name, cFid.Vnode, cFid.Unique, dFid->Vnode,
                   dFid->Unique, (long)(cv->vptr->disk.type));
        }
        break;
    case CREATEINCOBJ:
        // xxx BE CAREFUL WITH CHILD FID AND RENAMES
        SLog(9, "CheckSemPerformRes: Creating Inc Object");
        ExtractChildFidFromrsle(rp, &cFid);
        cFid.Volume = V_id(volptr);
        name        = ExtractNameFromrsle(rp);
        vntype      = ExtractVNTypeFromrsle(rp);
        errorCode = CreateObjToMarkInc(volptr, dFid, &cFid, name, vntype, vlist,
                                       &tblocks);
        if (errorCode == 0) {
            cv = FindVLE(*vlist, &cFid);
            CODA_ASSERT(cv);
            CODA_ASSERT(cv->vptr);
            MarkObjInc(&cFid, cv->vptr);
            AddILE(*inclist, name, cFid.Vnode, cFid.Unique,
                   cv->vptr->disk.vparent, cv->vptr->disk.uparent,
                   (long)(cv->vptr->disk.type));
        }
        break;
    default:
        SLog(0, "Illegal opcode for PerformCompOp\n");
        CODA_ASSERT(0);
    }

    *nblocks += tblocks;
    return (errorCode);
}

static int PerformResOp(rsle *r, dlist *vlist, olist *AllLogs, vle *pv,
                        Volume *volptr, VolumeId VSGVolnum, int *blocks)
{
    SLog(9, "Entering PerformResOp: %s ", PRINTOPCODE(r->opcode));
    int errorCode = 0;
    *blocks       = 0;
    char *name    = ExtractNameFromrsle(r);
    ViceFid cFid;
    ExtractChildFidFromrsle(r, &cFid);
    cFid.Volume = V_id(volptr);

    /* perform the operation */
    RPC2_Integer Mask;
    switch (r->opcode) {
    case ResolveViceNewStore_OP:
    case RES_NewStore_OP:
        if (r->u.newst.type != STSTORE)
            CODA_ASSERT(0);

        // perform the store
        Mask = r->u.newst.mask;

        if (Mask & SET_OWNER)
            pv->vptr->disk.owner = r->u.newst.owner;

        if (Mask & SET_MODE)
            pv->vptr->disk.modeBits = (unsigned)(r->u.newst.mode);

        pv->vptr->disk.author = r->u.newst.author;

        if (Mask & SET_TIME)
            pv->vptr->disk.unixModifyTime = r->u.newst.mtime;

        // this is for directory only, so there is no set length operation on it
        // spool the record
        if ((errorCode = SpoolVMLogRecord(
                 vlist, pv, volptr, &r->storeid, ResolveViceNewStore_OP,
                 STSTORE, r->u.newst.owner, r->u.newst.mode, r->u.newst.author,
                 r->u.newst.mtime, Mask, &(Vnode_vv(pv->vptr)))))
            SLog(0, "PeformResOp: error %d during SpoolVMLogRecord\n",
                 errorCode);
        break;

    case ResolveViceRemove_OP:
    case RES_Remove_OP: {
        SLog(9, "PerformResOP: Removing child %s(%x.%x)", name, cFid.Vnode,
             cFid.Unique);
        vle *cv = FindVLE(*vlist, &cFid);
        CODA_ASSERT(cv);
        CODA_ASSERT(cv->vptr);

        /* perform remove */
        {
            PerformRemove(NULL, VSGVolnum, volptr, pv->vptr, cv->vptr, name,
                          pv->vptr->disk.unixModifyTime, 0, &r->storeid,
                          &pv->d_cinode, blocks);
            if (cv->vptr->delete_me) {
                int tblocks = (int)(-nBlocks(cv->vptr->disk.length));
                CODA_ASSERT(AdjustDiskUsage(volptr, tblocks) == 0);
                *blocks += tblocks;
                cv->f_sinode = cv->vptr->disk.node.inodeNumber;
                cv->vptr->disk.node.inodeNumber = 0;
            }
        }
        /* spool log record */
        {
            SLog(9, "PerformResOp: Spooling log record Remove(%s)", name);
            ViceVersionVector ghostVV = cv->vptr->disk.versionvector;
            if ((errorCode = SpoolVMLogRecord(
                     vlist, pv, volptr, &r->storeid, ResolveViceRemove_OP, name,
                     cv->fid.Vnode, cv->fid.Unique, &ghostVV)))
                SLog(0, "PeformResOp: error %d during SpoolVMLogRecord\n",
                     errorCode);
        }
    } break;
    case ResolveViceCreate_OP:
    case RES_Create_OP: {
        SLog(9, "PerformResOP: creating child %s(%x.%x)", name, cFid.Vnode,
             cFid.Unique);
        /* create the vnode */
        vle *cv = AddVLE(*vlist, &cFid);
        CODA_ASSERT(cv->vptr == 0);
        if ((errorCode = AllocVnode(&cv->vptr, volptr, (ViceDataType)vFile,
                                    &cFid, &pv->fid, r->u.create.owner,
                                    blocks))) {
            SLog(0, "PerformResOP: Error %d in AllocVnode", errorCode);
            return (errorCode);
        }
        int tblocks = 0;
        errorCode   = PerformCreate(NULL, VSGVolnum, volptr, pv->vptr, cv->vptr,
                                    name, pv->vptr->disk.unixModifyTime,
                                    pv->vptr->disk.modeBits, 0, &r->storeid,
                                    &pv->d_cinode, &tblocks);
        *blocks += tblocks;
        CODA_ASSERT(errorCode == 0);
        cv->vptr->disk.owner  = r->u.create.owner; /* sent in a NULL client */
        cv->vptr->disk.author = r->u.create.owner;

        /* append log record */
        SLog(9, "PerformResOp: Spooling log record Create(%s)", name);
        if ((errorCode = SpoolVMLogRecord(
                 vlist, pv, volptr, &r->storeid, ResolveViceCreate_OP, name,
                 cFid.Vnode, cFid.Unique, cv->vptr->disk.owner)))
            SLog(0, "ResolveViceCreate: Error %d during SpoolVMLogRecord\n",
                 errorCode);
    } break;
    case ResolveViceRename_OP:
    case RES_Rename_OP:
        CODA_ASSERT(0);
        break;
    case ResolveViceSymLink_OP:
    case RES_SymLink_OP: {
        SLog(9, "PerformResOP: Creating SymLink %s(%x.%x)", name, cFid.Vnode,
             cFid.Unique);
        /* create the vnode */
        vle *cv = AddVLE(*vlist, &cFid);
        CODA_ASSERT(cv->vptr == 0);
        if ((errorCode = AllocVnode(&cv->vptr, volptr, (ViceDataType)vSymlink,
                                    &cFid, &pv->fid, r->u.slink.owner,
                                    blocks))) {
            SLog(0, "PerformResOP: Error %d in AllocVnode(symlink)", errorCode);
            return (errorCode);
        }
        int tblocks = 0;
        errorCode = PerformSymlink(NULL, VSGVolnum, volptr, pv->vptr, cv->vptr,
                                   name, 0, 0, pv->vptr->disk.unixModifyTime,
                                   pv->vptr->disk.modeBits, 0, &r->storeid,
                                   &pv->d_cinode, &tblocks);
        *blocks += tblocks;
        CODA_ASSERT(errorCode == 0);

        cv->vptr->disk.owner  = r->u.slink.owner;
        cv->vptr->disk.author = r->u.slink.owner;
        /* create the inode */
        cv->f_finode = icreate(V_device(volptr), V_id(volptr),
                               cv->vptr->vnodeNumber, cv->vptr->disk.uniquifier,
                               1);
        CODA_ASSERT(cv->f_finode > 0);
        cv->vptr->disk.node.inodeNumber = cv->f_finode;

        /* append log record */
        SLog(9, "PerformResOp: Spooling log record SymLink(%s)", name);
        if ((errorCode = SpoolVMLogRecord(
                 vlist, pv, volptr, &r->storeid, ResolveViceSymLink_OP, name,
                 cFid.Vnode, cFid.Unique, cv->vptr->disk.owner)))
            SLog(0, "PerformResOp(SymLink): Error %d in SpoolVMLogRecord\n",
                 errorCode);
    } break;
    case ResolveViceLink_OP:
    case RES_Link_OP: {
        SLog(9, "PerformResOP: Creating Link %s(%x.%x)", name, cFid.Vnode,
             cFid.Unique);
        vle *cv = FindVLE(*vlist, &cFid);
        if (!cv || !cv->vptr) {
            SLog(0, "PerformResOp: CreateL %x.%x doesnt exist", cFid.Vnode,
                 cFid.Unique);
            return (EINVAL);
        }

        /* add name to parent */
        errorCode = PerformLink(0, VSGVolnum, volptr, pv->vptr, cv->vptr, name,
                                cv->vptr->disk.unixModifyTime, 0, &r->storeid,
                                &pv->d_cinode, blocks);
        CODA_ASSERT(errorCode == 0);

        /* spool log record */
        SLog(9, "PerformResOp: Spooling log record Link(%s)", name);
        if ((errorCode = SpoolVMLogRecord(vlist, pv, volptr, &r->storeid,
                                          ResolveViceLink_OP, name, cFid.Vnode,
                                          cFid.Unique, &(Vnode_vv(cv->vptr)))))
            SLog(0, "ViceLink: Error %d during SpoolVMLogRecord\n", errorCode);
    } break;
    case ResolveViceMakeDir_OP:
    case RES_MakeDir_OP: {
        SLog(9, "PerformResOP: MakeDir %s(%x.%x)", name, cFid.Vnode,
             cFid.Unique);
        Vnode *cvptr = 0;
        /* allocate the vnode */
        if ((errorCode = AllocVnode(&cvptr, volptr, (ViceDataType)vDirectory,
                                    &cFid, &pv->fid, r->u.mkdir.owner,
                                    blocks))) {
            SLog(0, "PerformResOP: Error %d in AllocV(mkdir)", errorCode);
            return (errorCode);
        }
        vle *cv        = AddVLE(*vlist, &cFid);
        cv->vptr       = cvptr;
        cv->d_inodemod = 1;

        /* make the directory */
        int tblocks = 0;
        errorCode = PerformMkdir(0, VSGVolnum, volptr, pv->vptr, cv->vptr, name,
                                 pv->vptr->disk.unixModifyTime,
                                 pv->vptr->disk.modeBits, 0, &r->storeid,
                                 &pv->d_cinode, &tblocks);
        CODA_ASSERT(errorCode == 0);
        *blocks += tblocks;
        // set the storeid  of child
        Vnode_vv(cv->vptr).StoreId = r->storeid;
        cv->vptr->disk.owner       = r->u.mkdir.owner;
        cv->vptr->disk.author      = r->u.mkdir.owner;

        // spool log record
        SLog(9, "PerformResOp: Spooling log record MkDir(%s)", name);
        if ((errorCode = SpoolVMLogRecord(
                 vlist, pv, volptr, &r->storeid, ResolveViceMakeDir_OP, name,
                 cFid.Vnode, cFid.Unique, r->u.mkdir.owner)))
            SLog(
                0,
                "PeformResOp(Mkdir): Error %d during SpoolVMLogRecord for parent\n",
                errorCode);

        // spool a RES_MakeDir_OP record for child so that subsequent resolve can
        // find a common point
        if (!errorCode && (errorCode = SpoolVMLogRecord(
                               vlist, cv, volptr, &r->storeid, RES_MakeDir_OP,
                               ".", cFid.Vnode, cFid.Unique, r->u.mkdir.owner)))
            SLog(
                0,
                "PerformResOp(Mkdir): Error %d during SpoolVMLogRecord for child\n",
                errorCode);
    } break;
    case ResolveViceRemoveDir_OP:
    case RES_RemoveDir_OP: {
        SLog(9, "PerformResOP: Removing child dir %s(%x.%x)", name, cFid.Vnode,
             cFid.Unique);
        vle *cv = FindVLE(*vlist, &cFid);
        CODA_ASSERT(cv);
        CODA_ASSERT(cv->vptr);

        PDirHandle cdir;
        cdir = VN_SetDirHandle(cv->vptr);
        /* first make the directory empty */
        {
            if (!DH_IsEmpty(cdir)) {
                /* remove children first */
                TreeRmBlk pkdparm;
                pkdparm.init(0, VSGVolnum, volptr, 0, &r->storeid, vlist, 1,
                             AllLogs, r->index, blocks);
                DH_EnumerateDir(cdir, PerformTreeRemoval, (void *)&pkdparm);
            }
        }
        int tblocks = 0;
        // remove the empty directory
        {
            CODA_ASSERT(DH_IsEmpty(cdir));
            VN_PutDirHandle(cv->vptr);
            tblocks = 0;
            PerformRmdir(0, VSGVolnum, volptr, pv->vptr, cv->vptr, name,
                         pv->vptr->disk.unixModifyTime, 0, &r->storeid,
                         &pv->d_cinode, &tblocks);
            *blocks += tblocks;
            CODA_ASSERT(cv->vptr->delete_me);
            tblocks = (int)(-nBlocks(cv->vptr->disk.length));
            CODA_ASSERT(AdjustDiskUsage(volptr, tblocks) == 0);
            *blocks += tblocks;
        }

        //	spool log record - should have spooled records recursively
        {
            SLog(9, "PerformResOp: Spooling Log Record RmDir(%s)", name);
            int errorCode = 0;
            if ((errorCode = SpoolVMLogRecord(
                     vlist, pv, volptr, &r->storeid, ResolveViceRemoveDir_OP,
                     name, cv->fid.Vnode, cv->fid.Unique, VnLog(cv->vptr),
                     &(Vnode_vv(cv->vptr).StoreId),
                     &(Vnode_vv(cv->vptr).StoreId))))
                SLog(0, "PerformResOp(RmDir): error %d in SpoolVMLogRecord\n",
                     errorCode);
        }
    } break;
    case ResolveViceSetVolumeStatus_OP:
    case RES_SetVolumeStatus_OP: {
        SLog(9, "PerformResOP: Resolving quota for volume 0x%x\n",
             V_id(volptr));

        vle *cv = FindVLE(*vlist, &cFid);
        CODA_ASSERT(cv);
        CODA_ASSERT(cv->vptr);

        PerformSetQuota(0, VSGVolnum, volptr, cv->vptr, &cFid, r->u.sq.newquota,
                        0, &r->storeid);
        if ((errorCode = SpoolVMLogRecord(vlist, cv, volptr, &r->storeid,
                                          ResolveViceSetVolumeStatus_OP,
                                          r->u.sq.oldquota, r->u.sq.newquota)))
            SLog(0,
                 "PerfromResOp(SetQuota): Error %d during SpoolVMLogRecord\n",
                 errorCode);
    } break;
    default:
        SLog(0, "Illegal Opcode for performresop %d", r->opcode);
        CODA_ASSERT(0);
        break;
    }

    SLog(9, "PerformResOp: Returns %d", errorCode);
    return (errorCode);
}

ViceStoreId *GetRemoteRemoveStoreId(olist *AllLogs, unsigned long serverid,
                                    ViceFid *pFid, ViceFid *cFid, char *cname)
{
    SLog(9, "Entering GetRemoteRemoveStoreId: Parent = %x.%x; Child = %x.%x %s",
         pFid->Vnode, pFid->Unique, cFid->Vnode, cFid->Unique, cname);
    olist *rmtloglist = NULL;
    // find the remote parent's log
    {
        rmtloglist = FindRemoteLog(AllLogs, serverid, pFid);
        if (!rmtloglist)
            return (NULL);
    }

    // search log for child's deletion entry
    {
        olist_iterator next(*rmtloglist);
        rsle *ep = NULL;
        while ((ep = (rsle *)next())) {
            if ((ep->opcode == RES_Remove_OP ||
                 ep->opcode == ResolveViceRemove_OP) &&
                (ep->u.rm.cvnode == cFid->Vnode) &&
                (ep->u.rm.cunique == cFid->Unique) &&
                (!strcmp(ep->name1, cname)))
                return (&ep->storeid);
            if ((ep->opcode == RES_RemoveDir_OP ||
                 ep->opcode == ResolveViceRemoveDir_OP) &&
                (ep->u.rmdir.cvnode == cFid->Vnode) &&
                (ep->u.rmdir.cunique == cFid->Unique) &&
                (!strcmp(ep->name1, cname)))
                return (&ep->storeid);
        }
    }
    SLog(9, "GetRemoteRemoveStoreId: Couldnt find remove entry for %s %x.%x",
         cname, cFid->Vnode, cFid->Unique);
    return NULL;
}

// SetPhase3DirStatus
//	Set a new(local) storeid on directory so that if coordinator crashes
//		resolution will need to be reinvoked
//	Set the VV sent over from coordinator first simulating a COP1
//	Spool a new resolution log record indicating a resolve was done
static int SetPhase3DirStatus(ViceStatus *status, ViceFid *Fid, Volume *volptr,
                              dlist *vlist)
{
    ViceStoreId stid;
    AllocStoreId(&stid);
    ViceVersionVector DiffVV;
    DiffVV  = status->VV;
    vle *ov = FindVLE(*vlist, Fid);
    CODA_ASSERT(ov && ov->vptr);

    // check if new vv is legal
    VV_Cmp_Result res = VV_Cmp_IgnoreInc(&Vnode_vv(ov->vptr), &DiffVV);
    if (res != VV_EQ && res != VV_SUB) {
        SLog(0, "SetPhase3DirStatus: 0x%x.%x VV's are in conflict",
             ov->vptr->vnodeNumber, ov->vptr->disk.uniquifier);
        CODA_ASSERT(0);
    } else {
        SubVVs(&DiffVV, &Vnode_vv(ov->vptr));
        AddVVs(&Vnode_vv(ov->vptr), &DiffVV);
        AddVVs(&V_versionvector(volptr), &DiffVV);

        VolumeId VSGVolnum = Fid->Volume;
        if (ReverseXlateVid(&VSGVolnum)) {
            CodaBreakCallBack(0, Fid, VSGVolnum);
        } else {
            CodaBreakCallBack(0, Fid, Fid->Volume);
        }
    }

    //do cop1 update with new local storeid -
    // ensure that different directory replicas are unequal
    NewCOP1Update(volptr, ov->vptr, &stid);
    SetCOP2Pending(Vnode_vv(ov->vptr));

    ov->vptr->disk.owner          = status->Owner;
    ov->vptr->disk.author         = status->Author;
    ov->vptr->disk.modeBits       = status->Mode;
    ov->vptr->disk.unixModifyTime = status->Date;
    SetStatus(ov->vptr, status, 0, 0);

    // append log record
    return (SpoolVMLogRecord(vlist, ov, volptr, &stid, ResolveNULL_OP, 0));
}

static void PreProcessCompOps(arrlist *ops)
{
    // form new arrlist
    arrlist newops(ops->maxsize);

    arrlist_iterator next(ops);
    void *p;
    while ((p = next()))
        newops.add(p);

    qsort(newops.list, newops.cursize, sizeof(void *),
          (int (*)(const void *, const void *))CmpFidOp);

    rsle *prev = NULL;
    rsle *curr = NULL;
    arrlist_iterator nextnew(&newops);
    while ((curr = (rsle *)nextnew())) {
        if (!prev) {
            prev = curr;
            continue;
        }
        if (SrvDebugLevel > 20) {
            SLog(0, "Previous rsle is :\n");
            prev->print();
            SLog(0, "Current rsle is :\n");
            curr->print();
        }

        if ((ISDELETEOP(curr->opcode)) && (ISCREATEOP(prev->opcode))) {
            ViceFid c1fid, c2fid;
            c1fid.Volume = c2fid.Volume = 0;
            ExtractChildFidFromrsle(curr, &c1fid);
            ExtractChildFidFromrsle(prev, &c2fid);
            if (!FID_Cmp(&c1fid, &c2fid) && !strcmp(curr->name1, prev->name1)) {
                curr->opcode = ResolveNULL_OP;
                prev->opcode = ResolveNULL_OP;
            }
        }
        prev = curr;
    }
}
/*
 * CmpFidOp:
 * 	Compare 2 rsle * by fid and operation
 *	Return -ve, 0, +ve for a < b, a=b, a > b respectively
 *	Primary sort is on fid;
 *	If fids are equal then order of operation are :
 *		CREATE(Fid) <ALL OTHER OPS (Fid)> DELETE(Fid)
 */
static int CmpFidOp(rsle **a, rsle **b)
{
    ViceFid fa, fb;
    int res = 0;

    /* fill in the fids */
    {
        fa.Volume = fb.Volume = 0;
        ExtractChildFidFromrsle(*a, &fa);
        ExtractChildFidFromrsle(*b, &fb);
    }

    /* Compare the fid first */
    {
        res = FID_Cmp(&fa, &fb);
        if (res)
            return (res);
    }
    /* Compare the ops if fids are same */
    {
        int oa = (int)((*a)->opcode);
        int ob = (int)((*b)->opcode);

        switch (oa) {
        case RES_Create_OP:
        case ResolveViceCreate_OP:
        case RES_MakeDir_OP:
        case ResolveViceMakeDir_OP:
        case RES_SymLink_OP:
        case ResolveViceSymLink_OP:
            return (-1);
        case RES_Remove_OP:
        case ResolveViceRemove_OP:
        case RES_RemoveDir_OP:
        case ResolveViceRemoveDir_OP:
            return (1);
        default:
            break;
        }
        switch (ob) {
        case RES_Create_OP:
        case ResolveViceCreate_OP:
        case RES_MakeDir_OP:
        case ResolveViceMakeDir_OP:
        case RES_SymLink_OP:
        case ResolveViceSymLink_OP:
            return (1);
        case RES_Remove_OP:
        case ResolveViceRemove_OP:
        case RES_RemoveDir_OP:
        case ResolveViceRemoveDir_OP:
            return (-1);
        default:
            break;
        }
        return (0);
    }
}

static void UpdateStats(ViceFid *Fid, conflictstats *cs)
{
    VolumeId vid   = Fid->Volume;
    Volume *volptr = 0;
    if (XlateVid(&vid)) {
        if (!GetVolObj(vid, &volptr, VOL_NO_LOCK, 0, 0)) {
            if (AllowResolution && V_RVMResOn(volptr))
                V_VolLog(volptr)->vmrstats->conf.update(cs);
        } else {
            SLog(0, "UpdateStats: couldn't get vol obj 0x%x\n", vid);
            volptr = 0;
        }
    } else
        SLog(0, "UpdateStats: couldn't Xlate Fid 0x%x\n", vid);
    if (volptr)
        PutVolObj(&volptr, VOL_NO_LOCK, 0);
}
