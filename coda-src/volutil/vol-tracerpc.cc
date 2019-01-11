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
#endif

#include <util.h>

#define RPCTRACEBUFSIZE 500

int RPCTraceBufInited = 0;

/*
  BEGIN_HTML
  <a name="S_TraceRpc"><strong>Turn on(off) rpc tracing (and process trace buffer)</strong></a>
  END_HTML
*/
long S_TraceRpc(RPC2_Handle rpcid, SE_Descriptor *formal_sed)
{
    FILE *tracefile;
    SE_Descriptor sed;
    long rc = 0;
    int fd;

    LogMsg(1, VolDebugLevel, stdout, "Entering S_TraceRpc\n");

    tracefile = tmpfile();
    CODA_ASSERT(tracefile != NULL);

    if (!RPCTraceBufInited) {
        RPC2_InitTraceBuffer(RPCTRACEBUFSIZE);
        RPCTraceBufInited = 1;
        CODA_ASSERT(!RPC2_Trace);
        RPC2_Trace = 1;
        fprintf(tracefile, "Inited trace buffer; tracing is now ON\n");
    } else if (!RPC2_Trace) {
        RPC2_Trace = 1;
        fprintf(tracefile, "Tracing is now turned on\n");
    } else {
        // dump trace buffers and turn off tracing
        fprintf(tracefile, "Dumping RPC buffers \n");
        RPC2_DumpTrace(tracefile, RPCTRACEBUFSIZE);
        RPC2_DumpState(tracefile, 0);
        RPC2_Trace = 0;
    }

    // ship the file back
    fd = fileno(tracefile);
    lseek(fd, 0, SEEK_SET);
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = fd;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT)
        LogMsg(0, VolDebugLevel, stdout,
               "TraceRpc: InitSideEffect failed with %s", RPC2_ErrorMsg(rc));

    if (!rc && ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
                RPC2_ELIMIT))
        LogMsg(0, VolDebugLevel, stdout,
               "TraceRpc: CheckSideEffect failed with %s", RPC2_ErrorMsg(rc));

    fclose(tracefile);
    LogMsg(1, VolDebugLevel, stdout, "TraceRpc returns %d\n", rc);
    return (rc);
}
