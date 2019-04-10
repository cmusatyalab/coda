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

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#ifdef __cplusplus
}
#endif

#include <lwp/lwp.h>
#include "comm.h"
#include "mariner.h"
#include "mgrp.h"
#include "venus.private.h"
#include "venusvol.h"
#include "vsg.h"

char mgrpent::mgrp_sync;

#ifdef VENUSDEBUG
int mgrpent::allocs   = 0;
int mgrpent::deallocs = 0;
#endif

/* ***** Mgroup  ***** */

void Mgrp_Wait(void)
{
    LOG(0, ("WAITING(MGRPQ):\n"));
    START_TIMING();

    VprocWait((char *)&mgrpent::mgrp_sync);

    END_TIMING();
    LOG(0, ("WAIT OVER, elapsed = %3.1f\n", elapsed));
}

void Mgrp_Signal(void)
{
    VprocSignal((char *)&mgrpent::mgrp_sync, 0);
}

/* Bit-masks for ignoring certain classes of errors. */
#define _ETIMEDOUT 1
#define _EINVAL 2
#define _ENXIO 4
#define _ENOSPC 8
#define _EDQUOT 16
#define _EIO 32
#define _EACCES 64
#define _EWOULDBLOCK 128

static int Unanimity(int *codep, struct in_addr *hosts, RPC2_Integer *retcodes,
                     int mask)
{
    int code = -1;

    for (int i = 0; i < VSG_MEMBERS; i++) {
        if (!hosts[i].s_addr)
            continue;

        switch (retcodes[i]) {
        case ETIMEDOUT:
            if (mask & _ETIMEDOUT)
                continue;
            break;

        case EINVAL:
            if (mask & _EINVAL)
                continue;
            break;

        case ENXIO:
            if (mask & _ENXIO)
                continue;
            break;

        case EIO:
            if (mask & _EIO)
                continue;
            break;

        case ENOSPC:
            if (mask & _ENOSPC)
                continue;
            break;

        case EDQUOT:
            if (mask & _EDQUOT)
                continue;
            break;

        case EACCES:
            if (mask & _EACCES)
                continue;
            break;

        case EWOULDBLOCK:
            if (mask & _EWOULDBLOCK)
                continue;
            break;
        }

        if (code == -1)
            code = (int)retcodes[i];
        else if (code != retcodes[i])
            return (0);
    }

    *codep = (code == -1 ? ERETRY : code);
    return (1);
}

/* ***** Replicated operation context  ***** */

RepOpCommCtxt::RepOpCommCtxt()
{
    LOG(100, ("RepOpCommCtxt::RepOpCommCtxt: \n"));
    int i;

    HowMany = 0;
    memset(handles, 0, VSG_MEMBERS * sizeof(RPC2_Handle));
    memset(hosts, 0, VSG_MEMBERS * sizeof(struct in_addr));
    memset(retcodes, 0, VSG_MEMBERS * sizeof(RPC2_Integer));
    memset(&primaryhost, 0, sizeof(struct in_addr));
    MIp = 0;
    for (i = 0; i < VSG_MEMBERS; i++)
        dying[i] = 0;
}

int RepOpCommCtxt::AnyReturned(int code)
{
    for (int i = 0; i < VSG_MEMBERS; i++) {
        if (!hosts[i].s_addr)
            continue;

        if (retcodes[i] == code)
            return (1);
    }

    return (0);
}

mgrpent::mgrpent(vsgent *VSG, uid_t Uid, RPC2_Handle mid, int authflag)
{
    LOG(1, ("mgrpent::mgrpent %p, uid = %d, mid = %d, auth = %d\n", this, uid,
            mid, authflag));

    /* These members are immutable. */
    uid = Uid;
    memset(&McastInfo, 0, sizeof(RPC2_Multicast));
    McastInfo.Mgroup       = mid;
    McastInfo.ExpandHandle = 0;
    vsg                    = VSG;
    authenticated          = authflag;
    disconnectfs           = 0;

#ifdef VENUSDEBUG
    allocs++;
#endif
}

