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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/volutil/RCS/vol-tracerpc.cc,v 4.1 1997/01/08 21:52:37 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <string.h>

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
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>


#define TRACEFILE "/tmp/tracefileXXXXXX"
#define RPCTRACEBUFSIZE 500

static FILE *tracefile;
int RPCTraceBufInited = 0;

/*
  BEGIN_HTML
  <a name="S_TraceRpc"><strong>Turn on(off) rpc tracing (and process trace buffer)</strong></a>
  END_HTML
*/
long S_TraceRpc(RPC2_Handle rpcid, SE_Descriptor *formal_sed) {
    SE_Descriptor sed;
    long rc = 0;
    LogMsg(1, VolDebugLevel, stdout,
	   "Entering S_TraceRpc\n");
    char filename[256];
    strcpy(filename, TRACEFILE);
    mktemp(filename);
    tracefile = fopen(filename, "w");
    assert(tracefile != NULL);

    if (!RPCTraceBufInited) {
	RPC2_InitTraceBuffer(RPCTRACEBUFSIZE);
	RPCTraceBufInited = 1;
	assert(!RPC2_Trace)
	RPC2_Trace = 1;
	fprintf(tracefile, "Inited trace buffer; tracing is now ON\n");
    }
    else if (!RPC2_Trace) {
	RPC2_Trace = 1;
	fprintf(tracefile, "Tracing is now turned on\n");
    }
    else {
	// dump trace buffers and turn off tracing
	fprintf(tracefile, "Dumping RPC buffers \n");
	RPC2_DumpTrace(tracefile, RPCTRACEBUFSIZE);
	RPC2_DumpState(tracefile, 0);
	RPC2_Trace = 0;
    }
    fclose(tracefile);

    // ship the file back 
    bzero(&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, filename);
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0755;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) 
	LogMsg(0, VolDebugLevel, stdout, 
	       "TraceRpc: InitSideEffect failed with %s", RPC2_ErrorMsg(rc));

    if (!rc && ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
		RPC2_ELIMIT)) 
	LogMsg(0, VolDebugLevel, stdout, 
	       "TraceRpc: CheckSideEffect failed with %s", RPC2_ErrorMsg(rc));
    
    LogMsg(1, VolDebugLevel, stdout,
	   "TraceRpc returns %d\n", rc);
    unlink(filename);
    return(rc);
}
