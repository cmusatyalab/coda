#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /home/braam/src/coda-src/res/RCS/weres.cc,v 1.3 1996/12/09 17:28:02 braam Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <libc.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef CAMELOT
#include <cam/camelot_prefixed.h>
#include <camlib/camlib_prefixed.h>
#include <cam/_setjmp.h>
#endif CAMELOT
#if 0
#include <cthreads.h>
#else
#include <dummy_cthreads.h>
#endif
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include <util.h>
#ifndef CAMELOT
#include <rvmlib.h>
#endif CAMELOT 
#include <vcrcommon.h>
#include <srv.h>
#include <vrdb.h>
#include <inconsist.h>
#include <util.h>
#include <rescomm.h>
#include <resutil.h>
#include <res.h>

int WERes(ViceFid *Fid, ViceVersionVector **VV, ResStatus **rstatusp,
	  res_mgrpent *mgrp, unsigned long *hosts) {
    
    int errorcode = 0;

    LogMsg(9, SrvDebugLevel, stdout,  "Entering WERes(0x%x.%x.%x)",
	    Fid->Volume, Fid->Vnode, Fid->Unique);

    /* force a new vv */
    {
	ViceVersionVector newvv;
	GetMaxVV(&newvv, VV, -1);

	// Get ResStatus if necessary 
	ViceStatus vstatus;
	if (rstatusp) {
	    unsigned long succflags[VSG_MEMBERS];
	    CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, mgrp->rrcc.hosts, 
			  succflags);
	    GetResStatus(succflags, rstatusp, &vstatus);
	}
	else bzero(&vstatus, (int) sizeof(ViceStatus));	// for now send a zeroed vstatus.
	// rpc2 doesn\'t like a NULL being passed as an IN parameter 
	MRPC_MakeMulti(ForceDirVV_OP, ForceDirVV_PTR, VSG_MEMBERS, 
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, Fid, &newvv, /* rstatusp ? &vstatus : NULL*/ &vstatus);
	LogMsg(9, SrvDebugLevel, stdout,  "WERes returned from ForceVV");
    }

    /* coerce rpc errors as timeouts - check ret codes */
    {
	mgrp->CheckResult();
	errorcode = (int) CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
				  mgrp->rrcc.hosts, hosts);
    }
    return(errorcode);
}


/* subordinate end of Weakly Equal resolution -
 *	each subordinate forces a  new vector passed to it
 */
long RS_ForceVV(RPC2_Handle RPCid, ViceFid *Fid, ViceVersionVector *VV, ViceStatus *statusp) {

    int res = 0;
    Vnode *vptr = 0;
    Volume *volptr = 0;
    VolumeId VSGVolnum = Fid->Volume;
    int status = 0;
    ViceVersionVector *DiffVV = 0;
    int errorcode = 0;
    conninfo *cip = GetConnectionInfo(RPCid);
    if (cip == NULL){
	LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceVV: Couldnt get conninfo ");
	return(EINVAL);
    }

    if (!XlateVid(&Fid->Volume)) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceVV: Couldnt Xlate VSG %x",
		Fid->Volume);
	return(EINVAL);
    }
    
    /* get the object */
    if (errorcode = GetFsObj(Fid, &volptr, &vptr, WRITE_LOCK, NO_LOCK, 0, 0)) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceVV: GetFsObj returns error %d", errorcode);
	errorcode = EINVAL;
	goto FreeLocks;
    }
    
    /* make sure volume is locked by coordinator */
    if (V_VolLock(volptr).IPAddress != cip->GetRemoteHost()) {
	LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceVV: Volume not locked by coordinator");
	errorcode = EWOULDBLOCK;
	goto FreeLocks;
    }

    LogMsg(9, SrvDebugLevel, stdout,  "ForceVV: vector passed in is :");
    if (SrvDebugLevel >= 9)
	PrintVV(stdout, VV);
    LogMsg(9, SrvDebugLevel, stdout,  "ForceVV: vector in the vnode is :");
    if (SrvDebugLevel >= 9)
	PrintVV(stdout, &Vnode_vv(vptr));
    
    /* check that the new version vector is >=  old vv */
    res = VV_Cmp(&Vnode_vv(vptr), VV);
    if (res != VV_EQ) {
	if (res == VV_SUB) {
	    DiffVV = new ViceVersionVector;
	    *DiffVV = *VV;
	    SubVVs(DiffVV, &Vnode_vv(vptr));
	    AddVVs(&Vnode_vv(vptr), DiffVV);
	    AddVVs(&V_versionvector(volptr), DiffVV);
	    CodaBreakCallBack(0, Fid, VSGVolnum);
	    delete DiffVV;
	}
	else {
	    errorcode = EINCOMPATIBLE;
	    LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceVV: Version Vectors are inconsistent");
	    LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceVV: Vectors are: ");
	    LogMsg(0, SrvDebugLevel, stdout,  "[%d %d %d %d %d %d %d %d][0x%x.%x][%d]",
		    Vnode_vv(vptr).Versions.Site0, Vnode_vv(vptr).Versions.Site1,
		    Vnode_vv(vptr).Versions.Site2, Vnode_vv(vptr).Versions.Site3,
		    Vnode_vv(vptr).Versions.Site4, Vnode_vv(vptr).Versions.Site5,
		    Vnode_vv(vptr).Versions.Site6, Vnode_vv(vptr).Versions.Site7,
		    Vnode_vv(vptr).StoreId.Host, Vnode_vv(vptr).StoreId.Uniquifier, 
		    Vnode_vv(vptr).Flags);
	    LogMsg(0, SrvDebugLevel, stdout,  "[%d %d %d %d %d %d %d %d][0x%x.%x][%d]",
		    VV->Versions.Site0, VV->Versions.Site1,
		    VV->Versions.Site2, VV->Versions.Site3,
		    VV->Versions.Site4, VV->Versions.Site5,
		    VV->Versions.Site6, VV->Versions.Site7,
		    VV->StoreId.Host, VV->StoreId.Uniquifier, 
		    VV->Flags);
	    
	    goto FreeLocks;
	}
    }
    else 
	LogMsg(0, SrvDebugLevel, stdout,  "RS_ForceVV: Forcing the old version vector!!!!!!!!");
    
    /* if cop pending flag is set for this vnode, then clear it */
    if (COP2Pending(Vnode_vv(vptr))) {
	LogMsg(9, SrvDebugLevel, stdout,  "ForceVV: Clearing COP2 pending flag ");
	ClearCOP2Pending(Vnode_vv(vptr));
    }

    if (statusp) {
	// need to set the owner/author/date/modebits 
	// for now due to the rp2gen bug (can\'t pass NULL pointers for IN parameters)
	// make sure Date is nonzero to check that the status is valid 
	if (statusp->Date) {
	    vptr->disk.modeBits = statusp->Mode;
	    vptr->disk.author = statusp->Author;
	    vptr->disk.owner = statusp->Owner;
	    vptr->disk.unixModifyTime = statusp->Date;
	}
    }

FreeLocks:
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED);
    /* release lock on vnode and put the volume */
    Error filecode = 0;
    if (vptr) {
	VPutVnode(&filecode, vptr);
	assert(filecode == 0);
    }
    PutVolObj(&volptr, NO_LOCK);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, status);
    LogMsg(9, SrvDebugLevel, stdout,  "RS_ForceVV returns %d", errorcode);
    return(errorcode);
}
