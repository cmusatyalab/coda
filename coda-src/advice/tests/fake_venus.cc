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

#include <sys/param.h>
#include <sys/types.h>
#include "coda_assert.h"
#include "coda_string.h";
#include <stdio.h>

#include <unistd.h>
#include <stdlib.h>
#include <lwp.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "admon.h"
#include "adsrv.h"
#include "demo_handler.h"
#include "../conversions.h"

// Note that fake_venus purposely redefines ADSRVPORTAL 
#define ADSRVPORTAL     1435


#define DFTSTACKSIZE 1024

void Init_RPC(int *); 
void CreateLWPs();
void WorkerHandler();
void CodaDemoHandler();
void PrintHostIdent(), PrintPortalIdent();



const int VenusMajorVersion = 1;
const int VenusMinorVersion = 0;

int InterestedInReadDisco = 0;

const char AdviceSubsys[] = "AdviceSubsys";

int AdviceValid = 0;

unsigned short port; 
int pgid;
char hostname[MAXHOSTNAMELEN];

char preWorkerSync;
char workerSync;
char demoSync;
RPC2_Handle handle;

const int AdviceDaemonStackSize = 8192;

/* RPC Variables */
extern RPC2_PortalIdent rpc2_LocalPortal;
extern RPC2_HostIdent rpc2_LocalHost;

int thisPID;
int workerpid;
int demopid;
int mainpid;


int main(int argc, char *argv[]) {
  RPC2_RequestFilter filter;
  RPC2_Handle handle;
  RPC2_PacketBuffer *packet;
  long sig;

  InitializeCodaDemo();
  thisPID = getpid();
  Init_RPC(&mainpid);
  CODA_ASSERT(IOMGR_Initialize() == LWP_SUCCESS);
  CreateLWPs();

  /* Setup filter */
  filter.FromWhom = ONESUBSYS; 
  filter.OldOrNew = OLDORNEW;
  filter.ConnOrSubsys.SubsysId = ADSRVSUBSYSID; 
  handle = 0;
  packet = 0;

  struct timeval DaemonExpiry;
  DaemonExpiry.tv_sec = 5;
  DaemonExpiry.tv_usec = 0;
  for (;;) {
    long code = RPC2_GetRequest(&filter, &handle, &packet,
                                &DaemonExpiry, NULL, (long)NULL, NULL);
    /* Handle RPC2 errors. */
    if (code == RPC2_TIMEOUT) {
      sig = LWP_SignalProcess(&demoSync);
      CODA_ASSERT((sig == LWP_SUCCESS) || (sig == LWP_ENOWAIT));
      continue;
    }
    if (code <= RPC2_WLIMIT)
      printf("GetRequest -> %s\n", RPC2_ErrorMsg((int) code));
    if (code <= RPC2_ELIMIT) {
      printf("Calling Unbind\n"); fflush(stdout);
      RPC2_Unbind(handle);
      continue;
    }

    code = AdSrv_ExecuteRequest(handle, packet, NULL);
    if (code <= RPC2_WLIMIT)
      printf("ExecuteRequest => %s\n", RPC2_ErrorMsg((int) code));
    else {
      LWP_NoYieldSignal(&workerSync);
    }
  }
}

void Init_RPC(int *mainpid)
{
  char error_msg[BUFSIZ];
  RPC2_PortalIdent *portallist[1], portal1;
  RPC2_SubsysIdent sid;
  long rc ;

  if (LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, (PROCESS *)mainpid) != LWP_SUCCESS) {
    snprintf(error_msg, BUFSIZ, "Can't Initialize LWP");   /* Initialize LWP package */
    printf(error_msg);
    fflush(stdout);
  }

  /* Initialize RPC2 Package */
  portallist[0] = &portal1; 

  portal1.Tag = RPC2_PORTALBYINETNUMBER;
  portal1.Value.InetPortNumber = PORT_venus;

  rc = RPC2_Init(RPC2_VERSION, NULL, portallist, 1, -1, NULL) ;
  if (rc != RPC2_SUCCESS) {
    snprintf(error_msg, BUFSIZ, "%s:  Can't Initialize RPC2", RPC2_ErrorMsg((int)rc));
    printf(error_msg);
    fflush(stdout);
  }

  /* Export Venus subsystem */
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADSRVSUBSYSID;
  rc = RPC2_Export(&sid) != RPC2_SUCCESS ;
  if (rc != RPC2_SUCCESS) {
    snprintf(error_msg, BUFSIZ, "%s\nCan't export the advice subsystem", RPC2_ErrorMsg((int)rc));
    printf(error_msg);
    fflush(stdout);
  }
}


