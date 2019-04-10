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
 *
 * Implementation of the Venus Repair facility.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <rpc2/rpc2.h>
#include <vice.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <inconsist.h>
#include <copyfile.h>

/* from libal */
#include <prs.h>

/* from librepair */
#include <repio.h>

/* from venus */
#include "comm.h"
#include "fso.h"
#include "local.h"
#include "mariner.h"
#include "mgrp.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vproc.h"
#include "worker.h"
#include "realmdb.h"

/*
 *    New Repair Strategy
 *
 *    The basic idea of the new strategy is to represent an
 *    inconsistent object as a (read-only) directory with children
 *    that map to the rw-replicas of the object.  The view a user has
 *    of the inconsistent object depends upon which mode the volume
 *    containing the object is in (with respect to this user?).  In
 *    normal mode, the view is the same that we have been providing
 *    all along, i.e., a dangling, read-only symbolic link whose
 *    contents encode the fid of the object.  In repair mode, the view
 *    is of a read-only subtree headed by the inconsistent object.
 *
 * The internal implications of this change are the following: *
 *          1. Each volume must keep state as to what mode it is in
 *          (per-user?)  (this state will not persist across restarts;
 *          initial mode will be normal)

 *          2. We must cope with "fake" fsobjs, representing both the
 *          * inconsistent object and the mountpoints which map to the
 *          * rw-replicas
 * */

/* local-repair modification */
/* Attempt the Repair. */
int repvol::Repair(VenusFid *RepairFid, char *RepairFile, uid_t uid,
                   VolumeId *RWVols, int *ReturnCodes)
{
    LOG(0, ("volent::Repair: fid = %s, file = %s, uid = %d\n", FID_(RepairFid),
            RepairFile, uid));
    if (IsUnreachable())
        return ETIMEDOUT;

    if (IsResolving())
        return ERETRY;

    if (1 /* to be replaced by a predicate for not being issued by ASR */)
        return ConnectedRepair(RepairFid, RepairFile, uid, RWVols, ReturnCodes);
    return DisconnectedRepair(RepairFid, RepairFile, uid, RWVols, ReturnCodes);
}

/* Translate RepairFile to cache entry if "REPAIRFILE_BY_FID." */
static int GetRepairF(char *RepairFile, uid_t uid, fsobj **RepairF)
{
    VenusFid RepairFileFid;
    char tmp;
    int code;

    *RepairF = NULL;

    if (sscanf(RepairFile, "@%x.%x.%x@%c", &RepairFileFid.Volume,
               &RepairFileFid.Vnode, &RepairFileFid.Unique, &tmp) != 4)
        return 0;

    /* strrchr should succeed now because sscanf succeeded. */
    char *realmname     = strrchr(RepairFile, '@') + 1;
    Realm *realm        = REALMDB->GetRealm(realmname);
    RepairFileFid.Realm = realm->Id();

    code = FSDB->Get(RepairF, &RepairFileFid, uid, RC_DATA);

    realm->PutRef();

    if (code) {
        LOG(0, ("GetRepairF: fsdb::Get failed with code: %d\n", code));
        return code;
    }

    if (!(*RepairF)->IsFile()) {
        FSDB->Put(RepairF);
        LOG(0, ("GetRepairF: wasn't a file!\n"));
        return EINVAL;
    }
    return 0;
}

