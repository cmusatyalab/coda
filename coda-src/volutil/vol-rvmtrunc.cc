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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-rvmtrunc.cc,v 4.2 1997/02/26 16:04:13 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <rvm.h>
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