mgrpent::~mgrpent()
{
    LOG(1, ("mgrpent::~mgrpent %p, uid = %d, mid = %d, auth = %d\n", this, uid,
            McastInfo.Mgroup, authenticated));

    int code = 0;

#ifdef VENUSDEBUG
    deallocs++;
#endif

    if (!list_empty(&vsghandle))
        list_del(&vsghandle);

    /* should we keep a reference to the vsg? probably not. */
    //vsg->PutRef();

    if (disconnectfs) {
        /* Make the RPC call. */
        MarinerLog("fetch::DisconnectFS\n");
        MULTI_START_MESSAGE(ViceDisconnectFS_OP);
        int code = (int)MRPC_MakeMulti(ViceDisconnectFS_OP,
                                       ViceDisconnectFS_PTR, VSG_MEMBERS,
                                       rocc.handles, rocc.retcodes, rocc.MIp, 0,
                                       0);
        MULTI_END_MESSAGE(ViceDisconnectFS_OP);
        MarinerLog("fetch::disconnectfs done\n");

        /* Collate responses from individual servers and decide what to do next. */
        code = CheckNonMutating(code);
        MULTI_RECORD_STATS(ViceDisconnectFS_OP);
        LOG(1, ("mgrpent::~mgrpent: ViceDisconnectFS -> %s\n",
                RPC2_ErrorMsg(code)));
    }

    /* Kill active members. */
    for (int i = 0; i < VSG_MEMBERS; i++)
        KillMember(&rocc.hosts[i], 1);

    /* Delete Mgroup. */
    code = (int)RPC2_DeleteMgrp(McastInfo.Mgroup);
    LOG(1, ("mgrpent::~mgrpent: RPC2_DeleteMgrp -> %s\n", RPC2_ErrorMsg(code)));
}

void mgrpent::Put()
{
    LOG(100,
        ("mgrpent::Put %p, uid = %d, mid = %d, auth = %d, refcount = %d, detached = %d\n",
         this, uid, McastInfo.Mgroup, authenticated, refcount,
         list_empty(&vsghandle)));

    if (!InUse()) {
        print(GetLogFile());
        CHOKE("mgrpent::Put mgrp not in use");
    }

    /* Clean up the host set. */
    PutHostSet();
    /* wake threads that are looking for free mgrps */
    Mgrp_Signal();
    PutRef();
}

void mgrpent::Kill(int tellservers)
{
    LOG(100, ("mgrpent::Kill: %p uid = %d, mid = %d\n", this, uid,
              McastInfo.Mgroup));

    disconnectfs = tellservers;

    /* take this mgrp out of the lookup list */
    if (!list_empty(&vsghandle)) {
        list_del(&vsghandle);
        Mgrp_Signal();
        PutRef();
    }
}

int mgrpent::CreateMember(int idx)
{
    struct in_addr hosts[VSG_MEMBERS];

    vsg->GetHosts(hosts);

    LOG(100, ("mgrpent::CreateMember: %p, uid = %d, mid = %d, host = %s\n",
              this, uid, McastInfo.Mgroup, inet_ntoa(hosts[idx])));

    if (!hosts[idx].s_addr)
        CHOKE("mgrpent::CreateMember: no host at index %d", idx);

    int code = 0;

    /* Bind/Connect to the server. */
    srvent *s              = GetServer(&hosts[idx], vsg->realmid);
    RPC2_Handle ConnHandle = 0;
    int auth               = authenticated;
    code                   = s->Connect(&ConnHandle, &auth, uid, 0);
    PutServer(&s);
    if (code != 0)
        return (code);

    /* Add new connection to the Mgrp. */
    code = (int)RPC2_AddToMgrp(McastInfo.Mgroup, ConnHandle);
    LOG(1,
        ("mgrpent::CreateMember: RPC_AddToMgrp -> %s\n", RPC2_ErrorMsg(code)));
    if (code != 0) {
        (void)RPC2_Unbind(ConnHandle);
        return (ETIMEDOUT);
    }

    /* Update rocc state. */
    rocc.HowMany++;
    rocc.handles[idx]  = ConnHandle;
    rocc.hosts[idx]    = hosts[idx];
    rocc.retcodes[idx] = 0;
    rocc.dying[idx]    = 0;

    return (0);
}

