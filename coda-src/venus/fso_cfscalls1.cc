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
              Copyright (c) 2002-2003 Intel Corporation

#*/

/*
 *
 *    CFS calls1.
 *
 *    ToDo:
 *       1. All mutating Vice calls should have the following IN arguments:
 *            NewSid, NewMutator (implicit from connection), NewMtime,
 *            OldVV and DataVersion (for each object), NewStatus (for each object)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>

#include <rpc2/rpc2.h>
/* interfaces */
#include <vice.h>

#ifdef __cplusplus
}
#endif

#include "comm.h"
#include "fso.h"
#include "mariner.h"
#include "mgrp.h"
#include "venuscb.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"

/*  *****  Remove  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalRemove(Date_t Mtime, char *name, fsobj *target_fso)
{
    /* Update parent status. */
    {
        /* Delete the target name from the directory. */
        dir_Delete(name);

        /* Update the status to reflect the delete. */
        RVMLIB_REC_OBJECT(stat);
        stat.DataVersion++;
        stat.Length = dir_Length();
        stat.Date   = Mtime;
    }

    /* Update the target status. */
    {
        RVMLIB_REC_OBJECT(target_fso->stat);
        target_fso->stat.LinkCount--;
        if (target_fso->stat.LinkCount == 0) {
            UpdateCacheStats(&FSDB->FileAttrStats, REMOVE,
                             NBLOCKS(sizeof(fsobj)));
            UpdateCacheStats(&FSDB->FileDataStats, REMOVE, BLOCKS(target_fso));
            target_fso->Kill();
        } else {
            target_fso->stat.DataVersion++;
            target_fso->DetachHdbBindings();
        }
    }
}

/* local-repair modification */
int fsobj::DisconnectedRemove(Date_t Mtime, uid_t uid, char *name,
                              fsobj *target_fso, int prepend)
{
    int code = 0;
    repvol *rv;

    if (!(vol->IsReadWrite()))
        return ETIMEDOUT;
    rv = (repvol *)vol;

    Recov_BeginTrans();
    code = rv->LogRemove(Mtime, uid, &fid, name, &target_fso->fid,
                         target_fso->stat.LinkCount, prepend);

    if (code == 0 && prepend == 0)
        /* This MUST update second-class state! */
        /* It's already been updated if we're 'prepending',
	     * which basically means it is a repair-related operation,
	     * and doing it again would trigger an assertion. */
        LocalRemove(Mtime, name, target_fso);
    Recov_EndTrans(DMFP);

    return (code);
}

/* local-repair modification */
int fsobj::Remove(char *name, fsobj *target_fso, uid_t uid)
{
    LOG(10, ("fsobj::Remove: (%s, %s), uid = %d\n", GetComp(), name, uid));

    int code     = 0;
    Date_t Mtime = Vtime();

    code = DisconnectedRemove(Mtime, uid, name, target_fso);

    if (code != 0) {
        Demote();
        target_fso->Demote();
    }
    return (code);
}

/*  *****  Link  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalLink(Date_t Mtime, char *name, fsobj *source_fso)
{
    /* Update parent status. */
    {
        /* Add the new <name, fid> to the directory. */
        dir_Create(name, &source_fso->fid);

        /* Update the status to reflect the create. */
        RVMLIB_REC_OBJECT(stat);
        stat.DataVersion++;
        stat.Length = dir_Length();
        stat.Date   = Mtime;
        if (source_fso->IsDir())
            stat.LinkCount++;
        DemoteHdbBindings(); /* in case an expansion would now be satisfied! */
    }

    /* Update source status. */
    {
        RVMLIB_REC_OBJECT(source_fso->stat);
        /*    source_fso->stat.DataVersion++;*/
        source_fso->stat.LinkCount++;
    }
}

/* local-repair modification */
int fsobj::DisconnectedLink(Date_t Mtime, uid_t uid, char *name,
                            fsobj *source_fso, int prepend)
{
    int code = 0;
    repvol *rv;

    if (!(vol->IsReadWrite()))
        return ETIMEDOUT;
    rv = (repvol *)vol;

    Recov_BeginTrans();
    code = rv->LogLink(Mtime, uid, &fid, name, &source_fso->fid, prepend);

    if (code == 0 && prepend == 0)
        /* This MUST update second-class state! */
        /* It's already been updated if we're 'prepending',
	     * which basically means it is a repair-related operation,
	     * and doing it again would trigger an assertion. */
        LocalLink(Mtime, name, source_fso);
    Recov_EndTrans(DMFP);

    return (code);
}

