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
#include <srv.h>

/*
  BEGIN_HTML
  <a name="S_PrintStats"><strong>Print out various server statistics</strong></a>
  END_HTML
*/
long S_PrintStats(RPC2_Handle rpcid, SE_Descriptor *formal_sed)
{
    FILE *statsfile;
    SE_Descriptor sed;
    long rc = 0;
    int fd;

    LogMsg(1, VolDebugLevel, stdout, "Entering S_PrintStats\n");

    statsfile = tmpfile();
    CODA_ASSERT(statsfile != NULL);

    PrintCounters(statsfile);
    PrintCallBackState(statsfile);

    // ship the file back 
    fd = fileno(statsfile);
    lseek(fd, 0, SEEK_SET);
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = fd;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) 
	LogMsg(0, VolDebugLevel, stdout, 
	       "PrintStats: InitSideEffect failed with %s", RPC2_ErrorMsg(rc));

    if (!rc && ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
		RPC2_ELIMIT)) 
	LogMsg(0, VolDebugLevel, stdout, 
	       "PrintStats: CheckSideEffect failed with %s", RPC2_ErrorMsg(rc));

    fclose(statsfile);
    LogMsg(1, VolDebugLevel, stdout, "PrintStats returns %d\n", rc);
    return(rc);
}

