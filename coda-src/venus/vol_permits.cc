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
#ifdef __BSD44__
#include <machine/endian.h>
#endif

#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif

/* interfaces */
#include <vice.h>
#include <writeback.h>

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

/* ok, so this makes an RPC call to the server to try to get a permit;
   we return PermitSet if we successfully got one, otherwise NoPermit */
int repvol::GetPermit(uid_t uid)
{
    long permit = WB_DISABLED;
    int i,permits_recvd = 0;
    connent *c;
    int code = realm->GetAdmConn(&c);

    ViceFid fid;
    fid.Volume = vid;
    fid.Vnode = fid.Unique = 0;

    if (code != 0)
	return code;

    //   ViceGetWBPermit(c->connid, vid, &fid, &permit);
    
    /* Acquire an Mgroup. */
    
    mgrpent *m = 0;
    code = GetMgrp(&m, uid, 0);
    if (code != 0) {
	ClearPermit();
	return VPStatus;
    }

    /* Marshall arguments */
    ARG_MARSHALL(IN_MODE, VolumeId, vidvar, vid, VSG_MEMBERS);
    ARG_MARSHALL(IN_MODE, ViceFid, fidvar, fid, VSG_MEMBERS);
    ARG_MARSHALL(OUT_MODE, RPC2_Integer, permitvar, permit, VSG_MEMBERS);

    /* Send the MultiRPC */
    MULTI_START_MESSAGE(ViceGetWBPermit_OP);
    code = (int) MRPC_MakeMulti(ViceGetWBPermit_OP, ViceGetWBPermit_PTR,
				VSG_MEMBERS, m->rocc.handles,
				m->rocc.retcodes, m->rocc.MIp, 0, 0,
				vid, &fid, permitvar_ptrs);
    MULTI_END_MESSAGE(ViceGetWBPermit_OP);
    MULTI_RECORD_STATS(ViceGetWBPermit_OP);

    for (i=0;i<VSG_MEMBERS;i++) {
	ARG_UNMARSHALL(permitvar, permit, i);  /* do this for each copy */
	LOG(1, ("repvol::GetPermit(): %d replied to ViceGetWBPermit with %d:%d\n",i,m->rocc.retcodes[i],permit));

	if (permit == WB_PERMIT_GRANTED)
	    permits_recvd++;

    }
    
    if (permits_recvd == AVSGsize()) {       /* we have all permits   */
	LOG(1, ("repvol::GetPermit(): Got all permits."));
        VPStatus = PermitSet;
    }
    else if (permits_recvd == 0) {              /* we don't have any     */
	LOG(1, ("repvol::GetPermit(): Don't have any permits, doing nothing"));
	ClearPermit();
    }
    else {                                /* need to return those we have */
	LOG(1, ("repvol::GetPermit(): Have only %d of %d permits, returning others",permits_recvd,AVSGsize()));
	ReturnPermit(uid);
	ClearPermit();
    }
    
    return VPStatus;
}

void repvol::ClearPermit()
{
    VPStatus = NoPermit;

    LOG(1, ("repvol::ClearPermit(): hey, I just cleared a permit!\n"));
}

void repvol::ReturnPermit(uid_t uid)
{	
    mgrpent   *m = 0;
    int     code = GetMgrp(&m, uid, 0);

    ARG_MARSHALL(IN_MODE, VolumeId, vidvar,vid, VSG_MEMBERS);
    MULTI_START_MESSAGE(ViceRejectWBPermit_OP);
    code = (int) MRPC_MakeMulti(ViceRejectWBPermit_OP, ViceRejectWBPermit_PTR,
				VSG_MEMBERS, m->rocc.handles,
				m->rocc.retcodes, m->rocc.MIp, 0, 0,
				vid);
    MULTI_END_MESSAGE(ViceRejectWBPermit_OP);
    MULTI_RECORD_STATS(ViceRejectWBPermit_OP);	    
}