/* local-repair modification */
int fsobj::Link(char *name, fsobj *source_fso, uid_t uid)
{
    LOG(10, ("fsobj::Link: (%s/%s, %s), uid = %d\n", GetComp(),
             source_fso->comp, name, uid));

    int code     = 0;
    Date_t Mtime = Vtime();

    code = DisconnectedLink(Mtime, uid, name, source_fso);

    if (code != 0) {
        Demote();
        source_fso->Demote();
    }
    return (code);
}

/*  *****  Rename  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalRename(Date_t Mtime, fsobj *s_parent_fso, char *s_name,
                        fsobj *s_fso, char *t_name, fsobj *t_fso)
{
    int SameParent   = (s_parent_fso == this);
    int TargetExists = (t_fso != 0);

    /* Update local status. */
    {
        RVMLIB_REC_OBJECT(stat);
        if (!SameParent)
            RVMLIB_REC_OBJECT(s_parent_fso->stat);
        RVMLIB_REC_OBJECT(s_fso->stat);
        if (TargetExists)
            RVMLIB_REC_OBJECT(t_fso->stat);

        /*Remove the source <name, fid> from its directory. */
        s_parent_fso->dir_Delete(s_name);

        /* Remove the target <name, fid> from its directory (if it exists). */
        if (TargetExists) {
            dir_Delete(t_name);

            t_fso->DetachHdbBindings();
            if (t_fso->IsDir()) {
                stat.LinkCount--;

                /* Delete the target object. */
                UpdateCacheStats(&FSDB->DirAttrStats, REMOVE,
                                 NBLOCKS(sizeof(fsobj)));
                UpdateCacheStats(&FSDB->DirDataStats, REMOVE, BLOCKS(t_fso));
                t_fso->Kill();
            } else {
                /* Update the target status. */
                t_fso->stat.LinkCount--;
                if (t_fso->stat.LinkCount == 0) {
                    UpdateCacheStats(&FSDB->FileAttrStats, REMOVE,
                                     NBLOCKS(sizeof(fsobj)));
                    UpdateCacheStats(&FSDB->FileDataStats, REMOVE,
                                     BLOCKS(t_fso));
                    t_fso->Kill();
                } else {
                    t_fso->stat.DataVersion++;
                }
            }
        }

        /* Create the target <name, fid> in the target directory. */
        dir_Create(t_name, &s_fso->fid);

        /* Alter ".." entry in source if necessary. */
        if (!SameParent && s_fso->IsDir()) {
            s_fso->dir_Delete("..");
            s_parent_fso->stat.LinkCount--;
            s_fso->dir_Create("..", &fid);
            stat.LinkCount++;
        }

        /* Update parents' status to reflect the create(s) and delete(s). */
        stat.DataVersion++;
        stat.Length = dir_Length();
        stat.Date   = Mtime;
        if (SameParent) {
            DemoteHdbBindings(); /* in case an expansion would now be satisfied! */
        } else {
            s_parent_fso->stat.DataVersion++;
            s_parent_fso->stat.Length = s_parent_fso->dir_Length();
            s_parent_fso->stat.Date   = Mtime;
        }

        /* Update the source status to reflect the rename and possible create/delete. */
        if (t_name && !STREQ(s_fso->comp, t_name))
            s_fso->SetComp(t_name);
        s_fso->DetachHdbBindings();
        /*    s_fso->stat.DataVersion++;*/
        if (!SameParent)
            s_fso->SetParent(fid.Vnode, fid.Unique);
    }
}

/* local-repair modification */
int fsobj::DisconnectedRename(Date_t Mtime, uid_t uid, fsobj *s_parent_fso,
                              char *s_name, fsobj *s_fso, char *t_name,
                              fsobj *t_fso, int prepend)
{
    int code         = 0;
    int TargetExists = (t_fso != 0);
    repvol *rv;

    if (!(vol->IsReadWrite()))
        return ETIMEDOUT;
    rv = (repvol *)vol;

    Recov_BeginTrans();
    code = rv->LogRename(Mtime, uid, &s_parent_fso->fid, s_name, &fid, t_name,
                         &s_fso->fid, (TargetExists ? &t_fso->fid : &NullFid),
                         (TargetExists ? t_fso->stat.LinkCount : 0), prepend);

    if (code == 0 && prepend == 0)
        /* This MUST update second-class state! */
        /* It's already been updated if we're 'prepending',
	     * which basically means it is a repair-related operation,
	     * and doing it again would trigger an assertion. */
        LocalRename(Mtime, s_parent_fso, s_name, s_fso, t_name, t_fso);
    Recov_EndTrans(DMFP);

    return (code);
}