int repvol::ConnectedRepair(VenusFid *RepairFid, char *RepairFile, uid_t uid,
                            VolumeId *RWVols, int *ReturnCodes)
{
    int code = 0, i, j, fd = -1, localFake = 0;
    int *LCarr     = NULL; /* repLC, mvLC */
    fsobj *RepairF = NULL, *local = NULL;
    VenusFid *rFid        = NULL;
    VenusFid *fidarr      = NULL; /* entryFid, mvFid, mvPFid */
    struct listhdr *hlist = NULL, *l = NULL;
    dlist CMLappends;
    ViceStoreId sid;

    memset(ReturnCodes, 0, VSG_MEMBERS * sizeof(int));
    memset(RWVols, 0, VSG_MEMBERS * sizeof(VolumeId));
    for (i = 0; i < VSG_MEMBERS; i++)
        if (volreps[i])
            RWVols[i] = volreps[i]->GetVolumeId();

    /* Verify that RepairFid is inconsistent. */
    {
        fsobj *f = NULL;

        code = FSDB->Get(&f, RepairFid, uid, RC_STATUS, NULL, NULL, NULL, 1);

        CODA_ASSERT(f); //would be ridiculous to fail here
        if (code || (!f->IsFake() && !f->IsToBeRepaired())) {
            if (code == 0) {
                eprint("Repair: %s (%s) consistent\n", f->GetComp(),
                       FID_(RepairFid));
                LOG(0, ("repvol::Repair: %s (%s) consistent\n", f->GetComp(),
                        FID_(RepairFid)));
                code = EINVAL; /* XXX -JJK */
            }
            LOG(0, ("repvol::Repair: %s (%s) fsdb::Get failed with code %d\n",
                    f->GetComp(), FID_(RepairFid), code));
            FSDB->Put(&f);
            return (code);
        }

        localFake = f->IsToBeRepaired();

        rFid = RepairFid;
        FSDB->Put(&f);
    }

    LOG(0, ("repvol::Repair: (%s) inconsistent!\n", FID_(RepairFid)));

    /* Flush all COP2 entries. */
    /* This would NOT be necessary if ViceRepair took
     * a "PiggyCOP2" parameter! */
    {
        code = FlushCOP2();
        if (code != 0) {
            LOG(0, ("repvol::ConnectedRepair: FlushCOP2 failed with code %d!\n",
                    code));
            return (code);
        }
    }

    code = GetRepairF(RepairFile, uid, &RepairF);
    if (code) {
        LOG(0, ("repvol::ConnectedRepair: GetRepairF failed with code %d!\n",
                code));
        return code;
    }

    Recov_BeginTrans();
    Recov_GenerateStoreId(&sid);
    Recov_EndTrans(MAXFP);

    mgrpent *m = 0;

    /* Acquire an Mgroup. */
    code = GetMgrp(&m, uid);
    if (code != 0) {
        LOG(0,
            ("repvol::ConnectedRepair: GetMgrp failed with code %d!\n", code));
        goto Exit;
    }

    LOG(0, ("repvol::Repair: (%s) attempting COP1!\n", FID_(RepairFid)));

    /* The COP1 call. */
    ViceVersionVector UpdateSet;
    {
        /* Compute template VV. */
        ViceVersionVector tvv = NullVV;
        ViceVersionVector *RepairVVs[VSG_MEMBERS];
        memset((void *)RepairVVs, 0,
               VSG_MEMBERS * (int)sizeof(ViceVersionVector *));
        for (i = 0; i < VSG_MEMBERS; i++)
            if (volreps[i]) {
                fsobj *f = 0;
                VenusFid rwfid;
                rwfid.Realm  = volreps[i]->GetRealmId();
                rwfid.Volume = volreps[i]->GetVolumeId();
                rwfid.Vnode  = rFid->Vnode;
                rwfid.Unique = rFid->Unique;
                if (FSDB->Get(&f, &rwfid, uid, RC_STATUS) != 0)
                    continue;
                RepairVVs[i] = &f->stat.VV; /* XXX */
                FSDB->Put(&f);
            }
        GetMaxVV(&tvv, RepairVVs, -2);

        /* Set-up the status block. */
        ViceStatus status;
        memset((void *)&status, 0, (int)sizeof(ViceStatus));
        if (RepairF != 0) {
            status.Length    = RepairF->stat.Length;
            status.Date      = RepairF->stat.Date;
            status.Owner     = RepairF->stat.Owner;
            status.Mode      = RepairF->stat.Mode;
            status.LinkCount = RepairF->stat.LinkCount;
            status.VnodeType = RepairF->stat.VnodeType;
        } else {
            struct stat tstat;
            if (::stat(RepairFile, &tstat) < 0) {
                code = errno;
                LOG(0,
                    ("repvol::Repair: (%s) Failed stat of RepairFile (%s)!\n",
                     FID_(RepairFid), RepairFile));
                goto Exit;
            }

            status.Length       = (RPC2_Unsigned)tstat.st_size;
            status.Date         = (Date_t)tstat.st_mtime;
            RPC2_Integer se_uid = tstat.st_uid;
            status.Owner        = (UserId)se_uid;
            status.Mode         = (RPC2_Unsigned)tstat.st_mode & 0777;
            status.LinkCount    = (RPC2_Integer)tstat.st_nlink;
            switch (tstat.st_mode & S_IFMT) {
            case S_IFREG:
                status.VnodeType = File;
                break;

            case S_IFDIR:
                status.VnodeType = Directory;
                break;
#ifdef S_IFLNK
            case S_IFLNK:
                status.VnodeType = SymbolicLink;
                break;
#endif
            default:
                code = EINVAL;
                LOG(0, ("repvol::Repair: (%s) invalid Vnode type!\n",
                        FID_(RepairFid)));
                goto Exit;
            }
        }
        status.DataVersion = (FileVersion)0; /* Anything but -1? -JJK */
        status.VV          = tvv;

        /* A little debugging help. */
        if (GetLogLevel() >= 1) {
            fprintf(GetLogFile(), "Repairing %s:\n", FID_(rFid));
            fprintf(GetLogFile(),
                    "\tIV = %d, VT = %d, LC = %d, LE = %d, DV = %d, DA = %d\n",
                    status.InterfaceVersion, status.VnodeType, status.LinkCount,
                    status.Length, status.DataVersion, status.Date);
            fprintf(GetLogFile(),
                    "\tAU = %d, OW = %d, CB = %d, MA = %x, AA = %x, MO = %o\n",
                    status.Author, status.Owner, status.CallBack,
                    (int)status.MyAccess, (int)status.AnyAccess, status.Mode);
            ViceVersionVector *tvvs[VSG_MEMBERS];
            memset((void *)tvvs, 0,
                   VSG_MEMBERS * (int)sizeof(ViceVersionVector *));
            tvvs[0] = &status.VV;
            VVPrint(GetLogFile(), tvvs);
            fflush(GetLogFile());
        }

        /* Set up the SE descriptor. */
        SE_Descriptor sed;
        memset(&sed, 0, sizeof(SE_Descriptor));
        sed.Tag = SMARTFTP;
        struct SFTP_Descriptor *sei;
        sei                        = &sed.Value.SmartFTPD;
        sei->SeekOffset            = 0;
        sei->hashmark              = 0;
        sei->TransmissionDirection = CLIENTTOSERVER;

        /* and open a safe fd to the containerfile */
        if (RepairF)
            fd = RepairF->data.file->Open(O_RDONLY);
        else
            fd = open(RepairFile, O_RDONLY, (int)V_MODE);

        if (fd < 0) {
            LOG(0, ("repvol::Repair: (%s) failed opening fixfile (%s)!\n",
                    FID_(RepairFid), RepairFile));
            goto Exit;
        }

        sei->Tag              = FILEBYFD;
        sei->FileInfo.ByFD.fd = fd;

        /* For directory conflicts only! (for file conflicts, there is no
	 * fixfile)  If localhost is specified in fixfile, get info for
	 * pruning CML entries.  Must do this here, since later would get
	 * errno 157/ERETRY (Resource temporarily unavailable) */

        LOG(0, ("repvol::Repair: (%s) attempting fixfile parse\n",
                FID_(RepairFid)));
        if (ISDIR(*RepairFid)) {
            int hcount;
            struct repair *rep_ent;

            /* parse input file and obtain internal rep */
            if (repair_getdfile(fd, &hcount, &hlist) < 0) {
                code =
                    errno; /* XXXX - Could use a more meaningful return code here */
                LOG(0,
                    ("repvol::Repair: (%s) repair_getdfile failed: %d, fd = %d\n",
                     FID_(RepairFid), code, fd));
                goto Exit;
            }

            for (i = 0; i < hcount; i++)
                if ((hlist[i].replicaFid.Volume == RepairFid->Volume)) {
                    l = &(hlist[i]);
                    break;
                } /* localhost */

            if (l != NULL) {
                /* found localhost in fixfile */
                rep_ent = l->repairList;

                if (l->repairCount > 0) {
                    LCarr  = (int *)calloc((l->repairCount * 2), sizeof(int));
                    fidarr = (VenusFid *)calloc((l->repairCount * 3),
                                                sizeof(VenusFid));
                    if ((LCarr == NULL) || (fidarr == NULL)) {
                        code = ENOMEM;
                        LOG(0, ("repvol::Repair: (%s) LCarr calloc failed!\n",
                                FID_(RepairFid)));
                        goto Exit;
                    }
                }

                for (unsigned int i = 0; i < l->repairCount; i++) {
                    if ((rep_ent[i].opcode == REPAIR_REMOVEFSL) ||
                        (rep_ent[i].opcode == REPAIR_REMOVED) ||
                        (rep_ent[i].opcode == REPAIR_RENAME))
                    /* Lookup fid of the current CML entry name */
                    {
                        fsobj *e = NULL;

                        /* we can get data because (l != NULL) => _localcache exists */
                        code = FSDB->Get(&local, RepairFid, uid, RC_DATA, NULL,
                                         NULL, NULL, 1);
                        if (code || !local) {
                            LOG(0, ("Repair: FSDB->Get(%s) error: %d",
                                    FID_(RepairFid), rep_ent[i].name));
                            if (local)
                                FSDB->Put(&local);
                            goto Exit;
                        }

                        code = local->Lookup(&e, NULL, rep_ent[i].name, uid,
                                             CLU_CASE_SENSITIVE);
                        if (code) {
                            LOG(0, ("Repair: local(%s)->Lookup(%s) error",
                                    FID_(&local->fid), rep_ent[i].name));
                            goto Exit;
                        }
                        fidarr[i] = e->fid;
                        LCarr[i]  = e->stat.LinkCount;
                        FSDB->Put(&e);

                        if (rep_ent[i].opcode == REPAIR_RENAME) {
                            code = local->Lookup(&e, NULL, rep_ent[i].newname,
                                                 uid, CLU_CASE_SENSITIVE);
                            if (code != 0) {
                                LOG(0, ("Repair: (%s)->Lookup(%s) error",
                                        FID_(&local->fid), rep_ent[i].newname));
                                goto Exit;
                            }
                            fidarr[(l->repairCount + i)]       = e->fid;
                            fidarr[((2 * l->repairCount) + i)] = e->pfid;
                            LCarr[(l->repairCount + i)] = e->stat.LinkCount;
                            FSDB->Put(&e);
                        }
                        FSDB->Put(&local);
                    }
                }
            }
        }

        if (::lseek(fd, 0, SEEK_SET) != 0) {
            code = errno;
            goto Exit;
        }

        /* Make multiple copies of the IN/OUT and OUT parameters. */
        ARG_MARSHALL(IN_OUT_MODE, ViceStatus, statusvar, status, VSG_MEMBERS);
        ARG_MARSHALL(IN_OUT_MODE, SE_Descriptor, sedvar, sed, VSG_MEMBERS);

        /*
  BEGIN_HTML
  <a name="vice"><strong> call the Coda server to perform the actual
  repair actions </strong> </a>
  END_HTML
*/
        LOG(0, ("repvol::Repair: (%s) Attempting RPC call\n", FID_(RepairFid)));

        /* Make the RPC call. */
        MarinerLog("store::Repair (%s)\n", FID_(rFid));
        MULTI_START_MESSAGE(ViceRepair_OP);
        code = MRPC_MakeMulti(ViceRepair_OP, ViceRepair_PTR, VSG_MEMBERS,
                              m->rocc.handles, m->rocc.retcodes, m->rocc.MIp, 0,
                              0, MakeViceFid(rFid), statusvar_ptrs, &sid,
                              sedvar_bufs);
        MULTI_END_MESSAGE(ViceRepair_OP);
        MarinerLog("store::repair done\n");

        /* Collate responses from individual servers and decide what to do next. */
        /* Valid return codes are: {0, EINTR, ETIMEDOUT, ESYNRESOLVE, ERETRY}. */
        code = Collate_COP1(m, code, &UpdateSet);
        if (code == EASYRESOLVE) {
            code = 0;
        }
        MULTI_RECORD_STATS(ViceRepair_OP);
        if (code != 0 && code != ESYNRESOLVE) {
            LOG(0, ("repvol::Repair: (%s) Collate_COP1 failed: %d!\n",
                    FID_(RepairFid), code));
            goto Exit;
        }

        /* Collate ReturnCodes. */
        struct in_addr VSGHosts[VSG_MEMBERS];
        GetHosts(VSGHosts);
        int HostCount = 0; /* for sanity check */
        for (i = 0; i < VSG_MEMBERS; i++)
            if (volreps[i]) {
                for (j = 0; j < VSG_MEMBERS; j++)
                    if (VSGHosts[i].s_addr == m->rocc.hosts[j].s_addr) {
                        ReturnCodes[i] = (m->rocc.retcodes[j] >= 0) ?
                                             m->rocc.retcodes[j] :
                                             ETIMEDOUT;
                        HostCount++;
                        break;
                    }
                if (j == VSG_MEMBERS)
                    ReturnCodes[i] = ETIMEDOUT;
            }
        if (HostCount != m->rocc.HowMany)
            CHOKE("volent::Repair: collate failed");
        if (code != 0) {
            LOG(0, ("repvol::Repair: (%s) can you even get here? code:%d!\n",
                    FID_(RepairFid), code));
            goto Exit;
        }
    }

    /* For directory conflicts only! (for file conflicts, there is no fixfile)
     * Prune CML entries if localhost is specified in fixfile */
    if (ISDIR(*RepairFid) && (l != NULL)) {
        time_t modtime;
        int rc;
        struct repair *rep_ent;

        LOG(0, ("repvol::Repair: (%s) pruning CML from fixfile!\n",
                FID_(RepairFid)));

        /* found localhost in fixfile */
        rep_ent = l->repairList;

        rc = time(&modtime);
        if (rc == (time_t)-1) {
            LOG(0, ("repvol::Repair: (%s) time() failed!\n", FID_(RepairFid)));
            code = errno;
            goto Exit;
        }

        for (unsigned int i = 0; i < l->repairCount; i++) {
            switch (rep_ent[i].opcode) {
            case REPAIR_REMOVEFSL: /* Remove file or (hard) link */
                Recov_BeginTrans();
                CML.cancelFreezes(1);
                code = LogRemove(modtime, uid, rFid, rep_ent[i].name,
                                 &(fidarr[i]), LCarr[i], UNSET_TID);
                CML.cancelFreezes(0);
                Recov_EndTrans(CMFP);
                break;
            case REPAIR_REMOVED: /* Remove dir */
                Recov_BeginTrans();
                CML.cancelFreezes(1);
                code = LogRmdir(modtime, uid, rFid, rep_ent[i].name,
                                &(fidarr[i]), UNSET_TID);
                CML.cancelFreezes(0);
                Recov_EndTrans(CMFP);
                break;
            case REPAIR_SETMODE:
                Recov_BeginTrans();
                CML.cancelFreezes(1);
                code = LogChmod(modtime, uid, rFid,
                                (RPC2_Unsigned)rep_ent[i].parms[0], UNSET_TID);
                CML.cancelFreezes(0);
                Recov_EndTrans(CMFP);
                break;
            case REPAIR_SETOWNER: /* Have to be a sys administrator for this */
                Recov_BeginTrans();
                CML.cancelFreezes(1);
                code = LogChown(modtime, uid, rFid, (UserId)rep_ent[i].parms[0],
                                UNSET_TID);
                CML.cancelFreezes(0);
                Recov_EndTrans(CMFP);
                break;
            case REPAIR_SETMTIME:
                Recov_BeginTrans();
                CML.cancelFreezes(1);
                code = LogUtimes(modtime, uid, rFid,
                                 (Date_t)rep_ent[i].parms[0], UNSET_TID);
                CML.cancelFreezes(0);
                Recov_EndTrans(CMFP);
                break;
            case REPAIR_RENAME:
                Recov_BeginTrans();
                CML.cancelFreezes(1);
                code = LogRename(modtime, uid, rFid, rep_ent[i].name,
                                 &(fidarr[((2 * l->repairCount) + i)]),
                                 rep_ent[i].name, &(fidarr[i]),
                                 &(fidarr[(l->repairCount + i)]),
                                 LCarr[(l->repairCount + i)], UNSET_TID);
                CML.cancelFreezes(0);
                Recov_EndTrans(CMFP);
                break;

                /* These should never occur in the fixfile for the local replica */
            case REPAIR_CREATEF: /* Create file */
            case REPAIR_CREATED: /* Create directory */
            case REPAIR_CREATES: /* Create sym link */
            case REPAIR_CREATEL: /* Create (hard) link */
            case REPAIR_SETACL: /* Set rights */
            case REPAIR_SETNACL: /* Set negative rights */
                LOG(0, ("Unexpected local repair command code (%d)",
                        rep_ent[i].opcode));
                code =
                    EINVAL; /* XXXX - Could use a more meaningful return code here */
                break;

            case REPAIR_REPLICA:
                LOG(0, ("Unexpected REPAIR_REPLICA -- truncated fixfile?"));
                code =
                    EINVAL; /* XXXX - Could use a more meaningful return code here */
                break;

            default:
                LOG(0, ("Unknown local repair command code (%d)",
                        rep_ent[i].opcode));
                code =
                    EINVAL; /* XXXX - Could use a more meaningful return code here */
                break;
            }

            /* we still want to send a COP2 to finalize the repair of the
	   * server-server conflict. */
            // if (code != 0) goto Exit;
        }
    } else if (!ISDIR(*RepairFid) && localFake) { /* local/global file rep */
        char msg[2048];
        /* Walk the cml here, unsetting to_be_repaired on everything.
       * A 'cfs forcereintegrate' should then succeed/clear the conflict,
       * if it doesn't happen on its own. */
        CODA_ASSERT(!CML.DiscardLocalMutation(msg));
        CML.ClearToBeRepaired();
    }

    LOG(0, ("repvol::Repair: (%s) sending COP2!\n", FID_(RepairFid)));
    /* Send the COP2 message.  Don't Piggy!  */
    (void)COP2(m, &sid, &UpdateSet, 1);

Exit:
    if (fd != -1) {
        if (RepairF)
            RepairF->data.file->Close(fd);
        else
            close(fd);
    }

    if (m)
        m->Put();
    FSDB->Put(&RepairF);

    if (LCarr != NULL)
        free(LCarr);
    if (fidarr != NULL)
        free(fidarr);
    if (hlist != NULL)
        free(hlist);

    if ((code == 0) && !localFake) { /* successful server/server */
        /* Purge the fake object. */
        fsobj *f = FSDB->Find(RepairFid);
        if (f != 0) {
            f->Lock(WR);
            Recov_BeginTrans();
            f->flags.fake = 0; /* so we can refetch status before death?? */
            f->Kill();
            Recov_EndTrans(MAXFP);
            FSDB->Put(&f);
        }
        /* Invoke an asynchronous resolve for directories. */
        if (ISDIR(*RepairFid)) {
            ResSubmit(0, RepairFid);
        }
    }

    if (code == ESYNRESOLVE)
        code = EMULTRSLTS; /* "multiple results" */

    return (code);
}

