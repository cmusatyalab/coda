/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 *  Code relating to volume callbacks.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <struct.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "mgrp.h"
#include "venuscb.h"
#include "venusvol.h"
#include "venus.private.h"
#include "vproc.h"
#include "worker.h"

int vcbbreaks = 0; /* count of broken volume callbacks */

int vdb::CallBackBreak(Volid *volid)
{
    int rc       = 0;
    volent *v    = VDB->Find(volid);
    reintvol *vp = (reintvol *)v;

    if (!v)
        return 0;

    rc = vp->CallBackBreak();
    if (rc)
        vcbbreaks++;

    v->release();

    return (rc);
}

/*
 * GetVolAttr - Get a volume version stamp (or validate one if
 * present) and get a callback.  If validating, and there are
 * other volumes that need validating, do them too.
 */
int reintvol::GetVolAttr(uid_t uid)
{
    int code   = 0;
    connent *c = NULL;
    nonrepvol_iterator next;
    repvol *repv   = (repvol *)this;
    long cbtemp    = cbbreaks;
    mgrpent *m     = 0;
    unsigned int i = 0;
    struct MRPC_common_params rpc_common;
    struct in_addr ph_addr;
    int ret_code = 0;
    LOG(100, ("reintvol::GetVolAttr: %s, vid = 0x%x\n", name, vid));

    VOL_ASSERT(this, IsReachable());

    /* Acquire an Mgroup. */
    code = GetConn(&c, uid, &m, &rpc_common.ph_ix, &ph_addr);
    if (code != 0)
        goto RepExit;

    cbtemp        = cbbreaks;
    rpc_common.ph = ntohl(ph_addr.s_addr);

    if (IsReplicated()) {
        rpc_common.nservers = VSG_MEMBERS;
        rpc_common.hosts    = m->rocc.hosts;
        rpc_common.retcodes = m->rocc.retcodes;
        rpc_common.handles  = m->rocc.handles;
        rpc_common.MIp      = m->rocc.MIp;

    } else {
        rpc_common.nservers = 1;
        rpc_common.hosts    = &ph_addr;
        rpc_common.retcodes = &ret_code;
        rpc_common.handles  = &c->connid;
        rpc_common.MIp      = 0;
    }

    {
        /*
	 * if we're fetching (as opposed to validating) volume state,
	 * we must first ensure all cached file state from this volume
	 * is valid (i.e., our cached state corresponds to the version
	 * information we will get).  If the file state can't be
	 * validated, we bail.
	 */
        if (VV_Cmp(&VVV, &NullVV) == VV_EQ) {
            if ((code = ValidateFSOs()))
                goto RepExit;

            RPC2_Integer VS;
            ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS,
                         (unsigned int)rpc_common.nservers);

            CallBackStatus CBStatus;
            ARG_MARSHALL(OUT_MODE, CallBackStatus, CBStatusvar, CBStatus,
                         (unsigned int)rpc_common.nservers);

            /* Make the RPC call. */
            MarinerLog("store::GetVolVS %s\n", name);
            MULTI_START_MESSAGE(ViceGetVolVS_OP);
            code = (int)MRPC_MakeMulti(ViceGetVolVS_OP, ViceGetVolVS_PTR,
                                       rpc_common.nservers, rpc_common.handles,
                                       rpc_common.retcodes, rpc_common.MIp, 0,
                                       0, vid, VSvar_ptrs, CBStatusvar_ptrs);
            MULTI_END_MESSAGE(ViceGetVolVS_OP);
            MarinerLog("store::getvolvs done\n");

            /* Collate responses from individual servers and decide what to do next. */
            if (IsReplicated())
                code = repv->Collate_NonMutating(m, code);
            else
                code = Collate(c, code);
            MULTI_RECORD_STATS(ViceGetVolVS_OP);

            if (code != 0)
                goto RepExit;

            if (cbtemp == cbbreaks) {
                if (IsReplicated())
                    repv->CollateVCB(m, VSvar_bufs, CBStatusvar_bufs);
                else if (IsNonReplicated())
                    UpdateVCBInfo(VSvar_bufs[0], CBStatusvar_bufs[0]);
            }
        } else {
            /*
	     * Figure out how many volumes to validate.
	     * We can do this every call because there are a small number of volumes.
	     * We send the server its version stamp, it its slot and sends back yea or nay.
	     */
            int nVols = 0;
            ViceVolumeIdStruct VidList[MAX_PIGGY_VALIDATIONS];

            /*
	     * To minimize bandwidth, we should not send full version vectors
	     * to each server.  We could send each server its version stamp,
	     * but that would be extremely messy for multicast (which assumes
	     * the same message goes to all destinations).  We compromise by
	     * sending all the version stamps to each server.  If we had true
	     * multicast, this would be cheaper than sending a set of unicasts
	     * each with a different version stamp. The array is a BS because
	     * there isn't a one to one correspondence between it and the
	     * volume ID list. Besides, I hate introducing those damn
	     * structures.
	     */
            RPC2_CountedBS VSBS;
            VSBS.SeqLen  = 0;
            VSBS.SeqBody = (RPC2_ByteSeq)malloc(MAX_PIGGY_VALIDATIONS *
                                                rpc_common.nservers *
                                                sizeof(RPC2_Integer));

            /*
	     * this is a BS instead of an array because the RPC2 array
	     * implementation requires array elements to be structures. In the
	     * case of VFlags, that would be a real waste of space (which is
	     * going over the wire).
	     */
            signed char VFlags[MAX_PIGGY_VALIDATIONS];
            RPC2_BoundedBS VFlagBS;
            VFlagBS.MaxSeqLen = 0;
            VFlagBS.SeqLen    = 0;
            VFlagBS.SeqBody   = (RPC2_ByteSeq)VFlags;

            /*
	     * validate volumes that:
	     * - are replicated
	     * - are in the same vsg
	     * - are in the hoarding state
	     * - want a volume callback (includes check for presence of one)
	     * - have a non-null version vector for comparison
	     *
	     * Note that we may not pick up a volume for validation after a
	     * partition if the volume has not yet been demoted (i.e. the
	     * demotion_pending flag is set).  If the volume is awaiting
	     * demotion, it may appear to still have a callback when viewed
	     * "externally" as we do here. This does not violate correctness,
	     * because if an object is referenced in the volume the demotion
	     * will be taken first.
	     *
	     * We do not bother checking the stamps for volumes not in the
	     * hoarding state; when the transition is taken to the hoarding
	     * state the volume will be demoted and the callback cleared
	     * anyway.
	     */
            repvol_iterator next;
            repvol *rv;

            /* one of the following should be this volume. */
            while ((rv = next()) && (nVols < MAX_PIGGY_VALIDATIONS)) {
                /* Check whether the volume is hosted by the same VSG as the
                 * current volume */
                if (repv->vsg != rv->vsg)
                    continue;

                if ((!rv->IsReachable()) || !rv->WantCallBack() ||
                    VV_Cmp(&rv->VVV, &NullVV) == VV_EQ)
                    continue;

                LOG(1000,
                    ("volent::GetVolAttr: packing volume %s, vid %p, vvv:\n",
                     rv->GetName(), rv->GetVolumeId()));
                if (GetLogLevel() >= 1000)
                    FPrintVV(GetLogFile(), &rv->VVV);

                VidList[nVols].Vid = rv->GetVolumeId();
                for (i = 0; i < repv->vsg->MaxVSG(); i++) {
                    *((RPC2_Unsigned *)&((char *)VSBS.SeqBody)[VSBS.SeqLen]) =
                        htonl((&rv->VVV.Versions.Site0)[i]);
                    VSBS.SeqLen += sizeof(RPC2_Unsigned);
                }
                nVols++;
            }

            /*
	     * nVols could be 0 here if someone else got into this routine and
	     * validated while we were descheduled...such as in getmgrp.
	     */
            if (nVols == 0) {
                free(VSBS.SeqBody);
                goto RepExit;
            }

            VFlagBS.MaxSeqLen = nVols;

            LOG(100, ("volent::GetVolAttr: %s, sending %d version stamps\n",
                      name, nVols));

            ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, VFlagvar, VFlagBS,
                            rpc_common.nservers, VENUS_MAXBSLEN);

            /* Make the RPC call. */
            MarinerLog("store::ValidateVols %s [%d]\n", name, nVols);
            MULTI_START_MESSAGE(ViceValidateVols_OP);
            code = (int)MRPC_MakeMulti(ViceValidateVols_OP,
                                       ViceValidateVols_PTR,
                                       rpc_common.nservers, rpc_common.handles,
                                       rpc_common.retcodes, rpc_common.MIp, 0,
                                       0, nVols, VidList, &VSBS, VFlagvar_ptrs);
            MULTI_END_MESSAGE(ViceValidateVols_OP);
            MarinerLog("store::validatevols done\n");

            /* Collate responses from individual servers and decide what to do next. */
            if (IsReplicated())
                code = repv->Collate_NonMutating(m, code);
            else if (IsNonReplicated())
                code = Collate(c, code);
            MULTI_RECORD_STATS(ViceValidateVols_OP);
            free(VSBS.SeqBody);

            if (code) {
                ClearCallBack();
                Recov_BeginTrans();
                RVMLIB_REC_OBJECT(VVV);
                VVV = NullVV;
                Recov_EndTrans(MAXFP);
                goto RepExit;
            }

            unsigned int numVFlags = 0;
            for (i = 0; i < repv->vsg->MaxVSG(); i++) {
                if (rpc_common.hosts[i].s_addr != 0) {
                    if (numVFlags == 0) {
                        /* unset, copy in one response */
                        ARG_UNMARSHALL_BS(VFlagvar, VFlagBS, i);
                        numVFlags = (unsigned)VFlagBS.SeqLen;
                    } else {
                        /* "and" in results from other servers. note VFlagBS.SeqBody == VFlags. */
                        for (int j = 0; j < nVols; j++) {
                            if ((VFlags[j] == -1) ||
                                ((signed char)VFlagvar_bufs[i].SeqBody[j] ==
                                 -1))
                                VFlags[j] = -1;
                            else
                                VFlags[j] &= VFlagvar_bufs[i].SeqBody[j];
                        }
                    }
                }
            }

            LOG(10,
                ("volent::GetVolAttr: ValidateVols (%s), %d vids sent, %d checked\n",
                 name, nVols, numVFlags));

            volent *v;
            Volid volid;
            volid.Realm = realm->Id();

            /* now set status of volumes */
            for (i = 0; i < numVFlags; i++) { /* look up the object */
                volid.Volume = VidList[i].Vid;
                v            = VDB->Find(&volid);
                if (!v) {
                    LOG(0, ("volent::GetVolAttr: couldn't find vid 0x%x\n",
                            VidList[i].Vid));
                    continue;
                }

                CODA_ASSERT(v->IsReadWrite());

                switch (VFlags[i]) {
                case 1: /* OK, callback */
                    if (cbtemp == cbbreaks) {
                        LOG(1000, ("volent::GetVolAttr: vid 0x%x valid\n",
                                   GetVolumeId()));
                        SetCallBack();

                        /* validate cached access rights for the caller */
                        struct dllist_head *p;
                        list_for_each(p, fso_list)
                        {
                            fsobj *f =
                                list_entry_plusplus(p, fsobj, vol_handle);
                            if (!f->IsDir())
                                continue;

                            f->PromoteAcRights(ANYUSER_UID);
                            f->PromoteAcRights(uid);
                        }
                    }
                    break;
                case 0: /* OK, no callback */
                    LOG(0, ("volent::GetVolAttr: vid 0x%x valid, no "
                            "callback\n",
                            GetVolumeId()));
                    ClearCallBack();
                    break;
                default: /* not OK */
                    LOG(1, ("volent::GetVolAttr: vid 0x%x invalid\n",
                            GetVolumeId()));
                    ClearCallBack();
                    Recov_BeginTrans();
                    RVMLIB_REC_OBJECT(VVV);
                    VVV = NullVV;
                    Recov_EndTrans(MAXFP);
                    break;
                }
                v->release();
            }
        }
    }