/* local-repair modification */
int fsobj::Rename(fsobj *s_parent_fso, char *s_name, fsobj *s_fso, char *t_name,
                  fsobj *t_fso, uid_t uid)
{
    LOG(10, ("fsobj::Rename : (%s/%s, %s/%s), uid = %d\n",
             (s_parent_fso ? s_parent_fso->GetComp() : GetComp()), s_name,
             GetComp(), t_name, uid));

    int code       = 0;
    Date_t Mtime   = Vtime();
    int SameParent = (s_parent_fso == 0);
    if (SameParent)
        s_parent_fso = this;
    int TargetExists = (t_fso != 0);

    code = DisconnectedRename(Mtime, uid, s_parent_fso, s_name, s_fso, t_name,
                              t_fso);

    if (code != 0) {
        Demote();
        if (!SameParent)
            s_parent_fso->Demote();
        s_fso->Demote();
        if (TargetExists)
            t_fso->Demote();
    }
    return (code);
}

/*  *****  Mkdir  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalMkdir(Date_t Mtime, fsobj *target_fso, char *name, uid_t Owner,
                       unsigned short Mode)
{
    /* Update parent status. */
    {
        /* Add the new <name, fid> to the directory. */
        dir_Create(name, &target_fso->fid);

        /* Update the status to reflect the create. */
        RVMLIB_REC_OBJECT(stat);
        stat.DataVersion++;
        stat.Length = dir_Length();
        stat.Date   = Mtime;
        stat.LinkCount++;
    }

    /* Set target status and data. */
    {
        /* What about ACL? -JJK */
        RVMLIB_REC_OBJECT(*target_fso);
        target_fso->stat.VnodeType   = Directory;
        target_fso->stat.LinkCount   = 2;
        target_fso->stat.Length      = 0;
        target_fso->stat.DataVersion = 1;
        target_fso->stat.Date        = Mtime;
        target_fso->stat.Owner       = Owner;
        target_fso->stat.Mode        = Mode;
        target_fso->AnyUser          = AnyUser;
        memcpy(target_fso->SpecificUser, SpecificUser,
               (CPSIZE * sizeof(AcRights)));
        target_fso->SetParent(fid.Vnode, fid.Unique);

        /* Create the target directory. */
        target_fso->dir_MakeDir();

        target_fso->Matriculate();
        target_fso->Reference();
        target_fso->ComputePriority();
    }
}

/* local-repair modification */
int fsobj::DisconnectedMkdir(Date_t Mtime, uid_t uid, fsobj **t_fso_addr,
                             char *name, unsigned short Mode, int target_pri,
                             int prepend)
{
    int code          = 0;
    fsobj *target_fso = 0;
    VenusFid target_fid;
    repvol *rv;

    if (!(vol->IsReadWrite())) {
        code = ETIMEDOUT;
        goto Exit;
    }
    rv = (repvol *)vol;

    /* Allocate a fid for the new object. */
    /* if we time out, return so we will try again with a local fid. */
    code = rv->AllocFid(Directory, &target_fid, uid);
    if (code != 0)
        goto Exit;

    /* Allocate the fsobj. */
    target_fso = FSDB->Create(&target_fid, target_pri, name, &fid);
    if (target_fso == 0) {
        UpdateCacheStats(&FSDB->DirAttrStats, NOSPACE, NBLOCKS(sizeof(fsobj)));
        code = ENOSPC;
        goto Exit;
    }
    UpdateCacheStats(&FSDB->DirAttrStats, CREATE, NBLOCKS(sizeof(fsobj)));

    Recov_BeginTrans();
    code =
        rv->LogMkdir(Mtime, uid, &fid, name, &target_fso->fid, Mode, prepend);

    if (code == 0 && prepend == 0) {
        /* This MUST update second-class state! */
        /* It's already been updated if we're 'prepending',
	     * which basically means it is a repair-related operation,
	     * and doing it again would trigger an assertion. */
        LocalMkdir(Mtime, target_fso, name, uid, Mode);

        /* target_fso->stat is not initialized until LocalMkdir */
        RVMLIB_REC_OBJECT(target_fso->CleanStat);
        target_fso->CleanStat.Length = target_fso->stat.Length;
        target_fso->CleanStat.Date   = target_fso->stat.Date;
    }
    Recov_EndTrans(DMFP);

Exit:
    if (code == 0) {
        *t_fso_addr = target_fso;
    } else {
        if (target_fso != 0) {
            FSO_ASSERT(target_fso, !HAVESTATUS(target_fso));
            Recov_BeginTrans();
            target_fso->Kill();
            Recov_EndTrans(DMFP);
            FSDB->Put(&target_fso);
        }
    }
    return (code);
}