void CreateLWPs() {
  char c, s;

  /****************************************************
   ****    Create LWP to handle interface events   ****
   ****************************************************/

  CODA_ASSERT((LWP_CreateProcess((PFIC)CodaDemoHandler,
                            DFTSTACKSIZE*1024, LWP_NORMAL_PRIORITY,
                           (char *)&c, "CodaDemo Interface Handler",
                           (PROCESS *)&demopid)) == (LWP_SUCCESS));

  /****************************************************
   ******    Create LWP to handle venus events   ******
   ****************************************************/

  CODA_ASSERT((LWP_CreateProcess((PFIC)WorkerHandler,
                            DFTSTACKSIZE*2*1024, LWP_NORMAL_PRIORITY,
                           (char *)&c, "Worker Handler",
                           (PROCESS *)&workerpid)) == (LWP_SUCCESS));
}




int NewConnection(char *hostName, int portNumber, int pgrp) {

  CODA_ASSERT(strlen(hostName) <= MAXHOSTNAMELEN);
  strcpy(hostname, hostName);
  printf("MARIA: You should check that the hostname is our host\n");
  fflush(stdout);

  port = (unsigned short) portNumber;
  pgid = pgrp;
  AdviceValid = 1;

  LWP_NoYieldSignal(&preWorkerSync);
  return(0);
}


void ReturnConnection() {
  RPC2_HostIdent hid;
  RPC2_PortalIdent pid;
  RPC2_SubsysIdent sid;
  RPC2_Handle cid;
  long rc;
  RPC2_BindParms bp;

  CODA_ASSERT(strlen(hostname) < 64);

  hid.Tag = RPC2_HOSTBYNAME;
  strcpy(hid.Value.Name, hostname);
  pid.Tag = RPC2_PORTALBYINETNUMBER;
  pid.Value.InetPortNumber = port;
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADMONSUBSYSID;

  bp.SecurityLevel = RPC2_OPENKIMONO;
  bp.EncryptionType = (int)NULL;
  bp.SideEffectType = (int)NULL;
  bp.ClientIdent = NULL;
  bp.SharedSecret = NULL;
  rc = RPC2_NewBinding(&hid, &pid, &sid, &bp, &cid);
  if (rc != RPC2_SUCCESS) {
    printf("%s: Cannot connect to machine %s on port %d\n", RPC2_ErrorMsg((int)rc), hostname, port);
    fflush(stdout);
  }
  else {
    handle = cid;
  }
}


void WorkerHandler() {

    CODA_ASSERT(LWP_WaitProcess(&preWorkerSync) == LWP_SUCCESS);
    ReturnConnection();

    while (1) {
        CODA_ASSERT(LWP_WaitProcess(&workerSync) == LWP_SUCCESS);

//	printf("Triggering TokenExpiry\n");
//	fflush(stdout);
//	C_TokensExpired(handle);
    }
}

void CodaDemoHandler() {
    int count = 0;

    while (1) {
    CODA_ASSERT(LWP_WaitProcess(&demoSync) == LWP_SUCCESS);
    ProcessInputFromDemo();
    }
}


/********************************************************************************
 *  RPC Calls accepted by the advice daemon:
 *     NewAdviceService --  an advice monitor makes this call to Venus to inform
 *                          Venus of its existance.
 *     ConnectionAlive -- an advice monitor makes this call to Venus to confirm
 *                        that its connection is alive.
 *     RegisterInterest -- an advice monitor makes this call to Venus to register
 *                         its interest (or disinterest) in certain events.
 *     GetCacheStatistics -- an advice monitor makes this call to Venus to
 *                           request cache statistics information
 *     OutputUsageStatistics -- an advice monitor makes this call to Venus to 
 *                              obtain statistics on fsobj usage during discos
  *     SetParameters -- an advice monitor makes this call to Venus to set internal
 *                      Venus parameters.  (for wizards only)
 *     ResultOfASR -- an advice monitor makes this call to Venus to return the
 *                    result of an ASR Invokation.
 *     ImminentDeath --  an advice monitor makes this call to Venus to inform
 *                       Venus of its impending death.
 ********************************************************************************/