/* disablerepair - unset the volume from the repair state */
int repvol::DisconnectedRepair(VenusFid *RepairFid, char *RepairFile, uid_t uid,
                               VolumeId *RWVols, int *ReturnCodes)
{
    int code       = 0, i;
    fsobj *RepairF = 0;
    ViceStatus status;
    vproc *vp = VprocSelf();
    CODA_ASSERT(vp);

    LOG(0, ("volent::DisconnectedRepair: fid = (%s), file = %s, uid = %d\n",
            FID_(RepairFid), RepairFile, uid));

    VenusFid tpfid;
    tpfid.Realm  = RepairFid->Realm;
    tpfid.Volume = RepairFid->Volume;
    memset(ReturnCodes, 0, VSG_MEMBERS * sizeof(int));
    memset(RWVols, 0, VSG_MEMBERS * sizeof(VolumeId));
    for (i = 0; i < VSG_MEMBERS; i++)
        if (volreps[i])
            RWVols[i] = volreps[i]->GetVolumeId();

    /* Verify that RepairFid is a file fid */
    /* can't repair directories while disconnected */
    if (ISDIR(*RepairFid)) {
        eprint("DisconnectedRepair: (%s) is a dir - cannot repair\n",
               FID_(RepairFid));
        return (EINVAL); /* XXX - PK*/
    }

    /* Verify that RepairFid is inconsistent. */
    {
        fsobj *f = NULL;

        code = FSDB->Get(&f, RepairFid, uid, RC_STATUS, NULL, NULL, NULL, 1);

        CODA_ASSERT(f);
        if (code || (!f->IsFake() && !f->IsToBeRepaired())) {
            if (code == 0) {
                eprint("DisconnectedRepair: %s (%s) consistent\n", f->GetComp(),
                       FID_(RepairFid));
                LOG(0, ("DisconnectedRepair: %s (%s) consistent\n",
                        f->GetComp(), FID_(RepairFid)));
                code = EINVAL; /* XXX -JJK */
            } else
                LOG(0, ("DisconnectedRepair: %s (%s) Get failed with code %d\n",
                        f->GetComp(), FID_(RepairFid), code));
            FSDB->Put(&f);
            return (code);
        }

        /* save the fid of the parent of the inconsistent object */
        tpfid.Vnode  = f->pfid.Vnode;
        tpfid.Unique = f->pfid.Unique;
        FSDB->Put(&f);
    }

    LOG(0, ("DisconnectedRepair: (%s) inconsistent!\n", FID_(RepairFid)));
    /* check rights - can user write the file to be repaired */
    {
        LOG(0, ("DisconnectedRepair: Going to check access control (%s)\n",
                FID_(&tpfid)));
        if (!tpfid.Vnode) {
            LOG(0,
                ("DisconnectedRepair: Parent fid is NULL - cannot check access control\n"));
            return (EACCES);
        }
        /* check the parent's rights for write permission*/
        fsobj *parentf = 0;
        code           = FSDB->Get(&parentf, &tpfid, uid, RC_STATUS);
        if (code == 0) {
            code = parentf->Access(PRSFS_WRITE, C_A_W_OK, vp->u.u_uid);
            if (code) {
                LOG(0, ("DisconnectedRepair: Access disallowed (%s)\n",
                        FID_(&tpfid)));
                FSDB->Put(&parentf);
                return (code);
            }
        } else {
            LOG(0, ("DisconnectedRepair: Couldn't get parent (%s)\n",
                    FID_(&tpfid)));
            if (parentf)
                FSDB->Put(&parentf);
            return (code);
        }
        FSDB->Put(&parentf);
    }
    LOG(0, ("DisconnectedRepair: checking repair file %s\n", RepairFile));
    code = GetRepairF(RepairFile, uid, &RepairF);
    if (code)
        return code;

    /* prepare to fake the call */
    {
        /* Compute template VV. */
        ViceVersionVector tvv = NullVV;
        ViceVersionVector *RepairVVs[VSG_MEMBERS];
        memset(RepairVVs, 0, VSG_MEMBERS * sizeof(ViceVersionVector *));

        for (int i = 0; i < VSG_MEMBERS; i++)
            if (volreps[i]) {
                fsobj *f = 0;
                VenusFid rwfid;
                rwfid.Realm  = volreps[i]->GetRealmId();
                rwfid.Volume = volreps[i]->GetVolumeId();
                rwfid.Vnode  = RepairFid->Vnode;
                rwfid.Unique = RepairFid->Unique;
                if (FSDB->Get(&f, &rwfid, uid, RC_STATUS) != 0)
                    continue;
                RepairVVs[i] = &f->stat.VV; /* XXX */
                if (tpfid.Vnode && f->pfid.Vnode) {
                    CODA_ASSERT(tpfid.Vnode == f->pfid.Vnode);
                    CODA_ASSERT(tpfid.Unique == f->pfid.Unique);
                }
                FSDB->Put(&f);
            }
        GetMaxVV(&tvv, RepairVVs, -2);
        /* don't generate a new storeid yet - LogRepair will do that */

        /* set up status block */
        memset((void *)&status, 0, (int)sizeof(ViceStatus));
        if (RepairF != 0) {
            LOG(0, ("DisconnectedRepair: RepairF found! (%s)\n",
                    FID_(&RepairF->fid)));
            status.Length    = RepairF->stat.Length;
            status.Date      = RepairF->stat.Date;
            status.Owner     = RepairF->stat.Owner;
            status.Mode      = RepairF->stat.Mode;
            status.LinkCount = RepairF->stat.LinkCount;
            status.VnodeType = RepairF->stat.VnodeType;
        } else {
            LOG(0, ("DisconnectedRepair: RepairF not found!\n"));
            struct stat tstat;
            if (::stat(RepairFile, &tstat) < 0) {
                code = errno;
                LOG(0, ("DisconnectedRepair: (%s) stat failed\n",
                        FID_(RepairFid)));
                goto Exit;
            }

            status.Length       = (RPC2_Unsigned)tstat.st_size;
            status.Date         = (Date_t)tstat.st_mtime;
            RPC2_Integer se_uid = (short)tstat.st_uid; /* sign-extend uid! */
            status.Owner        = (UserId)se_uid;
            status.Mode         = (RPC2_Unsigned)tstat.st_mode & 0777;
            status.LinkCount    = (RPC2_Integer)tstat.st_nlink;
            if ((tstat.st_mode & S_IFMT) != S_IFREG) {
                code = EINVAL;
                LOG(0, ("DisconnectedRepair: (%s) not a regular file\n",
                        FID_(RepairFid)));
                goto Exit;
            }
        }
        status.DataVersion = (FileVersion)1; /* Anything but -1? -JJK */
        status.VV          = tvv;
    }

    /* fake the call */
    {
        /* first kill the fake directory if it exists */
        fsobj *f = FSDB->Find(RepairFid);
        if (f != 0) {
            LOG(0, ("DisconnectedRepair: Going to kill %s, refcnt: %d\n",
                    FID_(&f->fid), f->refcnt));

            if (f->IsExpandedObj())
                f->CollapseObject(); /* drop FSO_HOLD ref */

            f->Lock(WR);
            Recov_BeginTrans();
            f->Kill();
            Recov_EndTrans(MAXFP);

            if (f->refcnt > 1) {
                LOG(0,
                    ("DisconnectedRepair: (%s) has %d active references - cannot repair\n",
                     FID_(RepairFid), f->refcnt));
                /* Put isn't going to release the object so can't call create
	       * Instead of failing, put an informative message on console
	       * and ask user to retry */
                f->ClearRcRights();
                FSDB->Put(&f);
                code = ERETRY;
                goto Exit;
            }
            FSDB->Put(&f);
            /* Ought to flush its descendents too? XXX -PK */
        }
        /* attempt the create now */
        LOG(0, ("DisconnectedRepair: Going to create %s\n", FID_(RepairFid)));
        /* need to get the priority from the vproc pointer */
        f = FSDB->Create(RepairFid, vp->u.u_priority, NULL, NULL);
        /* don't know the component name */
        if (f == 0) {
            UpdateCacheStats(&FSDB->FileAttrStats, NOSPACE,
                             NBLOCKS(sizeof(fsobj)));
            LOG(0,
                ("DisconnectedRepair: Create failed (%s)\n", FID_(RepairFid)));
            code = ENOSPC;
            goto Exit;
        }

        Date_t Mtime = Vtime();

        LOG(0, ("DisconnectedRepair: Going to call LocalRepair(%s)\n",
                FID_(RepairFid)));
        Recov_BeginTrans();
        code = LogRepair(Mtime, uid, RepairFid, status.Length, status.Date,
                         status.Owner, status.Mode, 1);
        /*
	    * LogRepair puts a ViceRepair_OP record into the CML and it
	    * will be reintegrated to the servers at the end of the ASR
	    * execution. During the reintegration process on the server,
	    * a ViceRepair() call will be made, therefore the inconsistent
	    * file object on servers will get the new data and its inconsistent
	    * bit will be cleared. Because the VV of the file object is
	    * incremented as a result, the next FSDB::get() tries to get
	    * it, it will fetch the new clean server version and throw
	    * away the local fakeified object.
	    */
        if (code == 0)
            code = LocalRepair(
                f, &status, RepairF ? RepairF->data.file->Name() : RepairFile,
                &tpfid);
        Recov_EndTrans(DMFP);

        if (code != 0) {
            /* kill the object? - XXX */
            Recov_BeginTrans();
            f->Kill();
            Recov_EndTrans(MAXFP);
        }
        FSDB->Put(&f);
    }

Exit:
    if (RepairF)
        FSDB->Put(&RepairF);

    LOG(0, ("DisconnectedRepair: returns %u\n", code));
    return (code);
}

