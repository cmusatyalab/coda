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

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <libc.h>
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
PRIVATE void ProcessIncList(ViceFid *, dlist *, dlist *);


//Sub_ResPhase34
//	called between phase 3 and phase 4 of resolution 
//		only if needed i.e. if there are inconsistencies
long Sub_ResPhase34(RPC2_Handle RPCid, ViceFid *Fid, ViceStoreId *logid, 
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
		   "Sub_ResPhase34: Coudnt Xlate VSG %x", Fid->Volume);
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
		   "Sub_ResPhase34: Error getting objects");
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
	assert(ov && ov->vptr);
	if (errorCode = SpoolVMLogRecord(vlist, ov->vptr, volptr, 
					 logid, ResolveNULL_OP, 0)) {
	    if (errorCode == ENOSPC) {
		LogMsg(0, SrvDebugLevel, stdout, 
		       "Sub_ResPhase34 - no space for spooling log record - ignoring\n");
		errorCode = 0;
	    }
	    else {
		LogMsg(0, SrvDebugLevel, stdout, 
		       "Sub_ResPhase34 - error during SpoolVMLogRecord\n");
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

// for rpc2 compatibility
long RS_ResPhase34(RPC2_Handle RPCid, ViceFid *Fid, ViceStoreId *logid, 
		    ViceStatus *status, RPC2_BoundedBS *piggyinc) {
    return(Sub_ResPhase34(RPCid, Fid, logid, status, piggyinc));
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

PRIVATE void ProcessIncList(ViceFid *Fid, dlist *inclist, 
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
