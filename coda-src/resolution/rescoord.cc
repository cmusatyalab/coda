/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/






/* 
 * rescoord.c
 *	Implements the coordinator side for 
 *	directory resolution
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include "coda_assert.h"
#include <stdio.h>
#include <struct.h>
#include <lwp/lwp.h>
#include <rpc2/rpc2.h>
#include <util.h>
#include <codadir.h>
#include "coda_string.h"

#ifdef __cplusplus
}
#endif __cplusplus

#include <olist.h>
#include <rpc2/errors.h>
#include <srv.h>
#include <inconsist.h>
#include <vlist.h>
#include <operations.h>
#include <res.h>
#include <treeremove.h>

#include "rescomm.h"
#include "resutil.h"
#include "resforce.h"
#include "timing.h"

timing_path *tpinfo = 0;
timing_path *FileresTPinfo = 0; 
extern void ResCheckServerLWP();

/* private routines */
static int AlreadyIncGroup(ViceVersionVector **VV, int nvvs);
static int WEResPhase1(ViceVersionVector **, res_mgrpent *, ViceFid *, 
			unsigned long *, ViceStoreId *);
static int WEResPhase2(res_mgrpent *, ViceFid *, unsigned long *, 
		       ViceStoreId *);


/* two VV's are weakly equal if they have the same store-id: 
   this means that the files are identical, but the COP2 never made 
   it to the server
*/

int IsWeaklyEqual(ViceVersionVector **VV, int nvvs) 
{
    int i, j;

    SLog(10,  "Entering IsWeaklyEqual()");

    /* find first one */
    for (i = 0; i < nvvs - 1 ; i++){
	if (VV[i] != NULL) 
		break; 
    }

    /* there was at most one VV */
    if ( i >= nvvs-1 )
	    return 1;

    /* compare others */
    for (j = i + 1; j < nvvs; j++) {
	    if (VV[j] == NULL) 
		    continue;
	    if (bcmp((const void *)&(VV[i]->StoreId), 
		     (const void *) &(VV[j]->StoreId), 
		     sizeof(ViceStoreId))) {
		    SLog(10,  "IsWeaklyEqual returning 0");
		    return 0;
	    }
    }

    /* weakly equal vvs */
    SLog(10,  "IsWeaklyEqual returning 1.");
    return(1);
}

static int AlreadyIncGroup(ViceVersionVector **VV, int nvvs) 
{
    for (int i = 0; i < nvvs; i++) {
	if (VV[i] == NULL) 
		continue;
	if (IsIncon((*(VV[i])))) 
		return(1);
    }
    return(0);
}

static int WEResPhase1(ViceVersionVector **VV, 
		       res_mgrpent *mgrp, ViceFid *Fid, 
		       unsigned long *hosts, ViceStoreId *stid) 
{
	int errorCode = 0;
	ViceVersionVector newvv;
	
	SLog(9,  "Entering WEResPhase1 for %s", FID_(Fid));

	/* force a new vv */
	GetMaxVV(&newvv, VV, -1);
	*stid = newvv.StoreId;
	MRPC_MakeMulti(ForceDirVV_OP, ForceDirVV_PTR, VSG_MEMBERS, 
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, Fid, &newvv);
	SLog(9,  "WEResPhase1 returned from ForceDir");
	
	/* coerce rpc errors as timeouts - check ret codes */
	mgrp->CheckResult();
	errorCode = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
				  mgrp->rrcc.hosts, hosts);
	SLog(9,  "WEResPhase1 returning %d", errorCode);
	return(errorCode);
}