/* local-repair modification */
/* Returns target object write-locked (on success). */
int fsobj::Mkdir(char *name, fsobj **target_fso_addr, uid_t uid,
                 unsigned short Mode, int target_pri)
{
    LOG(10, ("fsobj::Mkdir: (%s, %s, %d), uid = %d\n", GetComp(), name,
             target_pri, uid));

    int code         = 0;
    Date_t Mtime     = Vtime();
    *target_fso_addr = 0;

    code =
        DisconnectedMkdir(Mtime, uid, target_fso_addr, name, Mode, target_pri);

    if (code != 0) {
        Demote();
    }
    return (code);
}

/*  *****  Rmdir  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalRmdir(Date_t Mtime, char *name, fsobj *target_fso)
{
    /* Update parent status. */
    {
        /* Delete the target name from the directory.. */
        dir_Delete(name);

        /* Update the status to reflect the delete. */
        RVMLIB_REC_OBJECT(stat);
        stat.DataVersion++;
        stat.Length = dir_Length();
        stat.Date   = Mtime;
        stat.LinkCount--;
    }

    /* Update target status. */
    {
        /* Delete the target object. */
        RVMLIB_REC_OBJECT(target_fso->stat);
        target_fso->stat.LinkCount--;
        target_fso->DetachHdbBindings();
        UpdateCacheStats(&FSDB->DirAttrStats, REMOVE, NBLOCKS(sizeof(fsobj)));
        UpdateCacheStats(&FSDB->DirDataStats, REMOVE, BLOCKS(target_fso));
        target_fso->Kill();
    }
}

/* local-repair modification */
int fsobj::DisconnectedRmdir(Date_t Mtime, uid_t uid, char *name,
                             fsobj *target_fso, int prepend)
{
    int code = 0;
    repvol *rv;

    if (!(vol->IsReadWrite()))
        return ETIMEDOUT;
    rv = (repvol *)vol;

    Recov_BeginTrans();
    code = rv->LogRmdir(Mtime, uid, &fid, name, &target_fso->fid, prepend);

    if (code == 0 && prepend == 0)
        /* This MUST update second-class state! */
        /* It's already been updated if we're 'prepending',
	     * which basically means it is a repair-related operation,
	     * and doing it again would trigger an assertion. */
        LocalRmdir(Mtime, name, target_fso);
    Recov_EndTrans(DMFP);

    return (code);
}

/* local-repair modification */
int fsobj::Rmdir(char *name, fsobj *target_fso, uid_t uid)
{
    LOG(10, ("fsobj::Rmdir: (%s, %s), uid = %d\n", GetComp(), name, uid));

    int code     = 0;
    Date_t Mtime = Vtime();

    code = DisconnectedRmdir(Mtime, uid, name, target_fso);

    if (code != 0) {
        Demote();
        target_fso->Demote();
    }
    return (code);
}

