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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "coda_string.h"

#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <rpc2/rpc2.h>
#include <volutil.h>

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
    CODA_ASSERT(tracefile != NULL);

    if (!RPCTraceBufInited) {
	RPC2_InitTraceBuffer(RPCTRACEBUFSIZE);
	RPCTraceBufInited = 1;
	CODA_ASSERT(!RPC2_Trace);
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
    memset(&sed, 0, sizeof(SE_Descriptor));
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