static int WEResPhase2(res_mgrpent *mgrp, ViceFid *Fid, 
		       unsigned long *successHosts, ViceStoreId *stid) 
{
	int i;
	ViceVersionVector UpdateSet;
	unsigned long hosts[VSG_MEMBERS];
	int error;

	SLog(9,  "Entering ResPhase2 %s", FID_(Fid));
	/* form the update set */
	bzero((void *)&UpdateSet, sizeof(ViceVersionVector));
	
	for (i = 0; i < VSG_MEMBERS; i++)
		if (successHosts[i])
			(&(UpdateSet.Versions.Site0))[i] = 1;
    
	MRPC_MakeMulti(COP2_OP, COP2_PTR, VSG_MEMBERS, 
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, stid, &UpdateSet);     

	mgrp->CheckResult();
	error = CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
			      mgrp->rrcc.hosts, 
			      hosts);
	return(error);
}

long OldDirResolve(res_mgrpent *mgrp, ViceFid *Fid, ViceVersionVector **VV) 
{

	int reserror = 1;
	int i;
	int HowMany = 0;
	ViceVersionVector *vv[VSG_MEMBERS];
	unsigned long hosts[VSG_MEMBERS];
	ViceStoreId stid;

	/* regenerate VVs for host set */
	for (i = 0; i < VSG_MEMBERS; i++) 
		if (!mgrp->rrcc.hosts[i])
			VV[i] = NULL;
	
	UpdateRunts(mgrp, VV, Fid);

	/* check if any object already inc */
	if (AlreadyIncGroup(VV, VSG_MEMBERS)) {
		SLog(0,  "OldDirResolve: Group already inconsistent");
		goto Exit;
	}

	/* checking if vv's already equal */
	SLog(9,  "DirResolve: Checking if Objects equal ");
	for (i = 0; i < VSG_MEMBERS; i++) 
		vv[i] = VV[i];
	if (VV_Check(&HowMany, vv, 1) == 1) {
		SLog(0,  "OldDirResolve: VECTORS ARE ALREADY EQUAL");
		return(0);
	}


	SLog(9,  "OldDirResolve: Checking for weak Equality");
	if (IsWeaklyEqual(VV, VSG_MEMBERS)) {
		SLog(39,  "DirResolve: WEAKLY EQUAL DIRECTORIES");

		reserror = WEResPhase1(VV, mgrp, Fid, hosts, &stid);
		if (reserror || mgrp->GetHostSet(hosts)) {
			SLog(0,  "OldDirResolve: error %d in (WE)ResPhase1",
			     reserror);
			goto Exit;
		}

		reserror = WEResPhase2(mgrp, Fid, hosts, &stid);
		if (reserror) {
			SLog(0,  "OldDirResolve: error %d in (WE)ResPhase2",
			     reserror);
			goto Exit;
		} else 
			return(0);
	}

  Exit:
    if (reserror) {
	MRPC_MakeMulti(MarkInc_OP, MarkInc_PTR, VSG_MEMBERS,
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes, 
		       mgrp->rrcc.MIp, 0, 0, Fid);
	reserror = EINCONS;
    }
    SLog(9,  "OldDirResolve returns %d", reserror);
    return(reserror);
}
    
int WERes(ViceFid *Fid, ViceVersionVector **VV, ResStatus **rstatusp,
	  res_mgrpent *mgrp, unsigned long *hosts) 
{
    
    int errorcode = 0;

    SLog(9,  "Entering WERes %s", FID_(Fid));

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
	else bzero((void *)&vstatus, (int) sizeof(ViceStatus));	// for now send a zeroed vstatus.
	// rpc2 doesn\'t like a NULL being passed as an IN parameter 
	MRPC_MakeMulti(ForceDirVV_OP, ForceDirVV_PTR, VSG_MEMBERS, 
		       mgrp->rrcc.handles, mgrp->rrcc.retcodes,
		       mgrp->rrcc.MIp, 0, 0, Fid, &newvv, /* rstatusp ? &vstatus : NULL*/ &vstatus);
	SLog(9,  "WERes returned from ForceVV");
    }

    /* coerce rpc errors as timeouts - check ret codes */
    {
	mgrp->CheckResult();
	errorcode = (int) CheckRetCodes((unsigned long *)mgrp->rrcc.retcodes, 
				  mgrp->rrcc.hosts, hosts);
    }
    return(errorcode);
}
    
