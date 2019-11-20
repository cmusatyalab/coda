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
 *    CFS calls0.
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
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <rpc2/rpc2.h>
#include <rpc2/se.h>
/* interfaces */
#include <vice.h>
#include <lka.h>
#include <struct.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include "fso.h"
#include "mariner.h"
#include "mgrp.h"
#include "venuscb.h"
#include "vproc.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "venusvol.h"
#include "worker.h"

static const char partial[8]    = "Partial";
static const char nonpartial[1] = "";

/*  *****  Fetch  *****  */

/* C-stub to jump into the c++ method without compiler warnings */
static void FetchProgressIndicator_stub(void *up, unsigned int offset)
{
    ((fsobj *)up)->FetchProgressIndicator((unsigned long)offset);
}

void fsobj::FetchProgressIndicator(unsigned long offset)
{
    static uint64_t last = 0;
    uint64_t curr;
    uint64_t total_data = GotThisDataEnd - GotThisDataStart;
    uint64_t curr_data  = offset - GotThisDataStart;

    if (total_data != 0) {
        curr = (100.0f * curr_data) / total_data;
    } else {
        curr = 0;
    }

    if (last != curr) {
        MarinerLog("progress::fetching (%s) %lu%% (%luBs/%luBs) [%lu - %lu]\n",
                   GetComp(), curr, curr_data, total_data, GotThisDataStart,
                   GotThisDataEnd);
    }

    last = curr;
}

/* MUST be called from within a transaction */
int fsobj::GetContainerFD(void)
{
    FSO_ASSERT(this, IsFile());

    RVMLIB_REC_OBJECT(data.file);

    /* create a sparse file of the desired size */
    if (!HAVEDATA(this)) {
        RVMLIB_REC_OBJECT(cf);
        data.file = &cf;
        data.file->Create(stat.Length);
    }

    /* and open the containerfile */
    return data.file->Open(O_WRONLY);
}

int fsobj::LookAside(void)
{
    static const char *venusRoot =
        GetVenusConf().get_string_value("mountpoint");
    long cbtemp = cbbreaks;
    int fd = -1, lka_successful = 0;
    char emsg[256];

    CODA_ASSERT(!IsLocalObj() && !IsFake());
    CODA_ASSERT(HAVESTATUS(this) && !HAVEALLDATA(this));

    if (!IsFile() || IsZeroSHA(VenusSHA))
        return 0;

    /* (Satya, 1/03)
       Do lookaside to see if fetch can be short-circuited.
       Assumptions for lookaside:
       (a) we are operating normally (not in a repair)
       (b) we have valid status
       (c) file is a plain file (not sym link, directory, etc.)
       (d) non-zero SHA

       These were all verified in Sanity Checks above;
       Check for replicated volume further ensures we never
       do lookaside during repair

       SHA value is obtained initially from fsobj and used for lookaside.
     */

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(flags);
    flags.fetching = 1;
    fd             = GetContainerFD();
    Recov_EndTrans(CMFP);

    if (fd != -1) {
        memset(emsg, 0, sizeof(emsg));

        /* lookaside always returns success for 0-length files, first of all
	 * there really is nothing to fetch, and secondly we would otherwise
	 * trigger the HAVEALLDATA assert in fsobj::Fetch */
        lka_successful = LookAsideAndFillContainer(
            VenusSHA, fd, stat.Length, venusRoot, emsg, sizeof(emsg) - 1);
        data.file->Close(fd);

        if (emsg[0])
            LOG(0, ("LookAsideAndFillContainer(%s): %s\n", cf.Name(), emsg));

        if (lka_successful)
            LOG(0, ("Lookaside of %s succeeded!\n", cf.Name()));
    }

    /* Note that the container file now has all the data we expected */
    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(flags);
    flags.fetching = 0;

    if (lka_successful)
        cf.SetValidData(cf.Length());
    Recov_EndTrans(CMFP);

    /* If we received any callbacks during the lookaside, the validity of the
     * found data is suspect and we shouldn't set the status to valid */
    if (lka_successful && cbtemp == cbbreaks)
        SetRcRights(RC_DATA | RC_STATUS); /* we now have the data */

    /* we're done, skip out of here */
    return lka_successful;
}

int fsobj::FetchFileRPC(connent *con, ViceStatus *status, uint64_t offset,
                        int64_t len, RPC2_CountedBS *PiggyBS,
                        SE_Descriptor *sed)
{
    int code = 0;
    char prel_str[256];
    const char *partial_sel   = nonpartial;
    int inconok               = !vol->IsReplicated();
    uint viceop               = 0;
    bool fetchpartial_support = con->srv->fetchpartial_support;

    viceop = fetchpartial_support ? ViceFetchPartial_OP : ViceFetch_OP;

    snprintf(prel_str, sizeof(prel_str), "fetch::Fetch%s %%s [%ld]\n",
             partial_sel, BLOCKS(this));

    CFSOP_PRELUDE(prel_str, comp, fid);
    UNI_START_MESSAGE(viceop);

    if (fetchpartial_support) {
        code = ViceFetchPartial(con->connid, MakeViceFid(&fid), &stat.VV,
                                inconok, status, 0, offset, len, PiggyBS, sed);
    } else {
        code = ViceFetch(con->connid, MakeViceFid(&fid), &stat.VV, inconok,
                         status, 0, offset, PiggyBS, sed);
    }

    UNI_END_MESSAGE(viceop);
    CFSOP_POSTLUDE("fetch::Fetch done\n");

    LOG(10, ("fsobj::FetchFileRPC: (%s), pos = %d, count = %d, ret = %d \n",
             GetComp(), offset, len, code));

    /* Examine the return code to decide what to do next. */
    code = vol->Collate(con, code);
    UNI_RECORD_STATS(viceop);

    return code;
}

static int CheckTransferredData(uint64_t pos, int64_t count, uint64_t length,
                                uint64_t transfred, bool vastro)
{
    LOG(10, ("(Multi)ViceFetch: fetched %lu bytes\n", transfred));

    if (pos > length)
        return EINVAL;

    if (count < 0)
        goto TillEndFetching;

    if (!vastro)
        goto TillEndFetching;

    /* Handle the VASTRO case */
    /* If reaching EOF */
    if (pos + count > length) {
        if (transfred != (length - pos)) {
            LOG(0, ("fsobj::Fetch: fetched data amount mismatch (%lu, %lu)\n",
                    transfred, (length - pos)));
            return ERETRY;
        }
    }

    if (transfred != (uint64_t)count) {
        LOG(0, ("fsobj::Fetch: fetched data amount mismatch (%lu, %lu)\n",
                transfred, count));
        return ERETRY;
    }

    return 0;

TillEndFetching:
    /* If not VASTRO or Fetch till the end */
    if ((pos + transfred) != length) {
        // print(GetLogFile());
        LOG(0, ("fsobj::Fetch: fetched file length mismatch (%lu, %lu)\n",
                pos + transfred, length));
        return ERETRY;
    }

    return 0;
}

int fsobj::Fetch(uid_t uid)
{
    return Fetch(uid, 0, -1);
}