/* MUST be called from within a transaction */
int repvol::LocalRepair(fsobj *f, ViceStatus *status, char *fname,
                        VenusFid *pfid)
{
    LOG(100, ("LocalRepair: %s local file %s \n", FID_(&f->fid), fname));
    RVMLIB_REC_OBJECT(*f);

    f->stat.VnodeType   = status->VnodeType;
    f->stat.LinkCount   = status->LinkCount; /* XXX - this could be wrong */
    f->stat.Length      = status->Length;
    f->stat.DataVersion = status->DataVersion;
    f->stat.VV          = status->VV;
    f->stat.Date        = status->Date;
    f->stat.Owner       = status->Owner;
    f->stat.Author      = status->Author;
    f->stat.Mode        = status->Mode;

    f->Matriculate();

    /* for now the parent pointers are just set to NULL */
    if (pfid)
        f->pfid = *pfid;
    f->pfso = NULL;

    /* now store the new contents of the file */
    {
        RVMLIB_REC_OBJECT(f->cf);
        f->data.file = &f->cf;
        f->data.file->Create();

        int srcfd = open(fname, O_RDONLY | O_BINARY, V_MODE);
        CODA_ASSERT(srcfd);
        LOG(0, ("LocalRepair: Going to open %s\n", f->data.file->Name()));
        int tgtfd =
            open(f->data.file->Name(), O_WRONLY | O_TRUNC | O_BINARY, V_MODE);
        CODA_ASSERT(tgtfd > 0);
        int rc;
        struct stat stbuf;

        rc = copyfile(srcfd, tgtfd);
        CODA_ASSERT(rc == 0);

        rc = fstat(tgtfd, &stbuf);
        CODA_ASSERT(rc == 0);

        rc = close(srcfd);
        CODA_ASSERT(rc == 0);

        rc = close(tgtfd);
        CODA_ASSERT(rc == 0);

        if ((unsigned long)stbuf.st_size != status->Length) {
            LOG(0,
                ("LocalRepair: Length mismatch - actual stored %u bytes, expected %u bytes\n",
                 stbuf.st_size, status->Length));
            return (EIO); /* XXX - what else can we return? */
        }
        f->data.file->SetLength((unsigned int)stbuf.st_size);
    }

    /* set the flags of the object before returning */
    f->flags.dirty = 1;

    return (0);
}

