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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/rvmres/subresphase2.cc,v 1.1 1996/11/22 19:13:20 braam Exp $";
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
#include <srv.h>
#include <res.h>
#include <operations.h>
#include <vrdb.h>
#include <timing.h>
#include "ops.h"
#include "rvmrestiming.h"

// resubphase2:
//	Subordinate side of Phase 2 during resolution: 
//		Log Collection and Merging
//		Subordinate returns the log of related objects as a byte stream
//		Coordinator merges these logs together into a big linear buffer

// *********** Private Routines ***************
PRIVATE int ShipLogs(RPC2_Handle, char *, int);


long Sub_ResPhase2(RPC2_Handle RPCid, ViceFid *Fid, RPC2_Integer *size, 
		   RPC2_Integer *nentries, SE_Descriptor *sed) {
    LogMsg(1, SrvDebugLevel, stdout,
	   "Sub_ResPhase2: Entering for Fid = (0x%x.%x.%x)\n",
	   Fid->Volume, Fid->Vnode, Fid->Unique);
    
    PROBE(tpinfo, RecovSubP2Begin);
    int errorCode = 0;
    Volume *volptr = 0;
    dlist *vlist = new dlist((CFN)VLECmp);
    vle *v;
    char *buf = NULL;
    *size = 0;
    *nentries = 0;

    // Validate parameters 
    {
	if (!XlateVid(&Fid->Volume)) {
	    LogMsg(0, SrvDebugLevel, stdout,  
		   "RS_FetchLog: Couldn't Xlate VSG %x", Fid->Volume);
	    return(EINVAL);
	}
    }
    // get objects 
    {
	v = AddVLE(*vlist, Fid);
	if (errorCode = GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, NO_LOCK, 0, 0))
	    goto Exit;
    }
    // Check Phase2 Semantics 
    {
	// Check if volume has been locked by caller 
	// Check that the log is not wrapped around 
	// XXX not implemented yet 
    }
    // dump log to buffer 
    {
	LogMsg(9, SrvDebugLevel, stdout,  
	       "Sub_ResPhase2: Dumping  log\n");
	DumpLog(VnLog(v->vptr), volptr, &buf, (int *)size, (int *)nentries);
	PollAndYield();
    }
    // Ship log back to coordinator
    {
	LogMsg(9, SrvDebugLevel, stdout,  
	       "Sub_ResPhase2: Shipping log\n");
	errorCode = ShipLogs(RPCid, buf, *size);
    }

  Exit:
    // put objects and return 
    {
	PutObjects(errorCode, volptr, NO_LOCK, vlist, 0, 0);
    }
    // clean up 
    if (buf) free(buf);
    PROBE(tpinfo, RecovSubP2End);
    LogMsg(1, SrvDebugLevel, stdout,
	   "Sub_ResPhase2: Leaving for Fid = (0x%x.%x.%x) result %d\n",
	   Fid->Volume, Fid->Vnode, Fid->Unique, errorCode);
    return(errorCode);
}

long RS_ResPhase2(RPC2_Handle RPCid, ViceFid *Fid, RPC2_Integer *size, 
		  RPC2_Integer *nentries, SE_Descriptor *sed) {
    
    return(Sub_ResPhase2(RPCid, Fid, size, nentries, sed));
}


PRIVATE int ShipLogs(RPC2_Handle RPCid, char *buf, int bufsize) {
    int errorCode = 0;

    SE_Descriptor sid;
    bzero(&sid, sizeof(SE_Descriptor));
    sid.Tag = SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sid.Value.SmartFTPD.SeekOffset = 0;
    sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
    sid.Value.SmartFTPD.Tag = FILEINVM;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = bufsize;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = bufsize;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)buf;
    
    if((errorCode = RPC2_InitSideEffect(RPCid, &sid)) 
       <= RPC2_ELIMIT) {
	LogMsg(0, SrvDebugLevel, stdout,  
	       "ShipLogs: InitSE failed (%d)\n", errorCode);
	return(errorCode);
    }
    
    if ((errorCode = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) 
	<= RPC2_ELIMIT) {
	LogMsg(0, SrvDebugLevel, stdout,  
	       "ShipLogs: CheckSE failed (%x)", errorCode);
	if (errorCode == RPC2_SEFAIL1) errorCode = EIO;
    }
    return(errorCode);
}