int fsobj::Fetch(uid_t uid, uint64_t pos, int64_t count)
{
    int fd   = -1;
    int code = 0;

    LOG(10, ("fsobj::Fetch: (%s), uid = %d\n", GetComp(), uid));

    CODA_ASSERT(!IsLocalObj() && !IsFake());

    /* Sanity checks. */
    {
        /* Better not be disconnected or dirty! */
        FSO_ASSERT(this, (REACHABLE(this) && !DIRTY(this)));

        /* We never fetch data if we don't already have status. */
        if (!HAVESTATUS(this)) {
            print(GetLogFile());
            CHOKE("fsobj::Fetch: !HAVESTATUS");
        }

        /* We never fetch data if we already have the file. */
        if (HAVEALLDATA(this) && !ISVASTRO(this)) {
            print(GetLogFile());
            CHOKE("fsobj::Fetch: HAVEALLDATA");
        }
    }

    /* Status parameters. */
    ViceStatus status;
    memset((void *)&status, 0, (int)sizeof(ViceStatus));

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen  = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* Set up the SE descriptor. */
    SE_Descriptor dummysed;
    memset(&dummysed, 0, sizeof(SE_Descriptor));
    SE_Descriptor *sed = &dummysed;

    uint64_t offset = 0;
    int64_t len     = -1;

    if (ISVASTRO(this)) {
        offset = cachechunksutil::align_to_ccblock_floor(pos);
        len    = cachechunksutil::align_to_ccblock_ceil(pos + count) - offset;

        /* If reading out-of-bound read missing file part */
        if ((offset + len) > Size())
            len = -1;
    } else if (IsFile()) {
        offset = cf.ConsecutiveValidData();
        len    = -1;
    }

    GotThisDataStart = offset;

    if (len < 0 || (offset + len) > Size())
        GotThisDataEnd = Size();
    else
        GotThisDataEnd = offset + len;

    /* C++ 3.0 whines if the following decls moved closer to use  -- Satya */
    {
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(flags);
        flags.fetching = 1;

        sed->Tag = SMARTFTP;

        sed->XferCB = FetchProgressIndicator_stub;
        sed->userp  = this;

        struct SFTP_Descriptor *sei = &sed->Value.SmartFTPD;
        sei->TransmissionDirection  = SERVERTOCLIENT;
        sei->hashmark               = 0;
        sei->SeekOffset             = offset;
        sei->ByteQuota              = len;
        switch (stat.VnodeType) {
        case File:
            /* and open the containerfile */
            fd = GetContainerFD();
            CODA_ASSERT(fd != -1);

            sei->Tag              = FILEBYFD;
            sei->FileInfo.ByFD.fd = fd;

            break;

            /* I don't know how to lock the DH here, but it should
		       be done. */
        case Directory:
            CODA_ASSERT(!data.dir);

            RVMLIB_REC_OBJECT(data.dir);
            data.dir = (VenusDirData *)rvmlib_rec_malloc(sizeof(VenusDirData));
            CODA_ASSERT(data.dir);
            memset((void *)data.dir, 0, sizeof(VenusDirData));

            {
                /* Make sure length is aligned wrt. DIR_PAGESIZE */
                unsigned long dirlen = (stat.Length + DIR_PAGESIZE - 1) &
                                       ~(DIR_PAGESIZE - 1);

                RVMLIB_REC_OBJECT(*data.dir);
                DH_Alloc(&data.dir->dh, dirlen,
                         GetRvmType() == VM ? DIR_DATA_IN_VM : DIR_DATA_IN_RVM);
                /* DH_Alloc already clears dh to zero */
            }

            sei->Tag                              = FILEINVM;
            sei->FileInfo.ByAddr.vmfile.MaxSeqLen = stat.Length;
            sei->FileInfo.ByAddr.vmfile.SeqBody =
                (RPC2_ByteSeq)(DH_Data(&data.dir->dh));
            break;

        case SymbolicLink:
            CODA_ASSERT(!data.symlink);

            RVMLIB_REC_OBJECT(data.symlink);
            /* Malloc one extra byte in case length is 0 (as for runts)! */
            data.symlink = (char *)rvmlib_rec_malloc((unsigned)stat.Length + 1);
            sei->Tag     = FILEINVM;
            sei->FileInfo.ByAddr.vmfile.MaxSeqLen = stat.Length;
            sei->FileInfo.ByAddr.vmfile.SeqBody   = (RPC2_ByteSeq)data.symlink;
            break;

        case Invalid:
            FSO_ASSERT(this, 0);
        }
        Recov_EndTrans(CMFP);
    }

    long cbtemp = cbbreaks;

    if (vol->IsReplicated()) {
        mgrpent *m = 0;
        connent *c = 0;
        repvol *vp = (repvol *)vol;

        /* Acquire an Mgroup. */
        code = vp->GetMgrp(&m, uid, (PIGGYCOP2 ? &PiggyBS : 0));
        if (code != 0)
            goto RepExit;

        /* We do not piggy the COP2 entries in PiggyBS when we intentionally
         * talk to only a single server when performing weak reintegration or
         * when fetching file data.
         * We can do an explicit COP2 call here to ship the PiggyBS array.  Or
         * simply ignore them and they will eventually be sent automatically or
         * piggied on the next multirpc. --JH */
        if (PiggyBS.SeqLen) {
            code           = vp->COP2(m, &PiggyBS);
            PiggyBS.SeqLen = 0;
        }

        /* The COP:Fetch call. */
        {
            /* Make multiple copies of the IN/OUT and OUT parameters. */
            int ph_ix;
            struct in_addr *phost;

            phost     = m->GetPrimaryHost(&ph_ix);
            srvent *s = GetServer(phost, vol->GetRealmId());
            code      = s->GetConn(&c, uid);
            PutServer(&s);
            if (code != 0)
                goto RepExit;

            /* Fetch the file from the server */
            code = FetchFileRPC(c, &status, offset, len, &PiggyBS, sed);
            if (code != 0)
                goto RepExit;

            {
                unsigned long bytes = sed->Value.SmartFTPD.BytesTransferred;
                code = CheckTransferredData(offset, len, status.Length, bytes,
                                            ISVASTRO(this));

                if (IsFile()) {
                    Recov_BeginTrans();
                    cf.SetValidData(offset, bytes);
                    Recov_EndTrans(CMFP);
                }
            }

            /* Handle failed validations. */
            if (VV_Cmp(&status.VV, &stat.VV) != VV_EQ) {
                if (GetLogLevel() >= 1) {
                    dprint("fsobj::Fetch: failed validation\n");
                    int *r = ((int *)&status.VV);
                    dprint(
                        "\tremote = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
                        r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8],
                        r[9], r[10]);
                    int *l = ((int *)&stat.VV);
                    dprint("\tlocal = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
                           l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7], l[8],
                           l[9], l[10]);
                }
                code = EAGAIN;
            }
        }

        /* Directories might have different sizes on different servers. We
	 * _have_ to discard the data, and start over again if we fetched a
	 * different size than expected. */
        if (!IsFile() && stat.Length != status.Length)
            code = EAGAIN;

        Recov_BeginTrans();
        UpdateStatus(&status, NULL, uid);
        Recov_EndTrans(CMFP);

    RepExit:
        if (c)
            PutConn(&c);
        if (m)
            m->Put();
        switch (code) {
        case 0:
            break;

        case ETIMEDOUT:
        case ESYNRESOLVE:
        case EINCONS:
            code = ERETRY;
            break;

        default:
            break;
        }
    } else {
        /* Acquire a Connection. */
        connent *c;
        volrep *vp = (volrep *)vol;
        code       = vp->GetConn(&c, uid);
        if (code != 0)
            goto NonRepExit;

        /* Make the RPC call. */
        code = FetchFileRPC(c, &status, offset, len, &PiggyBS, sed);
        if (code != 0)
            goto NonRepExit;

        {
            unsigned long bytes = sed->Value.SmartFTPD.BytesTransferred;
            code = CheckTransferredData(offset, len, status.Length, bytes,
                                        ISVASTRO(this));

            if (IsFile()) {
                Recov_BeginTrans();
                cf.SetValidData(offset, bytes);
                Recov_EndTrans(CMFP);
            }
        }

        /* Handle failed validations. */
        if (HAVESTATUS(this) && status.DataVersion != stat.DataVersion) {
            LOG(1, ("fsobj::Fetch: failed validation (%d, %d)\n",
                    status.DataVersion, stat.DataVersion));
            if (GetLogLevel() >= 1) {
                int *r = ((int *)&status.VV);
                dprint("\tremote = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
                       r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8],
                       r[9], r[10]);
                int *l = ((int *)&stat.VV);
                dprint("\tlocal = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
                       l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7], l[8],
                       l[9], l[10]);
            }
            code = EAGAIN;
        }

        Recov_BeginTrans();
        UpdateStatus(&status, NULL, uid);
        Recov_EndTrans(CMFP);

    NonRepExit:
        if (c)
            PutConn(&c);
    }

    if (fd != -1)
        data.file->Close(fd);

    Recov_BeginTrans();
    RVMLIB_REC_OBJECT(flags);
    flags.fetching = 0;

    if (code == 0) {
        if (status.CallBack == CallBackSet && cbtemp == cbbreaks)
            SetRcRights(RC_STATUS | RC_DATA);

        /* Note the presence of data. */
        switch (stat.VnodeType) {
        case File:
            /* File is already `truncated' to the correct length */
            break;

        case Directory:
            rvmlib_set_range(DH_Data(&data.dir->dh), stat.Length);
            break;

        case SymbolicLink:
            rvmlib_set_range(data.symlink, stat.Length);
            break;

        case Invalid:
            FSO_ASSERT(this, 0);
        }
    } else {
        /*
         * Return allocation and truncate. If a file, set the cache
         * file length so that DiscardData releases the correct
         * number of blocks (i.e., the number allocated in fsdb::Get).
         */
        /* when the server responds with EAGAIN, the VersionVector was
	 * changed, so this should effectively be handled like a failed
	 * validation, and we can throw away the data */
        if (HAVEDATA(this) && (!IsFile() || code == EAGAIN))
            DiscardData();

        /* ERETRY makes us drop back to the vproc_vfscalls level, and retry
	 * the whole FSDB->Get operation */
        if (code == EAGAIN)
            code = ERETRY;

        /* Demote existing status. */
        Demote();
    }
    Recov_EndTrans(CMFP);
    return (code);
}

