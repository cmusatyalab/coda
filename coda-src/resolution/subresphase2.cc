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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/rvmres/subresphase2.cc,v 4.3 1998/01/10 18:38:21 braam Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include <stdio.h>
#include <rpc2.h>
#include <util.h>
#include <vcrcommon.h>
#include <res.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <srv.h>
#include <olist.h>
#include <dlist.h>
#include <cvnode.h>
#include <vlist.h>
#include <operations.h>
#include <vrdb.h>
#include <timing.h>
#include "ops.h"
#include "rvmrestiming.h"

/* resubphase2:
	Subordinate side of Phase 2 during resolution: 
		Log Collection and Merging
		Subordinate returns the log of related objects as a byte stream
		Coordinator merges these logs together into a big linear buffer
*/
	/* *********** Private Routines ***************/

static int rs_ShipLogs(RPC2_Handle, char *, int);

long RS_ResPhase2(RPC2_Handle RPCid, ViceFid *Fid, RPC2_Integer *size, 
		  RPC2_Integer *nentries, SE_Descriptor *sed)
{
    SLog(1, "RS_ResPhase2: Entering for Fid = %s\n", FID_(Fid));
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
	    SLog(0, "RS_FetchLog: Couldn't Xlate VSG for %s", FID_(Fid));
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
	SLog(9, "RS_ResPhase2: Dumping  log for %s\n", FID_(Fid));
	DumpLog(VnLog(v->vptr), volptr, &buf, (int *)size, (int *)nentries);
	PollAndYield();
    }
    // Ship log back to coordinator
    {
	SLog(9, "RS_ResPhase2: Shipping log for %s\n", FID_(Fid));
	errorCode = rs_ShipLogs(RPCid, buf, *size);
    }

  Exit:
    // put objects and return 
    {
	PutObjects(errorCode, volptr, NO_LOCK, vlist, 0, 0);
    }
    // clean up 
    if (buf) 
	    free(buf);
    PROBE(tpinfo, RecovSubP2End);
    SLog(1, "RS_ResPhase2: Leaving for Fid = %s result %d\n", 
	 FID_(Fid), errorCode);
    return(errorCode);
}



static int rs_ShipLogs(RPC2_Handle RPCid, char *buf, int bufsize) 
{
    int errorCode = 0;

    SE_Descriptor sid;
    bzero((void *)&sid, sizeof(SE_Descriptor));
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