long S_NewAdviceService(RPC2_Handle _cid, RPC2_String hostname, RPC2_Integer userId, 
		      RPC2_Integer port, RPC2_Integer pgrp, RPC2_Integer AdSrvVersion, 
		      RPC2_Integer AdMonVersion, RPC2_Integer *VenusMajorVersionNum, 
		      RPC2_Integer *VenusMinorVersionNum) 
{
  char versionstring[8];
  int rc;

  printf("NewAdviceService: host = %s, userId = %d, port = %d, pgrp = %d\n", 
	 (char *)hostname, (int)userId, (int)port, (int)pgrp);
  fflush(stdout);

  if ((int)AdSrvVersion != ADSRV_VERSION) {
    printf("Version Skew(adsrv.rpc2): AdviceServer=%d, Venus=%d.\n", 
	   (int)AdSrvVersion, ADSRV_VERSION);
    fflush(stdout);
    return RPC2_FAIL;
  }

  if ((int)AdMonVersion != ADMON_VERSION) {
    printf("Version Skew(admon.rpc2):  AdviceServer=%d, Venus=%d.\n", 
	   (int)AdMonVersion, ADMON_VERSION);
    fflush(stdout);
    return RPC2_FAIL;
  }

  rc = NewConnection((char *)hostname, (int)port, (int)pgrp);

  *VenusMajorVersionNum = (RPC2_Integer)VenusMajorVersion;
  *VenusMinorVersionNum = (RPC2_Integer)VenusMinorVersion;

  if (rc == 0)
    return RPC2_SUCCESS;
  else
    return RPC2_FAIL;
;
}

long S_ConnectionAlive(RPC2_Handle _cid, RPC2_Integer userId) {
    printf("ConnectionAlive\n");
    return RPC2_SUCCESS;
}


long S_RegisterInterest(RPC2_Handle _cid, RPC2_Integer userId, long numEvents, InterestValuePair events[]) {
    char interest[128];
    char formatString[64];
  
    printf("RegisterInterest: %d is interested in the following %d items:\n", userId, numEvents);
    sprintf(formatString, "    %%%ds:  <argument=%%d, value=%%d>\n", MAXEVENTLEN);
    for (int i = 0; i < (int)numEvents; i++) {
      printf(formatString, InterestToString(events[i].interest), 
	     events[i].argument, events[i].value);
    }
    printf("\n");
    fflush(stdout);
    return RPC2_SUCCESS;
}


long S_GetCacheStatistics(RPC2_Handle _cid, RPC2_Integer *FilesAllocated, RPC2_Integer *FilesOccupied, RPC2_Integer *BlocksAllocated, RPC2_Integer *BlocksOccupied, RPC2_Integer *RVMAllocated, RPC2_Integer *RVMOccupied) {


    printf("E GetCacheStatistics\n");
    fflush(stdout);

    *FilesAllocated = (RPC2_Integer)6250;
    *FilesOccupied = (RPC2_Integer)390;

    *BlocksAllocated = (RPC2_Integer)50000;
    *BlocksOccupied = (RPC2_Integer)4392;

    *RVMAllocated = (RPC2_Integer)10000;
    *RVMOccupied = (RPC2_Integer)29424;

    printf("L GetCacheStatistics\n");
    fflush(stdout);
}