/*  *****  GetAttr/GetAcl  *****  */

int fsobj::GetAttr(uid_t uid, RPC2_BoundedBS *acl)
{
    static int PiggyValidations = GetVenusConf().get_int_value("validateattrs");
    repvol *vp                  = (repvol *)vol;
    connent *c                  = NULL;
    mgrpent *m                  = NULL;
    int code                    = 0;
    int ret_code                = 0;
    int getacl                  = (acl != 0);
    int inconok                 = !vol->IsReplicated();
    const char *prel_str        = getacl ? "fetch::GetACL %s\n" :
                                    "fetch::GetAttr %s\n";
    const char *post_str = getacl ? "fetch::GetACL done\n" :
                                    "fetch::GetAttr done\n";
    unsigned int i = 0;
    struct MRPC_common_params rpc_common;
    struct in_addr ph_addr;

    /*
     * these fields are for tracking vcb acquisition.  Since we
     * use vcbs on replicated volumes only, the data collection
     * goes in this branch of GetAttr.
     */
    int nchecked = 0, nfailed = 0;
    long cbtemp = cbbreaks;
    char val_prel_str[256];
    int asy_resolve = 0;

    LOG(10, ("fsobj::GetAttr: (%s), uid = %d\n", GetComp(), uid));

    CODA_ASSERT(!IsLocalObj());

    /* Sanity checks. */
    {
        /* Better not be disconnected or dirty! */
        FSO_ASSERT(this, (REACHABLE(this) && !DIRTY(this)));
    }

    /* Dummy argument for ACL */
    RPC2_BoundedBS dummybs;
    dummybs.MaxSeqLen = 0;
    dummybs.SeqLen    = 0;
    if (!getacl)
        acl = &dummybs;

    /* Status parameters. */
    ViceStatus status;

    /* SHA value (not always used) */
    RPC2_BoundedBS mysha;
    mysha.SeqBody   = (RPC2_Byte *)VenusSHA;
    mysha.MaxSeqLen = SHA_DIGEST_LENGTH;
    mysha.SeqLen    = 0;

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen  = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    if (vol->IsReadWrite()) {
        code = vp->GetConn(&c, uid, &m, &rpc_common.ph_ix, &ph_addr);
        if (code != 0)
            goto RepExit;

        cbtemp = cbbreaks;

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

        nchecked++; /* we're going to check at least the primary fid */
        {
            /* unneccesary in validation case but it beats duplicating code. */
            if (acl->MaxSeqLen > VENUS_MAXBSLEN)
                CHOKE("fsobj::GetAttr: BS len too large (%d)", acl->MaxSeqLen);
            ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, aclvar, *acl,
                            rpc_common.nservers, VENUS_MAXBSLEN);
            ARG_MARSHALL(OUT_MODE, ViceStatus, statusvar, status,
                         rpc_common.nservers);

            ARG_MARSHALL_BS(OUT_MODE, RPC2_BoundedBS, myshavar, mysha,
                            rpc_common.nservers, SHA_DIGEST_LENGTH);

            if (HAVESTATUS(this) && !getacl) {
                ViceFidAndVV FAVs[MAX_PIGGY_VALIDATIONS];

                /*
		 * pack piggyback fids and version vectors from this volume.
		 * We exclude busy objects because if their validation fails,
		 * they end up in the same state (demoted) that they are now.
		 * A nice optimization would be to pack them highest priority
		 * first, from the priority queue. Unfortunately this may not
		 * result in the most efficient packing because only
		 * _replaceable_ objects are in the priority queue (duh).
		 * So others that need to be checked may be missed,
		 * resulting in more rpcs!
		 */
                int numPiggyFids = 0;

                struct dllist_head *p;
                list_for_each(p, vol->fso_list)
                {
                    fsobj *f;

                    if (numPiggyFids >= PiggyValidations)
                        break;

                    f = list_entry_plusplus(p, fsobj, vol_handle);

                    if (!HAVESTATUS(f) || STATUSVALID(f) || DYING(f) ||
                        FID_EQ(&f->fid, &fid) || f->IsLocalObj() || BUSY(f) ||
                        DIRTY(f))
                        continue;

                    /* paranoia check */
                    FSO_ASSERT(this, f->vol->IsReadWrite());

                    LOG(1000,
                        ("fsobj::GetAttr: packing piggy fid (%s) comp = %s\n",
                         FID_(&f->fid), f->comp));

                    FAVs[numPiggyFids].Fid = *MakeViceFid(&f->fid);
                    FAVs[numPiggyFids].VV  = f->stat.VV;
                    numPiggyFids++;
                }

                /* we need the fids in order */
                (void)qsort((char *)FAVs, numPiggyFids, sizeof(ViceFidAndVV),
                            (int (*)(const void *, const void *))FAV_Compare);

                /*
                 * another OUT parameter. We don't use an array here
                 * because each char would be embedded in a struct that
                 * would be longword aligned. Ugh.
                 */
                char VFlags[MAX_PIGGY_VALIDATIONS];
                RPC2_BoundedBS VFlagBS;

                VFlagBS.MaxSeqLen = MAX_PIGGY_VALIDATIONS;
                VFlagBS.SeqLen    = 0;
                VFlagBS.SeqBody   = (RPC2_ByteSeq)VFlags;

                ARG_MARSHALL_BS(IN_OUT_MODE, RPC2_BoundedBS, VFlagvar, VFlagBS,
                                rpc_common.nservers, MAX_PIGGY_VALIDATIONS);

                /* make the RPC */
                sprintf(val_prel_str,
                        "fetch::ValidateAttrsPlusSHA %%s(%s) [%d]\n",
                        FID_(&fid), numPiggyFids);
                CFSOP_PRELUDE(val_prel_str, comp, fid);
                MULTI_START_MESSAGE(ViceValidateAttrsPlusSHA_OP);
                code = (int)MRPC_MakeMulti(
                    ViceValidateAttrsPlusSHA_OP, ViceValidateAttrsPlusSHA_PTR,
                    rpc_common.nservers, rpc_common.handles,
                    rpc_common.retcodes, rpc_common.MIp, 0, 0, rpc_common.ph,
                    MakeViceFid(&fid), statusvar_ptrs, myshavar_ptrs,
                    numPiggyFids, FAVs, VFlagvar_ptrs, &PiggyBS);
                MULTI_END_MESSAGE(ViceValidateAttrsPlusSHA_OP);
                CFSOP_POSTLUDE("fetch::ValidateAttrsPlusSHA done\n");

                /* Collate */
                if (vp->IsReplicated()) {
                    code = vp->Collate_NonMutating(m, code);
                } else {
                    code = vp->Collate(c, code);
                }

                MULTI_RECORD_STATS(ViceValidateAttrsPlusSHA_OP);

                if (code == EASYRESOLVE) {
                    asy_resolve = 1;
                    code        = 0;
                } else if (code == 0 || code == ERETRY) {
                    /*
                     * collate flags from vsg members. even if the return
                     * is ERETRY we can (and should) grab the flags.
                     */
                    int numVFlags = 0;

                    for (i = 0; i < rpc_common.nservers; i++)
                        if (rpc_common.hosts[i].s_addr != 0) {
                            if (numVFlags == 0) {
                                /* unset, copy in one response */
                                ARG_UNMARSHALL_BS(VFlagvar, VFlagBS, i);
                                numVFlags = (unsigned)VFlagBS.SeqLen;
                            } else {
                                /*
                                 * "and" in results from other servers.
                                 * Remember that VFlagBS.SeqBody == VFlags.
                                 */
                                for (int j = 0; j < numPiggyFids; j++)
                                    VFlags[j] &= VFlagvar_bufs[i].SeqBody[j];
                            }
                        }

                    LOG(10,
                        ("fsobj::GetAttr: ValidateAttrs (%s), %d fids sent, %d checked\n",
                         GetComp(), numPiggyFids, numVFlags));

                    nchecked += numPiggyFids;
                    /*
                     * now set status of piggybacked objects
                     */
                    for (i = 0; i < numVFlags; i++) {
                        /*
                         * lookup this object. It may have been flushed and
                         * reincarnated as a runt in the while we were out,
                         * so we check status again.
                         */
                        fsobj *pobj;
                        VenusFid vf;
                        MakeVenusFid(&vf, vol->GetRealmId(), &FAVs[i].Fid);

                        pobj = FSDB->Find(&vf);
                        if (pobj) {
                            if (VFlags[i] && HAVESTATUS(pobj)) {
                                LOG(1000,
                                    ("fsobj::GetAttr: ValidateAttrs (%s), fid (%s) valid\n",
                                     pobj->GetComp(), FID_(&FAVs[i].Fid)));
                                /* callbacks broken during validation make
                                 * any positive return codes suspect. */
                                if (cbtemp != cbbreaks)
                                    continue;

                                if (!HAVEALLDATA(pobj))
                                    pobj->SetRcRights(RC_STATUS);
                                else
                                    pobj->SetRcRights(RC_STATUS | RC_DATA);
                                /*
                                 * if the object matched, the access rights
                                 * cached for this object are still good.
                                 */
                                if (pobj->IsDir()) {
                                    pobj->PromoteAcRights(ANYUSER_UID);
                                    pobj->PromoteAcRights(uid);
                                }
                            } else {
                                /* invalidate status (and data) for this object */
                                LOG(1,
                                    ("fsobj::GetAttr: ValidateAttrs (%s), fid (%s) validation failed\n",
                                     pobj->GetComp(), FID_(&FAVs[i].Fid)));

                                if (REPLACEABLE(pobj) && !BUSY(pobj)) {
                                    LOG(1,
                                        ("fsobj::GetAttr: Killing (%s), REPLACEABLE and !BUSY\n",
                                         pobj->GetComp()));
                                    Recov_BeginTrans();
                                    pobj->Kill(0);
                                    Recov_EndTrans(MAXFP);
                                } else
                                    pobj->Demote();

                                nfailed++;
                                /*
                                 * If we have data, it is stale and must be
                                 * discarded, unless someone is writing or
                                 * executing it, or it is a fake directory.
                                 * In that case, we wait and rely on the
                                 * destructor to discard the data.
                                 *
                                 * We don't restart from the beginning,
                                 * since the validation of piggybacked fids
                                 * is a side-effect.
                                 */
                                if (HAVEDATA(pobj) && !ACTIVE(pobj) &&
                                    !pobj->IsFakeDir() &&
                                    !pobj->IsExpandedDir() && !DIRTY(pobj)) {
                                    Recov_BeginTrans();
                                    UpdateCacheStats((IsDir() ?
                                                          &FSDB->DirDataStats :
                                                          &FSDB->FileDataStats),
                                                     REPLACE, BLOCKS(pobj));
                                    pobj->DiscardData();
                                    Recov_EndTrans(MAXFP);
                                }
                            }
                        }
                    }
                }
            } else {
                /* The COP:Fetch call. */
                CFSOP_PRELUDE(prel_str, comp, fid);
                if (getacl) {
                    /* Note: this will cause SHA for this fso to be set to zero
                       because we haven't created a ViceGetACLPlusSHA().  This is
                       probably ok since the only use of this call is for "cfs getacl..."
                       If this proves unacceptable, we'll need to treat the
                       ViceGetACL branch of this code just like ViceValidateAttrs()
                       and ViceGetAttr() branches.   (Satya, 1/03)

                       Side note: ACL's only exist for directory objects, and
                       lookaside only works for file objects, so it really
                       shouldn't matter right now -JH.
                    */

                    MULTI_START_MESSAGE(ViceGetACL_OP);
                    code = (int)MRPC_MakeMulti(
                        ViceGetACL_OP, ViceGetACL_PTR, rpc_common.nservers,
                        rpc_common.handles, rpc_common.retcodes, rpc_common.MIp,
                        0, 0, MakeViceFid(&fid), inconok, aclvar_ptrs,
                        statusvar_ptrs, rpc_common.ph, &PiggyBS);
                    MULTI_END_MESSAGE(ViceGetACL_OP);
                    CFSOP_POSTLUDE(post_str);

                    /* Collate responses from individual servers and decide
                     * what to do next. */
                    if (vp->IsReplicated()) {
                        code = vp->Collate_NonMutating(m, code);
                    } else {
                        code = vp->Collate(c, code);
                    }
                    MULTI_RECORD_STATS(ViceGetACL_OP);
                } else {
                    /* get attributes from replicated servers */
                    LOG(1, ("fsobj::GetAttr: ViceGetAttrPlusSHA(0x%x.%x.%x)\n",
                            fid.Volume, fid.Vnode, fid.Unique));
                    MULTI_START_MESSAGE(ViceGetAttrPlusSHA_OP);
                    code = (int)MRPC_MakeMulti(
                        ViceGetAttrPlusSHA_OP, ViceGetAttrPlusSHA_PTR,
                        rpc_common.nservers, rpc_common.handles,
                        rpc_common.retcodes, rpc_common.MIp, 0, 0,
                        MakeViceFid(&fid), inconok, statusvar_ptrs,
                        myshavar_ptrs, rpc_common.ph, &PiggyBS);
                    MULTI_END_MESSAGE(ViceGetAttrPlusSHA_OP);
                    CFSOP_POSTLUDE(post_str);

                    /* Collate responses from individual servers and decide
                     * what to do next. */
                    if (vp->IsReplicated()) {
                        code = vp->Collate_NonMutating(m, code);
                    } else {
                        code = vp->Collate(c, code);
                    }
                    MULTI_RECORD_STATS(ViceGetAttrPlusSHA_OP);
                }

                if (code == EASYRESOLVE) {
                    asy_resolve = 1;
                    code        = 0;
                }
            }
            if (code != 0)
                goto RepExit;

            /* common code for replicated case */

            int dh_ix = -1;

            /* Finalize COP2 Piggybacking. There's no COP2 for non-replicated volumes */
            if (vol->IsReplicated()) {
                if (PIGGYCOP2)
                    vp->ClearCOP2(&PiggyBS);

                /* Collect the OUT VVs in an array so that they can be checked. */
                ViceVersionVector *vv_ptrs[VSG_MEMBERS];
                for (unsigned int j = 0; j < rpc_common.nservers; j++)
                    vv_ptrs[j] = &((statusvar_ptrs[j])->VV);

                /* Check the version vectors for consistency. */
                code = m->RVVCheck(vv_ptrs, (int)ISDIR(fid));
                if (code == EASYRESOLVE) {
                    asy_resolve = 1;
                    code        = 0;
                }
                if (code != 0)
                    goto RepExit;

                /*
                 * Compute the dominant host set.
                 * The index of a dominant host is returned as a side-effect.
                 */

                dh_ix = -1;
                code  = m->DHCheck(vv_ptrs, rpc_common.ph_ix, &dh_ix, 1);
                if (code != 0)
                    goto RepExit;
            } else {
                dh_ix = 0;
            }

            /* Manually compute the OUT parameters from the mgrpent::GetAttr() call! -JJK */
            if (getacl) {
                ARG_UNMARSHALL_BS(aclvar, *acl, dh_ix);
            }
            ARG_UNMARSHALL(statusvar, status, dh_ix);

            ARG_UNMARSHALL_BS(myshavar, mysha, dh_ix);

            if (GetLogLevel() >= 10 && mysha.SeqLen == SHA_DIGEST_LENGTH) {
                char printbuf[2 * SHA_DIGEST_LENGTH + 1];
                ViceSHAtoHex(VenusSHA, printbuf, sizeof(printbuf));
                dprint("mysha(%d, %d) = %s\n.", mysha.MaxSeqLen, mysha.SeqLen,
                       printbuf);
            }

#if 0 /* XXX: How can this be right? */
	    /* Handle successful validation of fake directory! */
	    if (IsFakeDir() || IsExpandedDir()) { /* XXX:? Adam 5/17/05 */
		LOG(0, ("fsobj::GetAttr: (%s) validated fake directory\n",
			FID_(&fid)));

		Recov_BeginTrans();
		Kill();
		Recov_EndTrans(0);

		code = ERETRY;
		goto RepExit;
	    }
#endif

            /* Handle failed validations. */
            if (HAVESTATUS(this) && VV_Cmp(&status.VV, &stat.VV) != VV_EQ) {
                if (GetLogLevel() >= 1) {
                    dprint("fsobj::GetAttr: failed validation\n");
                    int *r = ((int *)&status.VV);
                    dprint(
                        "\tremote = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
                        r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8],
                        r[9], r[10]);
                    int *l = ((int *)&stat.VV);
                    dprint("\tlocal = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
                           l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7], l[8],
                           l[9], l[10]);
                }

                Demote();
                nfailed++;

                /* If we have data, it is stale and must be discarded. */
                /* Operation MUST be restarted from beginning since, even though this */
                /* fetch was for status-only, the operation MAY be requiring data! */
                if (HAVEDATA(this)) {
                    Recov_BeginTrans();
                    UpdateCacheStats((IsDir() ? &FSDB->DirDataStats :
                                                &FSDB->FileDataStats),
                                     REPLACE, BLOCKS(this));
                    DiscardData();
                    Recov_EndTrans(CMFP);
                    code = ERETRY;

                    goto RepExit;
                }
            }
        }

        if (status.CallBack == CallBackSet && cbtemp == cbbreaks &&
            !asy_resolve) {
            if (!HAVEALLDATA(this))
                SetRcRights(RC_STATUS);
            else
                SetRcRights(RC_STATUS | RC_DATA);
        }

        Recov_BeginTrans();
        UpdateStatus(&status, NULL, uid);

        if (IsFile()) {
            RVMLIB_REC_OBJECT(VenusSHA);
            /* VenusSHA already set by getattr */
            if (mysha.SeqLen != SHA_DIGEST_LENGTH)
                memset(&VenusSHA, 0, SHA_DIGEST_LENGTH);
        }
        Recov_EndTrans(CMFP);

    RepExit:
        if (m)
            m->Put();

        if (c)
            PutConn(&c);

        switch (code) {
        case 0:
            if (asy_resolve)
                vp->ResSubmit(0, &fid);
            break;

        case ESYNRESOLVE:
            vp->ResSubmit(&((VprocSelf())->u.u_resblk), &fid);
            break;

        case EINCONS:
            /* We used to kill inconsistent objects, but that is not a
             * useful thing to do anymore, as the object now simply has
             * its attributes changed and functions as a .localcache
             * object in server/server conflict expansions. -- Adam */
            break;

        case ENXIO:
            /* VNOVOL is mapped to ENXIO, and when ViceValidateAttrs
             * returns this error for all servers, we should get rid of
             * all cached fsobjs for this volume. */
            if (vol) {
                struct dllist_head *p, *next;
                Recov_BeginTrans();
                for (p = vol->fso_list.next; p != &vol->fso_list; p = next) {
                    fsobj *n = NULL, *f;
                    f        = list_entry_plusplus(p, fsobj, vol_handle);

                    /* Kill is scary, so we make sure we keep an active
                     * reference to the ->next object */
                    next = p->next;
                    if (next != &vol->fso_list) {
                        n = list_entry_plusplus(next, fsobj, vol_handle);
                        FSO_HOLD(n);
                    }

                    f->Kill(0);

                    if (n)
                        FSO_RELE(n);
                }
                Recov_EndTrans(CMFP);
            }
            break;

        case ENOENT:
            /* Object no longer exists, discard if possible. */
            Recov_BeginTrans();
            Kill();
            Recov_EndTrans(CMFP);
            break;

        default:
            break;
        }
    } else { // !IsReadWrite()
        /* Acquire a Connection. */
        connent *c = NULL;
        volrep *vp = (volrep *)vol;
        code       = vp->GetConn(&c, uid);
        if (code != 0)
            goto NonRepExit;

        /* Make the RPC call. */
        cbtemp = cbbreaks;
        CFSOP_PRELUDE(prel_str, comp, fid);
        if (getacl) {
            UNI_START_MESSAGE(ViceGetACL_OP);
            code = (int)ViceGetACL(c->connid, MakeViceFid(&fid), inconok, acl,
                                   &status, 0, &PiggyBS);
            UNI_END_MESSAGE(ViceGetACL_OP);
            CFSOP_POSTLUDE(post_str);

            /* Examine the return code to decide what to do next. */
            code = vp->Collate(c, code);
            UNI_RECORD_STATS(ViceGetACL_OP);
        } else {
            RPC2_BoundedBS SHAval;
            SHAval.MaxSeqLen = SHAval.SeqLen = 0;

            UNI_START_MESSAGE(ViceGetAttrPlusSHA_OP);
            code = (int)ViceGetAttrPlusSHA(c->connid, MakeViceFid(&fid),
                                           inconok, &status, &SHAval, 0,
                                           &PiggyBS);
            UNI_END_MESSAGE(ViceGetAttrPlusSHA_OP);
            CFSOP_POSTLUDE(post_str);

            /* Examine the return code to decide what to do next. */
            code = vp->Collate(c, code);
            UNI_RECORD_STATS(ViceGetAttrPlusSHA_OP);
        }
        if (code != 0)
            goto NonRepExit;

        /* Handle failed validations. */
        if (HAVESTATUS(this) && status.DataVersion != stat.DataVersion) {
            LOG(1, ("fsobj::GetAttr: failed validation (%d, %d)\n",
                    status.DataVersion, stat.DataVersion));
            if (GetLogLevel() >= 1) {
                int *r = ((int *)&status.VV);
                dprint("\tremote = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
                       r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8],
                       r[9], r[10]);
                int *l = ((int *)&stat.VV);
                dprint("\tlocal = [%x %x %x %x %x %x %x %x] [%x %x] [%x]\n",
                       l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7], l[8],
                       l[9], l[10]);
            }

            Demote();

            /* If we have data, it is stale and must be discarded. */
            /* Operation MUST be restarted from beginning since, even though
             * this fetch was for status-only, the operation MAY be requiring
             * data! */
            if (HAVEDATA(this)) {
                Recov_BeginTrans();
                UpdateCacheStats((IsDir() ? &FSDB->DirDataStats :
                                            &FSDB->FileDataStats),
                                 REPLACE, BLOCKS(this));
                DiscardData();
                code = ERETRY;
                Recov_EndTrans(CMFP);

                goto NonRepExit;
            }
        }

        if (status.CallBack == CallBackSet && cbtemp == cbbreaks) {
            if (!HAVEALLDATA(this))
                SetRcRights(RC_STATUS);
            else
                SetRcRights(RC_STATUS | RC_DATA);
        }

        Recov_BeginTrans();
        UpdateStatus(&status, NULL, uid);

        if (IsFile()) {
            RVMLIB_REC_OBJECT(VenusSHA);
            /* VenusSHA already was set by getattr */
            if (mysha.SeqLen != SHA_DIGEST_LENGTH)
                memset(&VenusSHA, 0, SHA_DIGEST_LENGTH);
        }
        Recov_EndTrans(CMFP);

    NonRepExit:
        PutConn(&c);
    }

    if (code && (code != EINCONS)) {
        Recov_BeginTrans();
        /* Demote or discard existing status. */
        if (HAVESTATUS(this) && code != ENOENT)
            Demote();
        else
            Kill();
        Recov_EndTrans(DMFP);
    }

    return (code);
}

