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

#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2.h>
#include <rvm.h>
#include <volutil.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <srv.h>

extern int stack;
int rvm_truncate_stack = 1024;

void TruncProcess() {
    PROCESS mypid;
    LogMsg(0, VolDebugLevel, stdout,
	   "TruncProcess: Going to Truncate RVM log \n");
    rvm_truncate();
    LogMsg(0, VolDebugLevel, stdout,
	   "TruncProcess: Finished truncating rvm log \n");
    LWP_CurrentProcess(&mypid);
    LWP_DestroyProcess(mypid);
}
/*
  BEGIN_HTML
  <strong> Service rvm log truncation request</strong> 
  END_HTML
*/
long S_TruncateRVMLog(RPC2_Handle rpcid) {
    long rc = 0;
    PROCESS truncpid;

    LogMsg(1, VolDebugLevel, stdout,
	   "Entering S_TrucateRVMLog\n");
    LogMsg(1, VolDebugLevel, stdout,
	   "Forking New Thread to Truncate RVM Log\n");
    // give this thread a bigger stack(1Meg) since it is going to truncate the log
    rc = LWP_CreateProcess((PFIC)TruncProcess, rvm_truncate_stack * 1024, 
			   LWP_NORMAL_PRIORITY,
			   (char *)&rc/*dummy*/, "SynchronousRVMTrunc", 
			   &truncpid);
    LogMsg(1, VolDebugLevel, stdout, 
	   "Returning to volutil client\n");
    return(rc);
}