long S_OutputUsageStatistics(RPC2_Handle _cid, RPC2_Integer userId, RPC2_String pathname) {
    FILE *UsageFILE;

    printf("OutputUsageStatistics %s\n",pathname);
    fflush(stdout);

    UsageFILE = fopen((char *)pathname, "w+");
    if (UsageFILE == NULL) {
      printf("OutputUsageStatistics: Cannot open %s (root probably owns it)\n",
	     (char *)pathname);
      fflush(stdout);
    }
    fprintf(UsageFILE, "<FID> priority discosSinceLastUse discosUsed discosUnused \n");
    fprintf(UsageFILE, "<7f0003cf.212.12c> 0 2 3 2\n");
    fprintf(UsageFILE, "<7f0003cf.d4.f7> 0 0 1 1\n");
    fprintf(UsageFILE, "<7f0003ed.3416.950> 800 2 0 2\n");
    fprintf(UsageFILE, "<7f0003ed.341c.951> 800 2 0 2\n");
    fprintf(UsageFILE, "<7f0003ed.3422.952> 800 2 0 2\n");
    fprintf(UsageFILE, "<7f0003ed.3428.953> 800 2 0 2\n");
    fprintf(UsageFILE, "<7f0003ed.342e.954> 800 2 0 2\n");
    fprintf(UsageFILE, "<7f0003ed.3434.955> 800 2 0 2\n");
    fprintf(UsageFILE, "<7f0003ed.343a.956> 800 2 0 2\n");
    fprintf(UsageFILE, "<7f0003ef.182.62> 0 3 0 4\n");
    fprintf(UsageFILE, "<7f0003ef.fe.4c> 0 2 0 4\n");
    fflush(UsageFILE);
    fclose(UsageFILE);

    return RPC2_SUCCESS;
}

long S_HoardCommands(RPC2_Handle _cid, RPC2_Integer userId, long numCommands, HoardCmd commands[]) {
  char pathname[MAXPATHLEN];
  char PrefixString[64];
  int priority;
  
  printf("HoardCommands: %d has requested the following %d commands:\n", userId, numCommands);

  for (int i = 0; i < (int)numCommands; i++) {
    sprintf(PrefixString, "...%s", HoardCommandToString(commands[i].command));
    switch (commands[i].command) {
        case AddCMD:
	  if (commands[i].meta == NoneMETA)
              printf("%s: path=%s priority=%d", PrefixString, (char *)commands[i].pathname, 
		 (int)commands[i].priority);
	  else
              printf("%s: path=%s priority=%d meta=%s", PrefixString, (char *)commands[i].pathname, 
		 (int)commands[i].priority, MetaInfoIDToString(commands[i].meta));
          break;
        case ClearCMD:
          printf("%s", PrefixString);
          break;
        case DeleteCMD:
          printf("%s: path=%s", PrefixString, (char *)commands[i].pathname);
          break;
        case ListCMD:
          printf("%s: file=%s", PrefixString, (char *)commands[i].pathname);
          break;
        case OffCMD:
          printf("%s", PrefixString);
          break;
        case OnCMD:
          printf("%s", PrefixString);
          break;
        case WalkCMD:
          printf("%s", PrefixString);
          break;
        case VerifyCMD:
          printf("%s: file=%s", PrefixString, (char *)commands[i].pathname);
          break;
        default:
          CODA_ASSERT(1 == 0);
    }
    printf("\n");

  }
  printf("\n");
  fflush(stdout);
  return RPC2_SUCCESS;
}

long S_SetParameters(RPC2_Handle _cid, RPC2_Integer userId, long numParameters, ParameterValuePair parameters[]) {
  int uid;

  printf("SetParameters: \n");
  uid = (int)userId;

  for (int i=0; i < numParameters; i++) {
    switch (parameters[i].parameter) {
      AgeLimit:
        printf("%d set age limit to %d\n", uid, parameters[i].value);
	fflush(stdout);
        break;
      ReintLimit:
        printf("%d set reintegration limit to %d\n", uid, parameters[i].value);
	fflush(stdout);
        break;
      ReintBarrier:
        printf("%d set reintegration barrier to %d\n", uid, parameters[i].value);
	fflush(stdout);
        break;
      WeakThreshold:
        printf("%d set weak threshold to %d\n", uid, parameters[i].value);
	fflush(stdout);
        break;
      default:
        printf("Unknown parameter %d by %d -- ignored", parameters[i].parameter, uid);
	fflush(stdout);
    }
  }
  return RPC2_SUCCESS;

}

long S_ResultOfASR(RPC2_Handle _cid, RPC2_Integer ASRid, RPC2_Integer result) {
  printf("ResultOfASR: ASRid = %d, result = %d\n", ASRid, result);
  fflush(stdout);
  return RPC2_SUCCESS;
}

long S_ImminentDeath(RPC2_Handle _cid, RPC2_String hostname, RPC2_Integer userId, RPC2_Integer port) {
  printf("ImminentDeath: host = %s, userId = %d, port = %d\n", hostname, userId, port);
  fflush(stdout);
  return RPC2_SUCCESS;
}