int fsobj::GetACL(RPC2_BoundedBS *acl, uid_t uid)
{
    LOG(10, ("fsobj::GetACL: (%s), uid = %d\n", GetComp(), uid));

    if (IsLocalObj()) {
        /* Just read/lookup rights for System:AnyUser */
        const char *fakeacl = "1\n0\nSystem:AnyUser\t9\n";
        acl->SeqLen         = strlen(fakeacl) + 1;
        memcpy(acl->SeqBody, fakeacl, acl->SeqLen);
        return 0;
    }

    /* check if the object is FETCHABLE first! */
    if (!FETCHABLE(this))
        return ETIMEDOUT;

    return GetAttr(uid, acl);
}

/*  *****  Store  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalStore(Date_t Mtime, unsigned long NewLength)
{
    /* Update local state. */
    RVMLIB_REC_OBJECT(*this);

    stat.DataVersion++;
    stat.Length = NewLength;
    stat.Date   = Mtime;
    cf.SetLength(NewLength);
    cf.SetValidData(NewLength);
    memset(VenusSHA, 0, SHA_DIGEST_LENGTH);

    UpdateCacheStats((IsDir() ? &FSDB->DirAttrStats : &FSDB->FileAttrStats),
                     WRITE, NBLOCKS(sizeof(fsobj)));
}