RepExit:
    if (m)
        m->Put();

    if (c)
        PutConn(&c);
    return (code);
}

/* collate version stamp and callback status out parameters from servers */
void reintvol::UpdateVCBInfo(RPC2_Integer VS, CallBackStatus CBStatus)
{
    if (GetLogLevel() >= 100) {
        fprintf(GetLogFile(),
                "reintvol::UpdateVCBInfo: vid %08x Current VVV:\n", vid);
        FPrintVV(GetLogFile(), &VVV);

        fprintf(GetLogFile(),
                "reintvol::UpdateVCBInfo: Version stamps returned: %d\n", VS);

        fprintf(GetLogFile(),
                "reintvol::UpdateVCBInfo: Callback status returned: %d\n",
                CBStatus);
        fflush(GetLogFile());
    }

    /* This is the single server version of CollateVCB */
    if (CBStatus == CallBackSet) {
        SetCallBack();
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(VVV);
        memset(&VVV.Versions.Site0, 0, sizeof(ViceVersionVector));
        (&(VVV.Versions.Site0))[0] = VS;
        Recov_EndTrans(MAXFP);
    } else {
        ClearCallBack();

        /* check if any of the returned stamp is zero.
            If so, server said stamp invalid. */
        if (VS == 0) {
            Recov_BeginTrans();
            RVMLIB_REC_OBJECT(VVV);
            VVV = NullVV;
            Recov_EndTrans(MAXFP);
        }
    }
}

