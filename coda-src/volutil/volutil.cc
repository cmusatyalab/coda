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








/*

    volutil.c -- file server routine for servicing volume utility requests 

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>

#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp.h>
#include <lock.h>
#include <rpc2.h>
#include <se.h>
#include <rvmlib.h>
#include <util.h>
#include <vice.h> 
#include <volutil.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>
#include <vldb.h>
#include <vutil.h>



extern void ViceTerminate();
extern void ViceUpdateDB();
extern void SwapLog();
extern void SwapMalloc();
extern long int volUtil_ExecuteRequest(RPC2_Handle, RPC2_PacketBuffer*, SE_Descriptor*);

static void InitServer();
static void VolUtilLWP(int *);
/* RPC key lookup routine */
static long VolGetKey(RPC2_CountedBS *, RPC2_EncryptionKey, RPC2_EncryptionKey);

static RPC2_EncryptionKey vkey;	/* Encryption key for bind authentication */


/*
 * Called by fileserver to initialize VolUtil subsystem
 * and spawn any necessary lwps.
 */

void InitVolUtil(int stacksize)
{
    InitServer();

    if (stacksize == 0)
	stacksize = 8 * 1024;	/* Isn't this rediculously small? -- DCS */
    
    /* Must allow two utilities to run simultaneously */
    PROCESS mypid;
    for(int i = 0; i < 2; i++) {
	LWP_CreateProcess((PFIC)VolUtilLWP, stacksize, LWP_NORMAL_PRIORITY,
			    (char *)&i, "VolUtilLWP", &mypid);
    }
}


void VolUtilLWP(int *myindex) {
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer *myrequest;
    RPC2_Handle	mycid;
    int lwpid;
    register int rc;
    ProgramType *pt;

    /* using rvm - so set the per thread data structure for executing transactions */
    rvm_perthread_t rvmptt;
    if (RvmType == RAWIO || RvmType == UFS) {
	rvmptt.tid = NULL;
	rvmptt.list.table = NULL;
	rvmptt.list.count = 0;
	rvmptt.list.size = 0;
	rvmlib_set_thread_data(&rvmptt);
	LogMsg(0, SrvDebugLevel, stdout, 
	       "VolUtilLWP %d just did a rvmlib_set_thread_data()\n",
	       *myindex);
    }

    /* tag this lwp as a volume utility */
    pt = (ProgramType *) malloc(sizeof(ProgramType));
    *pt = volumeUtility;
    CODA_ASSERT(LWP_NewRock(FSTAG, (char *)pt) == LWP_SUCCESS);

    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = UTIL_SUBSYSID;
    lwpid = *myindex;
    LogMsg(1, SrvDebugLevel, stdout, "Starting VolUtil Worker %d", lwpid);

    while(1) {
	mycid = 0;

	rc = RPC2_GetRequest(&myfilter, &mycid, &myrequest, NULL,
			(long (*)(...))VolGetKey, RPC2_XOR, NULL);
	if (rc == RPC2_SUCCESS) {
	    LogMsg(5, SrvDebugLevel, stdout, "VolUtilWorker %d received request %d",
				lwpid, myrequest->Header.Opcode);

	    rc = volUtil_ExecuteRequest((RPC2_Handle)mycid, myrequest, NULL);
	    if (rc) {
		LogMsg(0, SrvDebugLevel, stdout, "volutil lwp %d: request %d failed with %s",
			lwpid, myrequest->Header.Opcode, ViceErrorMsg(rc));
	    }
	    if(rc <= RPC2_ELIMIT) {
		RPC2_Unbind(mycid);
	    }
	}
	else {
	   LogMsg(0, SrvDebugLevel, stdout,"RPC2_GetRequest failed with %s",ViceErrorMsg(rc));
	}
    }
}


static void InitServer() {
    RPC2_SubsysIdent subsysid;
    FILE *tokfile;

    /* get encryption key for authentication */
    tokfile = fopen(VolTKFile, "r");
    memset(vkey, 0, RPC2_KEYSIZE);
    fread(vkey, 1, RPC2_KEYSIZE, tokfile);
    fclose(tokfile);

    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = UTIL_SUBSYSID;
    CODA_ASSERT(RPC2_Export(&subsysid) == RPC2_SUCCESS);
    }

static long VolGetKey(RPC2_CountedBS *cid, RPC2_EncryptionKey id, RPC2_EncryptionKey skey) {
    char name[32];

    sprintf(name, "%s", VolName);
    if (strncmp((char *)cid->SeqBody, name, RPC2_KEYSIZE) != 0)
	return(-1);
    bcopy(vkey, id, RPC2_KEYSIZE);
    return(0);
}

long GetVolId(char *volume)
{
    LogMsg(29, SrvDebugLevel, stdout, "Entering GetVolId(%s)", volume);
    long volid = 0;
    if (sscanf(volume, "%lX", &volid) != 1){
	LogMsg(29, SrvDebugLevel, stdout, "GetVolId: Failed to convert volume number");
	/* try to look up the volume in the VLDB */
	struct vldb *vldp = NULL;
	vldp = VLDBLookup(volume);
	if (vldp != NULL){
	    volid = vldp->volumeId[vldp->volumeType];
	    LogMsg(29, SrvDebugLevel, stdout, "GetVolId: Id is 0x%x for name %s", 
		    volid, volume);
	    return volid;
	}
	LogMsg(29, SrvDebugLevel, stdout, "GetVolId Returns 0");
	return 0;
    }
    LogMsg(29, SrvDebugLevel, stdout, "GetVolId Returns %x", volid);
    return volid;
}
	    
/*
 * Routines for forwarding volutil administrative
 * calls to the appropriate fileserver routines.
 */

/*
  BEGIN_HTML
  <a name="S_VolUpdateDB"><strong>Update the VLDB, VRDB and VSGDB </strong></a> 
  END_HTML
*/
long  S_VolUpdateDB(RPC2_Handle cid) {
    ViceUpdateDB();
    return(0);
}

/*
  BEGIN_HTML
  <a name="S_VolShutdown"><strong>Request a server shutdown</strong></a> 
  END_HTML
*/
long S_VolShutdown(RPC2_Handle cid) {
    ViceTerminate();
    return(RPC2_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="S_VolSwaplog"><strong>Request a server to move its log file from
  <tt>SrvLog</tt> to <tt>SrvLog-1</tt></strong></a> 
  END_HTML
*/
long S_VolSwaplog(RPC2_Handle cid) {
    SwapLog();
    return(RPC2_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="S_VolSwapmalloc"><strong>Toggle rds malloc tracing</strong></a> 
  END_HTML
*/
long S_VolSwapmalloc(RPC2_Handle cid) {
    SwapMalloc();
    return(RPC2_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="S_VolSetDebug"><strong>Set the debug level 
  printing</strong></a> 
  END_HTML
*/
long S_VolSetDebug(RPC2_Handle cid, RPC2_Integer debuglevel) {
    LogMsg(0, VolDebugLevel, stdout, "Setting Volume debug level to %d", debuglevel);
    SetVolDebugLevel(debuglevel);
    SrvDebugLevel = debuglevel;	/* file server log level */
    AL_DebugLevel = RPC2_DebugLevel = SrvDebugLevel/10;
    DirDebugLevel = SrvDebugLevel;
    return(RPC2_SUCCESS);
}



/* temporary null files to satisfy linker */
long S_VolMerge(RPC2_Handle cid)
{
    LogMsg(0, VolDebugLevel, stdout, "S_VolMerge not yet implemented");
    return(0);
}