void mgrpent::KillMember(struct in_addr *host, int forcibly)
{
    LOG(100,
        ("mgrpent::KillMember: %p, uid = %d, mid = %d, host = %s, forcibly = %d\n",
         this, uid, McastInfo.Mgroup, inet_ntoa(*host), forcibly));

    long code = 0;

    if (!host->s_addr)
        return;

    /* we first mark the host that should die to avoid making the passed
     * host pointer useless (f.i. when it is &rocc.hosts[i]) */
    for (int i = 0; i < VSG_MEMBERS; i++)
        if (rocc.hosts[i].s_addr == host->s_addr)
            rocc.dying[i] = 1;

    for (int i = 0; i < VSG_MEMBERS; i++)
        if (rocc.dying[i] && rocc.hosts[i].s_addr == rocc.primaryhost.s_addr)
            rocc.primaryhost.s_addr = 0;

    if (InUse() && !forcibly)
        return;

    /* now we can safely kill dying members */
    for (int i = 0; i < VSG_MEMBERS; i++) {
        if (rocc.dying[i]) {
            code = RPC2_RemoveFromMgrp(McastInfo.Mgroup, rocc.handles[i]);
            LOG(1, ("mgrpent::KillMember: RPC2_RemoveFromMgrp(%s, %d) -> %s\n",
                    inet_ntoa(rocc.hosts[i]), rocc.handles[i],
                    RPC2_ErrorMsg((int)code)));

            code = RPC2_Unbind(rocc.handles[i]);
            LOG(1, ("mgrpent::KillMember: RPC2_Unbind(%s, %d) -> %s\n",
                    inet_ntoa(rocc.hosts[i]), rocc.handles[i],
                    RPC2_ErrorMsg((int)code)));

            rocc.HowMany--;
            rocc.handles[i]      = 0;
            rocc.hosts[i].s_addr = 0;
            rocc.retcodes[i]     = 0;
            rocc.dying[i]        = 0;
        }
    }
}

int mgrpent::GetHostSet()
{
    struct in_addr hosts[VSG_MEMBERS];
    vsg->GetHosts(hosts);

    LOG(100, ("mgrpent::GetHostSet: %p, uid = %d, mid = %d\n", this, uid,
              McastInfo.Mgroup));
    int i, rc;

    /* Create members of the specified set which are not already in the
     * Mgroup. */
    for (i = 0; i < VSG_MEMBERS; i++) {
        if (hosts[i].s_addr && !rocc.hosts[i].s_addr) {
            rc = CreateMember(i);
            if (rc == EINTR)
                return rc;

            if (rc == EPERM && authenticated)
                return ERETRY;

            if (rc == EPERM)
                return rc;
        }
    }

    /* Kill members of the Mgroup which are not in the specified set. */
    for (i = 0; i < VSG_MEMBERS; i++)
        if (!hosts[i].s_addr && rocc.hosts[i].s_addr)
            KillMember(&rocc.hosts[i], 1);

    /* Ensure that Mgroup is not empty. */
    if (rocc.HowMany == 0)
        return (ETIMEDOUT);

    /* Validate primaryhost. */
    if (!rocc.primaryhost.s_addr) {
        /* When the rocc.retcodes are all be zero, all available
         * hosts are Dominant Hosts, and we can use PickDH */
        memset(rocc.retcodes, 0, sizeof(RPC2_Integer) * VSG_MEMBERS);
        rocc.primaryhost = rocc.hosts[PickDH(NULL)];
    }

    return (0);
}

void mgrpent::PutHostSet()
{
    LOG(100, ("mgrpent::PutHostSet: %p\n", this));

    /* Kill dying members. */
    for (int i = 0; i < VSG_MEMBERS; i++)
        if (rocc.dying[i])
            KillMember(&rocc.hosts[i], 0);
}

/* Translate RPC and Volume errors, and update server state. */
void mgrpent::CheckResult()
{
    for (int i = 0; i < VSG_MEMBERS; i++) {
        if (!rocc.hosts[i].s_addr)
            continue;

        switch (rocc.retcodes[i]) {
        default:
            if (rocc.retcodes[i] < 0) {
                srvent *s = GetServer(&rocc.hosts[i], vsg->realmid);
                s->ServerError((int *)&rocc.retcodes[i]);
                PutServer(&s);
            }
            /* Note that KillMember may zero rocc.hosts[i] !!! */
            if (rocc.retcodes[i] == ETIMEDOUT || rocc.retcodes[i] == ERETRY)
                KillMember(&rocc.hosts[i], 1);
            break;

        case VBUSY:
            rocc.retcodes[i] = EWOULDBLOCK;
            break;

        case VNOVOL:
            rocc.retcodes[i] = ENXIO;
            break;

        case VNOVNODE:
            rocc.retcodes[i] = ENOENT;
            break;

        case VLOGSTALE:
            rocc.retcodes[i] = EALREADY;
            break;

        case VSALVAGE:
        case VVOLEXISTS:
        case VNOSERVICE:
        case VOFFLINE:
        case VONLINE:
        case VNOSERVER:
        case VMOVED:
        case VFAIL:
            eprint("mgrpent::CheckResult: illegal code (%d)", rocc.retcodes[i]);
            rocc.retcodes[i] = EINVAL;
            break;
        }
    }
}