int fsobj::DisconnectedStore(Date_t Mtime, uid_t uid, unsigned long NewLength,
                             int prepend)
{
    int code = 0;
    repvol *rv;

    if (!(vol->IsReadWrite())) {
        return ETIMEDOUT;
    }

    rv = (repvol *)vol;

    Recov_BeginTrans();
    /* Failure to log a store would be most unpleasant for the user! */
    /* Probably we should try to guarantee that it never happens
     * (e.g., by reserving a record at open). */
    code = rv->LogStore(Mtime, uid, &fid, NewLength, prepend);

    if (code == 0 && prepend == 0)
        /* It's already been updated if we're 'prepending',
         * which basically means it is a repair-related operation,
         * and doing it again would trigger an assertion. */
        LocalStore(Mtime, NewLength);
    Recov_EndTrans(DMFP);

    return (code);
}

int fsobj::Store(unsigned long NewLength, Date_t Mtime, uid_t uid)
{
    LOG(10, ("fsobj::Store: (%s), uid = %d\n", GetComp(), uid));

    int code = 0;

    if (IsPioctlFile()) {
        Recov_BeginTrans();
        LocalStore(Mtime, NewLength);
        Recov_EndTrans(DMFP);
    } else
        code = DisconnectedStore(Mtime, uid, NewLength);

    if (code != 0) {
        Recov_BeginTrans();
        /* Stores cannot be retried, so we have no choice but to nuke the file. */
        if (code == ERETRY)
            code = EINVAL;
        Kill();
        Recov_EndTrans(DMFP);
    }

    return (code);
}

