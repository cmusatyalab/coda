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





#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>
#include <rpc2.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <olist.h>
#include <dlist.h>
#include <cvnode.h>
#include <vcrcommon.h>
#include <vlist.h>
#include <vrdb.h>
#include <resutil.h>
#include <srv.h>
#include <operations.h>
#include <timing.h>
#include "ops.h"
#include "rvmrestiming.h"

// ******* Private Routines ***********
static void ProcessIncList(ViceFid *, dlist *, dlist *);


//RS_ResPhase34
//	called between phase 3 and phase 4 of resolution 
//		only if needed i.e. if there are inconsistencies
long RS_ResPhase34(RPC2_Handle RPCid, ViceFid *Fid, ViceStoreId *logid, 
		    ViceStatus *status, RPC2_BoundedBS *piggyinc) {
    PROBE(tpinfo, RecovSubP34Begin);
    Volume *volptr = 0;
    int errorCode = 0;
    int blocks = 0;
    VolumeId VSGVolnum = Fid->Volume;
    dlist *inclist = new dlist((CFN)CompareIlinkEntry);
    
    // validate parms 
    {
	if (!XlateVid(&Fid->Volume)) {
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "RS_ResPhase34: Coudnt Xlate VSG %x", Fid->Volume);
	    //PROBE(tpinfo, CPHASE2END);
	    return(EINVAL);
	}
    }
    
    // parse the inconsisteny list 
    {
	BSToDlist(piggyinc, inclist);
    }
    
    dlist *vlist = 0;
    // get the objects 
    {
	vlist = new dlist((CFN) VLECmp);
	
	if (errorCode = GetPhase2Objects(Fid, vlist, inclist, &volptr)) {
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "RS_ResPhase34: Error getting objects");
	    goto Exit;
	}
    }
    
    // create nonexistent objects 
    {
	if (errorCode = CreateResPhase2Objects(Fid, vlist, inclist, volptr, 
					       VSGVolnum, &blocks)) {
	    LogMsg(0, SrvDebugLevel, stdout,  "DirResPhase2: Error %d in create objects",
		    errorCode);
	    goto Exit;
	}
    }
    
    // Process all inclist entries 
    {
	ProcessIncList(Fid, inclist, vlist);
    }
	
    // spool a resolution record and set status
    {
	vle *ov = FindVLE(*vlist, Fid);
	CODA_ASSERT(ov && ov->vptr);
	if (errorCode = SpoolVMLogRecord(vlist, ov->vptr, volptr, 
					 logid, ResolveNULL_OP, 0)) {
	    if (errorCode == ENOSPC) {
		LogMsg(0, SrvDebugLevel, stdout, 
		       "RS_ResPhase34 - no space for spooling log record - ignoring\n");
		errorCode = 0;
	    }
	    else {
		LogMsg(0, SrvDebugLevel, stdout, 
		       "RS_ResPhase34 - error during SpoolVMLogRecord\n");
		goto Exit;
	    }
	}
	SetStatus(ov->vptr, status, 0, 0);
    }
    
  Exit:
    // put all objects 
    PutObjects(errorCode, volptr, NO_LOCK, vlist, blocks, 1);
    
    // clean up
    CleanIncList(inclist);
    
    LogMsg(9, SrvDebugLevel, stdout,  "DirResPhase2 returns %d", errorCode);
    PROBE(tpinfo, RecovSubP34End);
    return(errorCode);
}

// ProcessIncList
//	For each entry in the inclist 
//		If object exists in the same directory
//			as specified in the ilist entry then 
//			mark the object inconsistent
//		Otherwise mark real parent, object and parent specified
//			in the ilist entry are marked inconsistent
//		If Object doesn't exist at all then try to mark the 
//			parent directory in conflict 

static void ProcessIncList(ViceFid *Fid, dlist *inclist, 
			    dlist *vlist) {
    dlist_iterator next(*inclist);
    ilink *il;
    while (il = (ilink *)next()) {
	ViceFid cfid;
	ViceFid ipfid;
	FormFid(cfid, Fid->Volume, il->vnode, il->unique);
	FormFid(ipfid, Fid->Volume, il->pvnode, il->punique);
	
	vle *v = FindVLE(*vlist, &cfid);
	if (v) {
	    if (!strcmp(il->name, ".")) 
		MarkObjInc(&cfid, v->vptr);
	    else if(v->vptr->disk.vparent == il->pvnode &&
		    v->vptr->disk.uparent == il->punique)
		MarkObjInc(&cfid, v->vptr);
	    else {
		// parents are different - mark both parents inc 
		vle *ipv = FindVLE(*vlist, &ipfid);
		if (ipv && ipv->vptr)
		    MarkObjInc(&ipfid, ipv->vptr);
		
		ViceFid vpfid;
		FormFid(vpfid, Fid->Volume, v->vptr->disk.vparent, 
			v->vptr->disk.uparent);
		vle *vpv = FindVLE(*vlist, &vpfid);
		if (vpv && vpv->vptr)
		    MarkObjInc(&vpfid, vpv->vptr);
	    }
	}
	else { // object couldn't be found/created - mark parent inc
	    vle *ipv = FindVLE(*vlist, &ipfid);
	    if (ipv && ipv->vptr)
		MarkObjInc(&ipfid, ipv->vptr);
	}
    }
}