/*  *****  Symlink  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalSymlink(Date_t Mtime, fsobj *target_fso, char *name,
                         char *contents, uid_t Owner, unsigned short Mode)
{
    /* Update parent status. */
    {
        /* Add the new <name, fid> to the directory. */
        dir_Create(name, &target_fso->fid);

        /* Update the status to reflect the create. */
        RVMLIB_REC_OBJECT(stat);
        stat.DataVersion++;
        stat.Length = dir_Length();
        stat.Date   = Mtime;
    }

    /* Set target status. */
    {
        /* Initialize the target fsobj. */
        RVMLIB_REC_OBJECT(*target_fso);
        target_fso->stat.VnodeType   = SymbolicLink;
        target_fso->stat.LinkCount   = 1;
        target_fso->stat.Length      = 0;
        target_fso->stat.DataVersion = 1;
        target_fso->stat.Date        = Mtime;
        target_fso->stat.Owner       = Owner;
        target_fso->stat.Mode        = Mode;
        target_fso->Matriculate();
        target_fso->SetParent(fid.Vnode, fid.Unique);

        /* Write out the link contents. */
        int linklen              = (int)strlen(contents);
        target_fso->stat.Length  = linklen;
        target_fso->data.symlink = (char *)rvmlib_rec_malloc(linklen + 1);
        rvmlib_set_range(target_fso->data.symlink, linklen);
        memcpy(target_fso->data.symlink, contents, linklen);
        UpdateCacheStats(&FSDB->FileDataStats, CREATE, NBLOCKS(linklen));

        target_fso->Reference();
        target_fso->ComputePriority();
    }
}

/* local-repair modification */
int fsobj::DisconnectedSymlink(Date_t Mtime, uid_t uid, fsobj **t_fso_addr,
                               char *name, char *contents, unsigned short Mode,
                               int target_pri, int prepend)
{
    int code            = 0;
    fsobj *target_fso   = 0;
    VenusFid target_fid = NullFid;
    repvol *rv;

    if (!(vol->IsReadWrite())) {
        code = ETIMEDOUT;
        goto Exit;
    }
    rv = (repvol *)vol;

    /* Allocate a fid for the new object. */
    /* if we time out, return so we will try again with a local fid. */
    code = rv->AllocFid(SymbolicLink, &target_fid, uid);
    if (code != 0)
        goto Exit;

    /* Allocate the fsobj. */
    target_fso = FSDB->Create(&target_fid, target_pri, name, &fid);
    if (target_fso == 0) {
        UpdateCacheStats(&FSDB->FileAttrStats, NOSPACE, NBLOCKS(sizeof(fsobj)));
        code = ENOSPC;
        goto Exit;
    }
    UpdateCacheStats(&FSDB->FileAttrStats, CREATE, NBLOCKS(sizeof(fsobj)));

    Recov_BeginTrans();
    code = rv->LogSymlink(Mtime, uid, &fid, name, contents, &target_fso->fid,
                          Mode, prepend);

    if (code == 0 && prepend == 0) {
        /* This MUST update second-class state! */
        /* It's already been updated if we're 'prepending',
	     * which basically means it is a repair-related operation,
	     * and doing it again would trigger an assertion. */
        LocalSymlink(Mtime, target_fso, name, contents, uid, Mode);

        /* target_fso->stat is not initialized until LocalSymlink */
        RVMLIB_REC_OBJECT(target_fso->CleanStat);
        target_fso->CleanStat.Length = target_fso->stat.Length;
        target_fso->CleanStat.Date   = target_fso->stat.Date;
    }
    Recov_EndTrans(DMFP);

Exit:
    if (code == 0) {
        *t_fso_addr = target_fso;
    } else {
        if (target_fso != 0) {
            FSO_ASSERT(target_fso, !HAVESTATUS(target_fso));
            Recov_BeginTrans();
            target_fso->Kill();
            Recov_EndTrans(DMFP);
            FSDB->Put(&target_fso);
        }
    }
    return (code);
}

/* local-repair modification */
int fsobj::Symlink(char *s_name, char *t_name, uid_t uid, unsigned short Mode,
                   int target_pri)
{
    LOG(10, ("fsobj::Symlink: (%s, %s, %s, %d), uid = %d\n", GetComp(), s_name,
             t_name, target_pri, uid));

    int code          = 0;
    Date_t Mtime      = Vtime();
    fsobj *target_fso = 0;

    code = DisconnectedSymlink(Mtime, uid, &target_fso, t_name, s_name, Mode,
                               target_pri);

    if (code == 0) {
        /* Target is NOT an OUT parameter. */
        FSDB->Put(&target_fso);
    } else {
        Demote();
    }
    return (code);
}

/*  *****  SetVV  *****  */