/* collate version stamp and callback status out parameters from servers */
void repvol::CollateVCB(mgrpent *m, RPC2_Integer *sbufs, CallBackStatus *cbufs)
{
    unsigned int i            = 0;
    CallBackStatus collatedCB = CallBackSet;

    if (GetLogLevel() >= 100) {
        fprintf(GetLogFile(), "repvol::CollateVCB: vid %08x Current VVV:\n",
                vid);
        FPrintVV(GetLogFile(), &VVV);

        fprintf(GetLogFile(), "repvol::CollateVCB: Version stamps returned:");
        for (i = 0; i < vsg->MaxVSG(); i++)
            if (m->rocc.hosts[i].s_addr != 0)
                fprintf(GetLogFile(), " %u", sbufs[i]);

        fprintf(GetLogFile(),
                "\nrepvol::CollateVCB: Callback status returned:");
        for (i = 0; i < vsg->MaxVSG(); i++)
            if (m->rocc.hosts[i].s_addr != 0)
                fprintf(GetLogFile(), " %u", cbufs[i]);

        fprintf(GetLogFile(), "\n");
        fflush(GetLogFile());
    }

    for (i = 0; i < vsg->MaxVSG(); i++) {
        if (m->rocc.hosts[i].s_addr != 0 && (cbufs[i] != CallBackSet))
            collatedCB = NoCallBack;
    }

    if (collatedCB == CallBackSet) {
        SetCallBack();
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(VVV);
        for (i = 0; i < vsg->MaxVSG(); i++)
            if (m->rocc.hosts[i].s_addr != 0)
                (&VVV.Versions.Site0)[i] = sbufs[i];
        Recov_EndTrans(MAXFP);
    } else {
        ClearCallBack();

        /* check if any of the returned stamps are zero.
	   If so, server said stamp invalid. */
        for (i = 0; i < vsg->MaxVSG(); i++)
            if (m->rocc.hosts[i].s_addr != 0 && (sbufs[i] == 0)) {
                Recov_BeginTrans();
                RVMLIB_REC_OBJECT(VVV);
                VVV = NullVV;
                Recov_EndTrans(MAXFP);
                break;
            }
    }

    return;
}

