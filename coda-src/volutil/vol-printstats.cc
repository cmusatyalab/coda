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

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/volutil/RCS/vol-printstats.cc,v 4.1 1997/01/08 21:52:32 rvb Exp $";
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
    assert(statsfile != NULL);

    PrintCounters(statsfile);
    PrintCallBackState(statsfile);
    fclose(statsfile);

    // ship the file back 
    bzero(&sed, sizeof(sed));
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