/* Maps return codes from Vice:
	0		Call succeeded at all responding hosts (|responders| > 0),
	ETIMEDOUT	No hosts responded,
	ESYNRESOLVE	Multiple non-maskable results were returned,
	EASYRESOLVE	Call succeeded at (at least) one host, and no non-maskable
			errors were returned, but some maskable errors were,
	ERETRY		Some responders NAK'ed,
	Other (> 0)	Call succeeded at no responding host, and all non-maskable errors
			were the same, but some maskable errors may have been returned.
*/
int mgrpent::CheckNonMutating(int acode)
{
    LOG(100,
        ("mgrpent::CheckNonMutating: acode = %d\n\t\thosts = [%#x %#x %#x %#x %#x %#x %#x %#x],\n\t\tretcodes = [%d %d %d %d %d %d %d %d]\n",
         acode, rocc.hosts[0], rocc.hosts[1], rocc.hosts[2], rocc.hosts[3],
         rocc.hosts[4], rocc.hosts[5], rocc.hosts[6], rocc.hosts[7],
         rocc.retcodes[0], rocc.retcodes[1], rocc.retcodes[2], rocc.retcodes[3],
         rocc.retcodes[4], rocc.retcodes[5], rocc.retcodes[6],
         rocc.retcodes[7]));

    int code = 0;
    int i;

    CheckResult();

    /* check for this here because CheckResult may nuke hosts */
    if (rocc.HowMany == 0)
        return (ETIMEDOUT);

    /* Perform additional translations. */
    for (i = 0; i < VSG_MEMBERS; i++) {
        if (!rocc.hosts[i].s_addr)
            continue;

        switch (rocc.retcodes[i]) {
        case ENOSPC:
        case EDQUOT:
        case EINCOMPATIBLE:
            eprint("mgrpent::CheckNonMutating: illegal code (%d)",
                   rocc.retcodes[i]);
            rocc.retcodes[i] = EINVAL;
            break;
        }
    }

    /* The ideal case is a unanimous response. */
    if (Unanimity(&code, rocc.hosts, rocc.retcodes, 0))
        return (code);

    /* Since this operation is non-mutating, we can retry immediately if any
     * host NAK'ed. */
    if (rocc.AnyReturned(ERETRY))
        return (ERETRY);

    /* Look for unanimity, masking off more and more error types. */
    static int ErrorMasks[] = { _ETIMEDOUT,
                                _ETIMEDOUT | _EINVAL,
                                _ETIMEDOUT | _EINVAL | _ENXIO,
                                _ETIMEDOUT | _EINVAL | _ENXIO | _EIO,
                                _ETIMEDOUT | _EINVAL | _ENXIO | _EIO | _EACCES,
                                _ETIMEDOUT | _EINVAL | _ENXIO | _EIO | _EACCES |
                                    _EWOULDBLOCK };
    static int nErrorMasks  = (int)(sizeof(ErrorMasks) / sizeof(int));
    int mask                = ErrorMasks[0];
    for (i = 0; i < nErrorMasks; i++) {
        mask = ErrorMasks[i];
        if (Unanimity(&code, rocc.hosts, rocc.retcodes, mask)) {
            if (code == 0)
                code = EASYRESOLVE;
            return (code);
        }
    }

    /* We never achieved consensus. */
    /* Force a synchronous resolve. */
    return (ESYNRESOLVE);
}