/*
 * Ensure all cached state from volume "vol" is valid.
 * Returns success if it is able to do this, an errno otherwise.
 *
 * Error handling is simple: if one occurs, quit and propagate.
 * There's no volume synchronization because we've already
 * done it.
 *
 * complications:
 * - this can't be called from fsdb::Get (a reasonable place)
 *   unless the target fid is known, because this routine calls
 *   fsdb::Get on potentially everything.
 */
int reintvol::ValidateFSOs()
{
    int code = 0;

    LOG(100, ("reintvol::ValidateFSOs: vid = 0x%x\n", vid));

    vproc *vp = VprocSelf();

    struct dllist_head *p, *next;
    for (p = fso_list.next; p != &fso_list; p = next) {
        fsobj *n = NULL, *f = list_entry_plusplus(p, fsobj, vol_handle);
        next = p->next;

        if (DYING(f) || (STATUSVALID(f) && (!HAVEDATA(f) || DATAVALID(f))))
            continue;

        if (next != &fso_list) {
            n = list_entry_plusplus(next, fsobj, vol_handle);
            FSO_HOLD(n);
        }

        int whatToGet = 0;
        if (!STATUSVALID(f))
            whatToGet = RC_STATUS;

        if (HAVEDATA(f) && !DATAVALID(f))
            whatToGet |= RC_DATA;

        LOG(100, ("reintvol::ValidateFSOs: vget(%s, %x, %d)\n", FID_(&f->fid),
                  whatToGet, f->stat.Length));

        fsobj *tf = 0;
        code      = FSDB->Get(&tf, &f->fid, vp->u.u_uid, whatToGet);
        FSDB->Put(&tf);

        if (n)
            FSO_RELE(n);

        LOG(100,
            ("reintvol::ValidateFSOs: vget returns %s\n", VenusRetStr(code)));
        if (code == EINCONS)
            k_Purge(&f->fid, 1);
        if (code)
            break;
    }
    return (code);
}