/*  *****  SetAttr/SetAcl  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalSetAttr(Date_t Mtime, unsigned long NewLength, Date_t NewDate,
                         uid_t NewOwner, unsigned short NewMode)
{
    /* Update local state. */
    RVMLIB_REC_OBJECT(*this);

    if (NewLength != (unsigned long)-1) {
        FSO_ASSERT(this, !WRITING(this));

        if (HAVEDATA(this)) {
            int delta_blocks = (int)(BLOCKS(this) - NBLOCKS(NewLength));
            if (delta_blocks < 0) {
                eprint("LocalSetAttr: %d\n", delta_blocks);
            }
            UpdateCacheStats(&FSDB->FileDataStats, REMOVE, delta_blocks);
            FSDB->FreeBlocks(delta_blocks);

            data.file->Truncate((unsigned)NewLength);
        }
        stat.Length = NewLength;
        stat.Date   = Mtime;
        memset(VenusSHA, 0, SHA_DIGEST_LENGTH);
    }
    if (NewDate != (Date_t)-1) {
        stat.Date = NewDate;

        /* if this is an open file, the time will be set to the mtime of the
	 * container file when the fd is closed. So we also update the mtime
	 * of the container file so that this will become the mtime (as long
	 * as there are no further writes). */
        if (IsFile() && flags.owrite) {
            struct timeval times[2];
            times[0].tv_sec = times[1].tv_sec = NewDate;
            times[0].tv_usec = times[1].tv_usec = 0;
            data.file->Utimes(times);
        }
    }
    if (NewOwner != VA_IGNORE_UID)
        stat.Owner = NewOwner;
    if (NewMode != VA_IGNORE_MODE)
        stat.Mode = NewMode;

    UpdateCacheStats((IsDir() ? &FSDB->DirAttrStats : &FSDB->FileAttrStats),
                     WRITE, NBLOCKS(sizeof(fsobj)));
}