/* Maps return codes from Vice:
	0		Call succeeded at all responding hosts (|responders| > 0),
	ETIMEDOUT	No hosts responded,
	EASYRESOLVE	Call succeeded at (at least) one host, and some (maskable or
			non-maskable) errors were returned,
	ERETRY		All responders NAK'ed, or call succeeded at no responding host
			and multiple non-maskable errors were returned,
	Other (> 0)	Call succeeded at no responding host, and all non-maskable errors
			were the same, but some maskable errors may have been returned.

   OUT parameter, UpdateSet, indicates which sites call succeeded at.
*/
int mgrpent::CheckCOP1(int acode, ViceVersionVector *UpdateSet,
                       int TranslateEincompatible)
{
    LOG(100,
        ("mgrpent::CheckCOP1: acode = %d\n\t\thosts = [%#x %#x %#x %#x %#x %#x %#x %#x],\n\t\tretcodes = [%d %d %d %d %d %d %d %d]\n",
         acode, rocc.hosts[0], rocc.hosts[1], rocc.hosts[2], rocc.hosts[3],
         rocc.hosts[4], rocc.hosts[5], rocc.hosts[6], rocc.hosts[7],
         rocc.retcodes[0], rocc.retcodes[1], rocc.retcodes[2], rocc.retcodes[3],
         rocc.retcodes[4], rocc.retcodes[5], rocc.retcodes[6],
         rocc.retcodes[7]));

    int code = 0;
    int i;

    InitVV(UpdateSet);

    CheckResult();

    /* check for this here because CheckResult may nuke hosts */
    if (rocc.HowMany == 0)
        return (ETIMEDOUT);

    /* Perform additional translations. */
    for (i = 0; i < VSG_MEMBERS; i++) {
        if (!rocc.hosts[i].s_addr)
            continue;

        switch (rocc.retcodes[i]) {
        case EINCOMPATIBLE:
            if (TranslateEincompatible)
                rocc.retcodes[i] = ERETRY; /* NOT for reintegrate! */
            break;
        }
    }

    /* Record successes in the UpdateSet. */
    for (i = 0; i < VSG_MEMBERS; i++) {
        if (!rocc.hosts[i].s_addr)
            continue;

        if (rocc.retcodes[i] == 0)
            (&(UpdateSet->Versions.Site0))[i] = 1;
    }

    /* The ideal case is a unanimous response. */
    if (Unanimity(&code, rocc.hosts, rocc.retcodes, 0))
        return (code);

    /* Look for unanimity, masking off more and more error types. */
    static int ErrorMasks[] = { _ETIMEDOUT,
                                _ETIMEDOUT | _EINVAL,
                                _ETIMEDOUT | _EINVAL | _ENXIO,
                                _ETIMEDOUT | _EINVAL | _ENXIO | _EIO,
                                _ETIMEDOUT | _EINVAL | _ENXIO | _EIO | ENOSPC,
                                _ETIMEDOUT | _EINVAL | _ENXIO | _EIO | ENOSPC |
                                    EDQUOT,
                                _ETIMEDOUT | _EINVAL | _ENXIO | _EIO | ENOSPC |
                                    EDQUOT | _EACCES,
                                _ETIMEDOUT | _EINVAL | _ENXIO | _EIO | ENOSPC |
                                    EDQUOT | _EACCES | _EWOULDBLOCK };
    static int nErrorMasks  = (int)(sizeof(ErrorMasks) / sizeof(int));
    int mask                = ErrorMasks[0];
    for (i = 0; i < nErrorMasks; i++) {
        mask = ErrorMasks[i];
        if (Unanimity(&code, rocc.hosts, rocc.retcodes, mask)) {
            if (code == 0)
                code = EASYRESOLVE;
            return (code);
        }
    }

    /* We never achieved consensus. */
    /* Return ASYRESOLVE if operation succeeded at any host. */
    /* Otherwise, return RETRY, which will induce a RESOLVE at a more convenient point. */
    if (rocc.AnyReturned(0))
        return (EASYRESOLVE);
    return (ERETRY);
}

/* This is identical to mgrpent::CheckCOP1(), EXCEPT that we want to treat */
/* EINCOMPATIBLE results as non-maskable rather that translating them to ERETRY. */
int mgrpent::CheckReintegrate(int acode, ViceVersionVector *UpdateSet)
{
    int ret = CheckCOP1(acode, UpdateSet, 0);

    /* CheckCOP1 doesn't know how to handle EALREADY. If any host had
     * returned EALREADY we can get rid of some CML entries. */
    if (ret == ERETRY) {
        if (rocc.AnyReturned(EALREADY))
            return (EALREADY);
    }
    return (ret);
}