void reintvol::PackVS(int nstamps, RPC2_CountedBS *BS)
{
    BS->SeqLen  = 0;
    BS->SeqBody = (RPC2_ByteSeq)malloc(nstamps * sizeof(RPC2_Integer));

    for (int i = 0; i < nstamps; i++) {
        *((RPC2_Unsigned *)&((char *)BS->SeqBody)[BS->SeqLen]) =
            (&VVV.Versions.Site0)[i];
        BS->SeqLen += sizeof(RPC2_Unsigned);
    }
    return;
}

int reintvol::CallBackBreak()
{
    /*
     * Track vcb's broken for this volume. Total vcb's broken is
     * accumulated in vdb::CallbackBreak.
     */

    int rc = (VCBStatus == CallBackSet);

    if (rc) {
        VCBStatus = NoCallBack;

        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(VVV);
        VVV = NullVV;
        Recov_EndTrans(MAXFP);
    }

    return (rc);
}

void reintvol::ClearCallBack()
{
    VCBStatus = NoCallBack;
}

void reintvol::SetCallBack()
{
    VCBStatus = CallBackSet;
}

int reintvol::WantCallBack()
{
    /*
     * This is a policy module that decides if a volume
     * callback is worth acquiring.  This is a naive policy,
     * with a minimal threshold for files.  One could use
     * CallBackClears as an approximation to the partition
     * rate (p), and CallbackBreaks as an approximation
     * to the mutation rate (m).
     */
    struct dllist_head *p;
    int count = 0;

    if (VCBStatus != NoCallBack)
        return 0;

    /* this used to be (fso_list->count() > 1) */
    list_for_each(p, fso_list)
    {
        if (++count > 1)
            return 1;
    }

    return 0;
}
