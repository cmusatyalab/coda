/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
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

#include <sys/types.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <volutil.h>

#ifdef __cplusplus
}
#endif

#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <timing.h>
#include <util.h>

extern timing_path *tpinfo;
extern int probingon;

#define TIMINGFILE "/tmp/timing.tmp"
static FILE * timingfile;
/*
  BEGIN_HTML
  <a name="S_VolTiming"><strong>Toggle timing flag and process timing trace</strong></a>
  END_HTML
*/
long S_VolTiming(RPC2_Handle rpcid, RPC2_Integer OnFlag, SE_Descriptor *formal_sed) {
    SE_Descriptor sed;
    int rc = 0;
    
    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolTiming: OnFlag = %d", OnFlag);

    VInitVolUtil(volumeUtility);
    if (OnFlag) {
	probingon = 1;
	LogMsg(9, VolDebugLevel, stdout, "Probing is now %s", probingon?"ON":"OFF");
    }
    else if (probingon) {
	probingon = 0;
	timingfile = fopen(TIMINGFILE, "w");
	fprintf(timingfile, "Processing tpinfo and FileresTPinfo \n");
	/* process the buffer */
	if (tpinfo) {
	    tpinfo->postprocess(timingfile);
	    delete tpinfo;
	    tpinfo = 0;
	}
	if (FileresTPinfo) {
	    FileresTPinfo->postprocess(timingfile);
	    delete FileresTPinfo;
	    FileresTPinfo = 0;
	}
	fprintf(timingfile, "Finished processing tpinfo and FileresTPinfo \n");
	fclose(timingfile);

	/* set up SE_Descriptor for transfer */
	memset(&sed, 0, sizeof(SE_Descriptor));
	sed.Tag = SMARTFTP;
	sed.Value.SmartFTPD.Tag = FILEBYNAME;
	sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, TIMINGFILE);
	sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0755;

	if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) {
	    LogMsg(0, VolDebugLevel, stdout, "VolTiming: InitSideEffect failed with %s", 
		RPC2_ErrorMsg(rc));
	    VDisconnectFS();
	    return(-1);
	}

	if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
	    RPC2_ELIMIT) {
	    LogMsg(0, VolDebugLevel, stdout, "VolTiming: CheckSideEffect failed with %s", 
		RPC2_ErrorMsg(rc));
	    VDisconnectFS();
	    return(-1);
	}
    }
    VDisconnectFS();
    return(0);
}
	
	