/* This should eventually disappear to be a side-effect of the Repair call! -JJK */
/* Call with object write-locked. */
int fsobj::SetVV(ViceVersionVector *newvv, uid_t uid)
{
    LOG(10, ("fsobj::SetVV: (%s), uid = %d\n", GetComp(), uid));

    int code = 0;

    /* This is a connected-mode only routine! */
    if (!REACHABLE(this))
        return ETIMEDOUT;

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen  = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    if (vol->IsReadWrite()) {
        mgrpent *m = NULL;
        repvol *vp = (repvol *)vol;
        connent *c = NULL;
        struct MRPC_common_params rpc_common;
        struct in_addr ph_addr;
        int ret_code;

        /* Non-replicated volumes don't know about inconsistency */
        if (vol->IsNonReplicated() && IsIncon(*newvv)) {
            return EINVAL;
        }

        code = vp->GetConn(&c, uid, &m, &rpc_common.ph_ix, &ph_addr);
        if (code != 0)
            goto RepExit;

        rpc_common.ph = ntohl(ph_addr.s_addr);

        if (vol->IsReplicated()) {
            rpc_common.nservers = VSG_MEMBERS;
            rpc_common.hosts    = m->rocc.hosts;
            rpc_common.retcodes = m->rocc.retcodes;
            rpc_common.handles  = m->rocc.handles;
            rpc_common.MIp      = m->rocc.MIp;

        } else { // Non-replicated

            rpc_common.nservers = 1;
            rpc_common.hosts    = &ph_addr;
            rpc_common.retcodes = &ret_code;
            rpc_common.handles  = &c->connid;
            rpc_common.MIp      = 0;
        }

        /* The SetVV call. */
        {
            /* Make the RPC call. */
            CFSOP_PRELUDE("store::SetVV %-30s\n", comp, fid);
            MULTI_START_MESSAGE(ViceSetVV_OP);
            code = (int)MRPC_MakeMulti(ViceSetVV_OP, ViceSetVV_PTR,
                                       rpc_common.nservers, rpc_common.handles,
                                       rpc_common.retcodes, rpc_common.MIp, 0,
                                       0, MakeViceFid(&fid), newvv, &PiggyBS);
            MULTI_END_MESSAGE(ViceSetVV_OP);
            CFSOP_POSTLUDE("store::setvv done\n");

            /* Collate responses from individual servers and decide what to do next. */
            if (vol->IsReplicated())
                code = vp->Collate_COP2(m, code);
            else
                code = vp->Collate(c, code);
            MULTI_RECORD_STATS(ViceSetVV_OP);
            if (code != 0)
                goto RepExit;

            /* Finalize COP2 Piggybacking. */
            if (PIGGYCOP2 && vol->IsReplicated())
                vp->ClearCOP2(&PiggyBS);
        }

        /* Do op locally. */
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(stat);
        stat.VV = *newvv;
        Recov_EndTrans(CMFP);

    RepExit:
        if (m)
            m->Put();
        if (c)
            PutConn(&c);
        switch (code) {
        case 0:
            break;

        case ETIMEDOUT:
            code = ERETRY;
            break;

        case ESYNRESOLVE:
        case EINCONS:
            CHOKE("fsobj::SetVV: code = %d", code);
            break;

        default:
            break;
        }
    } else { // !IsReplicated (including non-replicated)
        /* Acquire a Connection. */
        connent *c;
        volrep *vp = (volrep *)vol;
        code       = vp->GetConn(&c, uid);
        if (code != 0)
            goto NonRepExit;

        /* Make the RPC call. */
        CFSOP_PRELUDE("store::SetVV %-30s\n", comp, fid);
        UNI_START_MESSAGE(ViceSetVV_OP);
        code = (int)ViceSetVV(c->connid, MakeViceFid(&fid), newvv, &PiggyBS);
        UNI_END_MESSAGE(ViceSetVV_OP);
        CFSOP_POSTLUDE("store::setvv done\n");

        /* Examine the return code to decide what to do next. */
        code = vp->Collate(c, code);
        UNI_RECORD_STATS(ViceSetVV_OP);
        if (code != 0)
            goto NonRepExit;

        /* Do op locally. */
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(stat);
        stat.VV = *newvv;
        Recov_EndTrans(CMFP);

    NonRepExit:
        PutConn(&c);
    }

    /* Replica control rights are invalid in any case. */
    Demote();

    LOG(0, ("MARIA:  We just SetVV'd.\n"));

    return (code);
}