int fsobj::DisconnectedSetAttr(Date_t Mtime, uid_t uid, unsigned long NewLength,
                               Date_t NewDate, uid_t NewOwner,
                               unsigned short NewMode, int prepend)
{
    int code = 0;
    repvol *rv;

    if (!(vol->IsReadWrite())) {
        return ETIMEDOUT;
    }

    rv = (repvol *)vol;

    Recov_BeginTrans();
    RPC2_Integer tNewMode = (short)NewMode; /* sign-extend!!! */

    CODA_ASSERT(vol->IsReadWrite());
    code = rv->LogSetAttr(Mtime, uid, &fid, NewLength, NewDate, NewOwner,
                          (RPC2_Unsigned)tNewMode, prepend);
    if (code == 0 && prepend == 0)
        /* It's already been updated if we're 'prepending',
         * which basically means it is a repair-related operation,
         * and doing it again would trigger an assertion. */
        LocalSetAttr(Mtime, NewLength, NewDate, NewOwner, NewMode);
    Recov_EndTrans(DMFP);

    return (code);
}

int fsobj::SetAttr(struct coda_vattr *vap, uid_t uid)
{
    Date_t NewDate          = (Date_t)-1;
    unsigned long NewLength = (unsigned long)-1;
    uid_t NewOwner          = VA_IGNORE_UID;
    unsigned short NewMode  = VA_IGNORE_MODE;

    LOG(10, ("fsobj::SetAttr: (%s), uid = %d\n", GetComp(), uid));
    VPROC_printvattr(vap);

    if (vap->va_size != VA_IGNORE_SIZE)
        NewLength = vap->va_size;

    if ((vap->va_mtime.tv_sec != VA_IGNORE_TIME1) &&
        (vap->va_mtime.tv_sec != (time_t)stat.Date))
        NewDate = vap->va_mtime.tv_sec;

    if (vap->va_uid != VA_IGNORE_UID && vap->va_uid != stat.Owner)
        NewOwner = vap->va_uid;

    if (vap->va_mode != VA_IGNORE_MODE) {
        /* Only retain the actual user/group/other permission bits */
        vap->va_mode &= (S_IRWXU | S_IRWXG | S_IRWXO);
        if (vap->va_mode != stat.Mode)
            NewMode = vap->va_mode;
    }

    /* When we are truncating to zero length, should create any missing
     * container files */
    if (!NewLength && !HAVEDATA(this)) {
        Recov_BeginTrans();
        RVMLIB_REC_OBJECT(data.file);
        RVMLIB_REC_OBJECT(cf);
        data.file = &cf;
        data.file->Create();
        Recov_EndTrans(MAXFP);
    }

    /* Only update cache file when truncating and open for write! */
    if (NewLength != (unsigned long)-1 && WRITING(this)) {
        Recov_BeginTrans();
        data.file->Truncate((unsigned)NewLength);
        Recov_EndTrans(MAXFP);
        NewLength = (unsigned long)VA_IGNORE_SIZE;
    }

    /* Avoid performing action where possible. */
    if (NewLength == (unsigned long)-1 && NewDate == (Date_t)-1 &&
        NewOwner == VA_IGNORE_UID && NewMode == VA_IGNORE_MODE) {
        return (0);
    }

    int code     = 0;
    Date_t Mtime = Vtime();

    if (IsPioctlFile()) {
        Recov_BeginTrans();
        LocalSetAttr(Mtime, NewLength, NewDate, NewOwner, NewMode);
        Recov_EndTrans(DMFP);
    } else
        code = DisconnectedSetAttr(Mtime, uid, NewLength, NewDate, NewOwner,
                                   NewMode);

    if (code != 0) {
        Demote();
    }
    return (code);
}