/* Check the remote vectors. */
/* Returns:conf
	0		Version check succeeded
	ESYNRESOLVE	Version check failed
	EASYRESOLVE	!EqReq and check yielded Dom/Sub
*/
int mgrpent::RVVCheck(ViceVersionVector **RVVs, int EqReq)
{
    /* Construct the array so that only valid VVs are checked. */
    for (int j = 0; j < VSG_MEMBERS; j++)
        if (!rocc.hosts[j].s_addr || rocc.retcodes[j])
            RVVs[j] = 0;
    if (GetLogLevel() >= 100)
        VVPrint(GetLogFile(), RVVs);

    int dom_cnt = 0;
    if (!VV_Check(&dom_cnt, RVVs, EqReq))
        return (ESYNRESOLVE);

    if (dom_cnt <= 0 || dom_cnt > rocc.HowMany) {
        print(GetLogFile());
        CHOKE("mgrpent::RVVCheck: bogus dom_cnt (%d)", dom_cnt);
    }

    /* Notify servers which have out of date copies. */
    if (dom_cnt < rocc.HowMany)
        return (EASYRESOLVE);

    return (0);
}

#define DOMINANT(idx)                                     \
    (rocc.hosts[idx].s_addr && rocc.retcodes[idx] == 0 && \
     (RVVs == 0 || RVVs[idx] != 0))

int mgrpent::PickDH(ViceVersionVector **RVVs)
{
    int i, chosen = 0;
    srvent *s;
    unsigned long bw, bwmax = 0;

    CODA_ASSERT(rocc.HowMany != 0);

    /* find strongest host in the dominant set. */
    for (i = 0; i < VSG_MEMBERS; i++) {
        if (DOMINANT(i)) {
            s = GetServer(&rocc.hosts[i], vsg->realmid);
            s->GetBandwidth(&bw);
            PutServer(&s);

            if (bw >= bwmax) {
                bwmax  = bw;
                chosen = i;
            }
        }
    }

    return chosen;
}

/* Validate the existence of a dominant host; return its index in OUT parameter. */
/* If there are multiple hosts in the dominant set, prefer the primary host. */
/* The caller may specify that the PH must be dominant. */
/* Returns {0, ERETRY}. */
int mgrpent::DHCheck(ViceVersionVector **RVVs, int ph_ix, int *dh_ixp,
                     int PHReq)
{
    *dh_ixp = -1;

    /* Return the primary host if it is in the dominant set. */
    if (ph_ix != -1 && DOMINANT(ph_ix)) {
        *dh_ixp = ph_ix;
        return (0);
    }

    /* Find a non-primary host from the dominant set. */
    *dh_ixp = PickDH(RVVs);

    if (PHReq) {
        LOG(1, ("DHCheck: PH -> %x", rocc.hosts[*dh_ixp]));
        rocc.primaryhost = rocc.hosts[*dh_ixp];
        return (ERETRY);
    }

    return (0);
}

struct in_addr *mgrpent::GetPrimaryHost(int *ph_ixp)
{
    int i;

    if (ph_ixp)
        *ph_ixp = -1;

    if (!rocc.primaryhost.s_addr)
        rocc.primaryhost = rocc.hosts[PickDH(NULL)];

    /* Sanity check. */
    for (i = 0; i < VSG_MEMBERS; i++)
        if (rocc.hosts[i].s_addr == rocc.primaryhost.s_addr) {
            if (ph_ixp)
                *ph_ixp = i;
#if 0
            /* Add a round robin distribution, primarily to spread fetches
             * across AVSG. */
            /* Added a random factor to reduce the amount of switching
             * between servers to only of 1 out of every 32 calls --JH */
	    if (RoundRobin && ((rpc2_NextRandom(NULL) & 0x1f) == 0)) {
		int j;
		for (j = i + 1; j != i; j = (j + 1) % VSG_MEMBERS)
		    if (rocc.hosts[j].s_addr) {
			/* We have a valid host. It'd be nice to use strongly
			   connected hosts in preference to weak ones, but I'm
			   not sure how to access to srvent from here.
			   -- DCS 2/2/96
			   */
			rocc.primaryhost = rocc.hosts[j];
			break;
		    }
	    }
#endif
            return (&rocc.hosts[i]);
        }

    CHOKE("mgrpent::GetPrimaryHost: ph (%x) not found", rocc.primaryhost);
    return (NULL); /* dummy to keep g++ happy */
}