/* Enable ASR invocation for this volume (as a volume service)  */
void reintvol::EnableASR(uid_t uid)
{
    LOG(100, ("reintvol::EnableASR: vol = %x, uid = %d\n", vid, uid));

    /* Place volume in "repair mode." */
    if (IsASREnabled())
        LOG(0, ("volent::EnableASR: ASR for %x already enabled", vid));

    flags.enable_asrinvocation = 1;
}

int reintvol::DisableASR(uid_t uid)
{
    LOG(100, ("reintvol::DisableASR: vol = %x, uid = %d\n", vid, uid));

    if (asr_running())
        return EBUSY;

    if (!IsASREnabled())
        LOG(0, ("volent::DisableASR: ASR for %x already disabled", vid));

    flags.enable_asrinvocation = 0;

    return 0;
}

/* Allow ASR invocation for this volume (this is user's permission). */
int reintvol::AllowASR(uid_t uid)
{
    LOG(100, ("reintvol::AllowASR: vol = %x, uid = %d\n", vid, uid));

    /* Place volume in "repair mode." */
    if (IsASRAllowed())
        LOG(0, ("volent::AllowASR: ASR for %x already allowed", vid));

    flags.allow_asrinvocation = 1;

    return 0;
}

int reintvol::DisallowASR(uid_t uid)
{
    LOG(100, ("reintvol::DisallowASR: vol = %x, uid = %d\n", vid, uid));

    if (asr_running())
        return EBUSY;

    if (!IsASRAllowed())
        LOG(0, ("volent::DisallowASR: ASR for %x already disallowed", vid));

    flags.allow_asrinvocation = 0;

    return 0;
}

void reintvol::lock_asr()
{
    CODA_ASSERT(flags.asr_running == 0);
    flags.asr_running = 1;
}

void reintvol::unlock_asr()
{
    CODA_ASSERT(flags.asr_running == 1);
    flags.asr_running = 0;
}

void reintvol::asr_pgid(pid_t new_pgid)
{
    CODA_ASSERT(flags.asr_running == 1);
    pgid = new_pgid;
}
