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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <rpc2/rpc2.h>
#include <util.h>
#include <vcrcommon.h>
#include <res.h>

#ifdef __cplusplus
}
#endif

#include <srv.h>
#include <olist.h>
#include <dlist.h>
#include <cvnode.h>
#include <vlist.h>
#include <operations.h>
#include <vrdb.h>
#include <timing.h>
#include <lockqueue.h>
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

long RS_FetchLogs(RPC2_Handle RPCid, ViceFid *Fid, RPC2_Integer *size,
                  RPC2_Integer *nentries, SE_Descriptor *sed)
{
    SLog(1, "RS_FetchLogs: Entering for Fid = %s\n", FID_(Fid));
    PROBE(tpinfo, RecovSubP2Begin);
    int errorCode  = 0;
    Volume *volptr = 0;
    dlist *vlist   = new dlist((CFN)VLECmp);
    vle *v;
    char *buf = NULL;
    *size     = 0;
    *nentries = 0;

    // Validate parameters
    {
        if (!XlateVid(&Fid->Volume)) {
            SLog(0, "RS_FetchLogs: Couldn't Xlate VSG for %s", FID_(Fid));
            return (EINVAL);
        }
    }
    // get objects
    {
        v = AddVLE(*vlist, Fid);
        if ((errorCode =
                 GetFsObj(Fid, &volptr, &v->vptr, READ_LOCK, NO_LOCK, 0, 0, 0)))
            goto Exit;
    }
    // Check Phase2 Semantics
    {
        // Check if volume has been locked by caller
        // Check that the log is not wrapped around
        // XXX not implemented yet
    } // dump log to buffer
    {
        SLog(9, "RS_FetchLogs: Dumping log for %s\n", FID_(Fid));
        DumpLog(VnLog(v->vptr), volptr, &buf, (int *)size, (int *)nentries);
        PollAndYield();
    }
    // Ship log back to coordinator
    {
        SLog(9, "RS_FetchLogs: Shipping log for %s\n", FID_(Fid));
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
    SLog(1, "RS_FetchLogs: Leaving for Fid = %s result %d\n", FID_(Fid),
         errorCode);
    return (errorCode);
}

static int rs_ShipLogs(RPC2_Handle RPCid, char *buf, int bufsize)
{
    int errorCode = 0;

    SE_Descriptor sid;
    memset(&sid, 0, sizeof(SE_Descriptor));
    sid.Tag                                   = SMARTFTP;
    sid.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sid.Value.SmartFTPD.SeekOffset            = 0;
    sid.Value.SmartFTPD.hashmark = (SrvDebugLevel > 2 ? '#' : '\0');
    sid.Value.SmartFTPD.Tag      = FILEINVM;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen    = bufsize;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = bufsize;
    sid.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody   = (RPC2_ByteSeq)buf;

    if ((errorCode = RPC2_InitSideEffect(RPCid, &sid)) <= RPC2_ELIMIT) {
        LogMsg(0, SrvDebugLevel, stdout, "ShipLogs: InitSE failed (%d)\n",
               errorCode);
        return (errorCode);
    }

    if ((errorCode = RPC2_CheckSideEffect(RPCid, &sid, SE_AWAITLOCALSTATUS)) <=
        RPC2_ELIMIT) {
        LogMsg(0, SrvDebugLevel, stdout, "ShipLogs: CheckSE failed (%x)",
               errorCode);
        if (errorCode == RPC2_SEFAIL1)
            errorCode = EIO;
    }
    return (errorCode);
}