int fsobj::SetACL(RPC2_CountedBS *acl, uid_t uid)
{
    struct MRPC_common_params rpc_common;
    struct in_addr ph_addr;
    ViceVersionVector UpdateSet;
    ViceStoreId sid;
    long cbtemp     = 0;
    int ret_code    = 0;
    connent *c      = NULL;
    mgrpent *m      = NULL;
    int asy_resolve = 0;
    repvol *vp      = (repvol *)vol;

    LOG(10, ("fsobj::SetACL: (%s), uid = %d\n", GetComp(), uid));

    if (!REACHABLE(this))
        return ETIMEDOUT;

    if (IsFake() || IsLocalObj())
        /* Ignore attempts to set ACLs on fake objects. */
        return EPERM;

    if (DIRTY(this))
        return EBUSY;

    /* Since we cannot log this operation in the CML we should not be
     * disconnected. The server also responds with a new status block that
     * includes new access rights, so the object should not have pending
     * operations in the log. */
    CODA_ASSERT(acl != 0);

    int code = 0;

    /* Status parameters. */
    ViceStatus status;
    memset(&status, 0, sizeof(status));

    /* setacl really only looks at these two */
    status.DataVersion = stat.DataVersion;
    status.VV          = stat.VV;

    /* but we clear these just in case... */
    status.Date   = (Date_t)-1;
    status.Owner  = (uid_t)-1;
    status.Mode   = (unsigned)-1;
    status.Length = (unsigned)-1;

    LOG(100, ("fsobj::ConnectedSetAcl\n"));

    /* COP2 Piggybacking. */
    char PiggyData[COP2SIZE];
    RPC2_CountedBS PiggyBS;
    PiggyBS.SeqLen  = 0;
    PiggyBS.SeqBody = (RPC2_ByteSeq)PiggyData;

    /* VCB arguments */
    RPC2_Integer VS          = 0;
    CallBackStatus VCBStatus = NoCallBack;
    RPC2_CountedBS OldVS;
    OldVS.SeqLen  = 0;
    OldVS.SeqBody = 0;

    if (vol->IsReadWrite()) {
        code = vp->GetConn(&c, uid, &m, &rpc_common.ph_ix, &ph_addr);
        if (code != 0)
            goto RepExit;

        cbtemp = cbbreaks;

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

        Recov_BeginTrans();
        Recov_GenerateStoreId(&sid);
        Recov_EndTrans(MAXFP);
        {
            vp->PackVS(rpc_common.nservers, &OldVS);

            ARG_MARSHALL(IN_OUT_MODE, ViceStatus, statusvar, status,
                         rpc_common.nservers);
            ARG_MARSHALL(OUT_MODE, RPC2_Integer, VSvar, VS,
                         rpc_common.nservers);
            ARG_MARSHALL(OUT_MODE, CallBackStatus, VCBStatusvar, VCBStatus,
                         rpc_common.nservers)

            /* Make the RPC call. */
            CFSOP_PRELUDE("store::setacl %s\n", comp, fid);
            MULTI_START_MESSAGE(ViceSetACL_OP);
            code = (int)MRPC_MakeMulti(
                ViceSetACL_OP, ViceSetACL_PTR, rpc_common.nservers,
                rpc_common.handles, rpc_common.retcodes, rpc_common.MIp, 0, 0,
                MakeViceFid(&fid), acl, statusvar_ptrs, rpc_common.ph, &sid,
                &OldVS, VSvar_ptrs, VCBStatusvar_ptrs, &PiggyBS);
            MULTI_END_MESSAGE(ViceSetACL_OP);
            CFSOP_POSTLUDE("store::setacl done\n");

            /* Collate responses from individual servers and decide what to
             * do next. */
            if (vol->IsReplicated()) {
                code = vp->Collate_COP1(m, code, &UpdateSet);
            } else {
                code = vol->Collate(c, ret_code);
                InitVV(&UpdateSet);
                if (ret_code == 0)
                    (&(UpdateSet.Versions.Site0))[0] = 1;
            }
            MULTI_RECORD_STATS(ViceSetACL_OP);

            if (code == EASYRESOLVE) {
                asy_resolve = 1;
                code        = 0;
            }
            if (code != 0)
                goto RepExit;

            if (vol->IsReplicated()) {
                /* Collate volume callback information */
                if (cbtemp == cbbreaks)
                    vp->CollateVCB(m, VSvar_bufs, VCBStatusvar_bufs);

                /* Finalize COP2 Piggybacking. */
                if (PIGGYCOP2)
                    vp->ClearCOP2(&PiggyBS);

                /* Manually compute the OUT parameters from the mgrpent::SetAttr()
                 * call! -JJK */
                int dh_ix;
                dh_ix = -1;
                (void)m->DHCheck(0, rpc_common.ph_ix, &dh_ix);
                ARG_UNMARSHALL(statusvar, status, dh_ix);
            } else { // IsNonReplicated
                if (cbtemp == cbbreaks)
                    ((reintvol *)this)
                        ->UpdateVCBInfo(VSvar_bufs[0], VCBStatusvar_bufs[0]);
            }
        }

        Recov_BeginTrans();
        UpdateStatus(&status, &UpdateSet, uid);
        Recov_EndTrans(CMFP);
        if (vol->IsReplicated()) {
            /* Send the COP2 message or add an entry for piggybacking. */
            vp->COP2(m, &sid, &UpdateSet);
        }

    RepExit:
        if (m)
            m->Put();
        if (c)
            PutConn(&c);
        switch (code) {
        case 0:
            if (asy_resolve)
                vp->ResSubmit(0, &fid);
            break;

        case ETIMEDOUT:
        case ESYNRESOLVE:
        case EINCONS:
            code = ERETRY;
            break;

        default:
            break;
        }
    } else { // !IsReadWrite
        /* Acquire a Connection. */
        connent *c;
        ViceStoreId Dummy; /* ViceStore needs an address for indirection */
        volrep *vp = (volrep *)vol;
        code       = vp->GetConn(&c, uid);
        if (code != 0)
            goto NonRepExit;

        /* Make the RPC call. */
        CFSOP_PRELUDE("store::setacl %s\n", comp, fid);
        UNI_START_MESSAGE(ViceSetACL_OP);
        code = (int)ViceSetACL(c->connid, MakeViceFid(&fid), acl, &status, 0,
                               &Dummy, &OldVS, &VS, &VCBStatus, &PiggyBS);
        UNI_END_MESSAGE(ViceSetACL_OP);
        CFSOP_POSTLUDE("store::setacl done\n");

        /* Examine the return code to decide what to do next. */
        code = vp->Collate(c, code);
        UNI_RECORD_STATS(ViceSetACL_OP);

        if (code != 0)
            goto NonRepExit;

        /* Do setattr locally. */
        Recov_BeginTrans();
        UpdateStatus(&status, NULL, uid);
        Recov_EndTrans(CMFP);

    NonRepExit:
        PutConn(&c);
    }

    /* Cached rights are suspect now! */
    if (code == 0)
        Demote();

    if (OldVS.SeqBody)
        free(OldVS.SeqBody);

    return (code);
}

/*  *****  Create  *****  */

/* MUST be called from within transaction! */
void fsobj::LocalCreate(Date_t Mtime, fsobj *target_fso, char *name,
                        uid_t Owner, unsigned short Mode)
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

    /* Set target status and data. */
    {
        /* Initialize the target fsobj. */
        RVMLIB_REC_OBJECT(*target_fso);
        target_fso->stat.VnodeType   = File;
        target_fso->stat.LinkCount   = 1;
        target_fso->stat.Length      = 0;
        target_fso->stat.DataVersion = 0;
        target_fso->stat.Date        = Mtime;
        target_fso->stat.Owner       = Owner;
        target_fso->stat.Mode        = Mode;
        target_fso->Matriculate();
        target_fso->SetParent(fid.Vnode, fid.Unique);

        RVMLIB_REC_OBJECT(target_fso->cf);
        target_fso->data.file = &target_fso->cf;
        target_fso->data.file->Create();

        /* We don't bother doing a ChangeDiskUsage() here since
         * NBLOCKS(target_fso->stat.Length) == 0. */

        target_fso->Reference();
        target_fso->ComputePriority();
    }
}

int fsobj::DisconnectedCreate(Date_t Mtime, uid_t uid, fsobj **t_fso_addr,
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
    code = rv->AllocFid(File, &target_fid, uid);
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
    code =
        rv->LogCreate(Mtime, uid, &fid, name, &target_fso->fid, Mode, prepend);

    if (code == 0 && prepend == 0) {
        /* This MUST update second-class state! */
        /* It's already been updated if we're 'prepending',
         * which basically means it is a repair-related operation,
         * and doing it again would trigger an assertion. */
        LocalCreate(Mtime, target_fso, name, uid, Mode);

        /* target_fso->stat is not initialized until LocalCreate */
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

/* Returns target object write-locked (on success). */
int fsobj::Create(char *name, fsobj **target_fso_addr, uid_t uid,
                  unsigned short Mode, int target_pri)
{
    LOG(10, ("fsobj::Create: (%s, %s, %d), uid = %d\n", GetComp(), name,
             target_pri, uid));

    int code         = 0;
    Date_t Mtime     = Vtime();
    *target_fso_addr = 0;

    code =
        DisconnectedCreate(Mtime, uid, target_fso_addr, name, Mode, target_pri);

    if (code != 0) {
        Demote();
    }
    return (code);
}
