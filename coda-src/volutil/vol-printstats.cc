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

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <volutil.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <srv.h>

#define STATSFILE "/tmp/statsfileXXXXXX"

static FILE *statsfile;
/*
  BEGIN_HTML
  <a name="S_PrintStats"><strong>Print out various server statistics</strong></a>
  END_HTML
*/
long S_PrintStats(RPC2_Handle rpcid, SE_Descriptor *formal_sed) {
    SE_Descriptor sed;
    long rc = 0;
    LogMsg(1, VolDebugLevel, stdout,
	   "Entering S_PrintStats\n");
    char filename[256];
    strcpy(filename, STATSFILE);
    mktemp(filename);
    statsfile = fopen(filename, "w");
    CODA_ASSERT(statsfile != NULL);

    PrintCounters(statsfile);
    PrintCallBackState(statsfile);
    fclose(statsfile);

    // ship the file back 
    bzero((void *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, filename);
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0755;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) 
	LogMsg(0, VolDebugLevel, stdout, 
	       "PrintStats: InitSideEffect failed with %s", RPC2_ErrorMsg(rc));

    if (!rc && ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
		RPC2_ELIMIT)) 
	LogMsg(0, VolDebugLevel, stdout, 
	       "PrintStats: CheckSideEffect failed with %s", RPC2_ErrorMsg(rc));
    LogMsg(1, VolDebugLevel, stdout,
	   "PrintStats returns %d\n", rc);
    unlink(filename);
    return(rc);
}

