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

#include "venus.private.h"
#include "venus.version.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <struct.h>
#include <netinet/in.h>
#include <netdb.h>

/* from rvm */
#include <rds.h>

#ifdef __cplusplus
}
#endif __cplusplus


/* interfaces */
#include <adsrv.h>
#include <admon.h>

/* from venus */
#include "tallyent.h"
#include "user.h"
#include "advice.h"
#include "adviceconn.h"
#include "advice_daemon.h"

#define CAESUCCESS RPC2_SUCCESS
// All other CAE return codes are defined in ../rpc2/errordb.txt

const char AdviceSubsys[] = "AdviceSubsys";

int AdviceEnabled = 1;
int ASRallowed = 1;

double totalFetched = 0;
double totalToFetch = 0;
int lastPercentage = 0;

const int AdviceDaemonStackSize = 0x4000; /* 16k */

int MaxAMServers = UNSET_MAXAMSERVERS;

int PATIENCE_ALPHA = UNSET_PATIENCE_ALPHA;
int PATIENCE_BETA = UNSET_PATIENCE_BETA;
int PATIENCE_GAMMA = UNSET_PATIENCE_GAMMA;


void AdviceInit() {
  RPC2_SubsysIdent sid;

  if (!AdviceEnabled) return;

  LOG(100, ("E AdviceInit()\n"));

  /* Initialize Variables */
  if (PATIENCE_ALPHA == UNSET_PATIENCE_ALPHA)
      PATIENCE_ALPHA = DFLT_PATIENCE_ALPHA;
  if (PATIENCE_BETA == UNSET_PATIENCE_BETA)
      PATIENCE_BETA = DFLT_PATIENCE_BETA;
  if (PATIENCE_GAMMA == UNSET_PATIENCE_GAMMA)
      PATIENCE_GAMMA = DFLT_PATIENCE_GAMMA;
  if (MaxAMServers == UNSET_MAXAMSERVERS)
    MaxAMServers = DFLT_MAXAMSERVERS;

  /* Export the advice service. */
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = ADSRVSUBSYSID;
  if (RPC2_Export(&sid) != RPC2_SUCCESS)
    CHOKE("AdviceInit: RPC2_Export failed");

  /* Start up the AM servers. */
  for (int i = 0; i < MaxAMServers; i++) 
    (void) new adviceserver;

  LOG(100, ("L AdviceInit()\n"));
}

adviceserver::adviceserver() :
    vproc("AdviceServer", NULL, VPT_AdviceDaemon, AdviceDaemonStackSize)
{

  LOG(0, ("E adviceserver::adviceserver: %-16s\n", name));

  /* Setup filter */
  filter.FromWhom = ONESUBSYS; 
  filter.OldOrNew = OLDORNEW;
  filter.ConnOrSubsys.SubsysId = ADSRVSUBSYSID; 
  handle = 0;
  packet = 0;

  /* Poke main procedure. */
  start_thread();

  LOG(100, ("L adviceserver::adviceserver()\n"));
}

/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
adviceserver::adviceserver(adviceserver &a) : vproc(*(vproc *)&a) {
   abort();
}


int adviceserver::operator=(adviceserver& a) {
  abort();
  return(0);
}

adviceserver::~adviceserver() {
  LOG(100, ("adviceserver::~adviceserver: %-16s : lwpid = %d\n", name, lwpid));
}

void adviceserver::main(void)
{
  LOG(0, ("adviceserver::main()\n"));

  for (;;) {
    idle = 1;
    LOG(100, ("adviceserver::GetRequest\n"));
    long code = RPC2_GetRequest(&filter, &handle, &packet,
				NULL, NULL, 0, NULL);
    idle = 0;

    /* Handle RPC2 errors. */
    if (code <= RPC2_WLIMIT)
      LOG(0, ("adviceserver::main: GetRequest -> %s\n",
	      RPC2_ErrorMsg((int) code)));
    if (code <= RPC2_ELIMIT) {
      userent *u = FindUserByAdviceHandle(handle);
      if (u) u->admon.Reset();
      else RPC2_Unbind(handle);
      continue;
    }

    LOG(100, ("Executing the request!\n"));
    code = AdSrv_ExecuteRequest(handle, packet, NULL);
    if (code <= RPC2_WLIMIT)
      LOG(0, ("adviceserver::main: ExecuteRequest => %s\n",
	      RPC2_ErrorMsg((int) code)));

    CheckConnections();

    /* Bump sequence number. */
    seq++; 
  }
}

void adviceserver::CheckConnections() {
  user_iterator next;
  userent *u;

  while ((u = next()))
    u->admon.CheckConnection();
}


/********************************************************************************
 *  RPC Calls accepted by the advice daemon:
 *     NewAdviceService --  an advice monitor makes this call to Venus to inform
 *                          Venus of its existance.
 *     ConnectionAlive -- an advice monitor makes this call to Venus to confirm
 *                        that its connection is alive.
 *     RegisterInterest -- an advice monitor makes this call to Venus to register
 * 			   its interest (or disinterest) in certain events.
 *     GetServerNames -- an advice monitor makes this call to Venus to request
 *                       a list of server names and their bandwidth estimates
 *     GetCacheStatistics -- an advice monitor makes this call to Venus to
 *                           request cache statistics information
 *     GetNextHoardWalk -- an advice monitor makes this call to Venus to 
 *                         determine the time of the next scheduled hoard walk
 *     OutputUsageStatistics -- an advice monitor makes this call to Venus to 
 *                              obtain statistics on fsobj usage during discos
 *     HoardCommands -- an advice monitor makes this call to Venus to hand off 
 *			a list of hoard commands.
 *     SetParameters -- an advice monitor makes this call to Venus to set internal
 *                      Venus parameters.  (for wizards only)
 *     ResultOfASR -- an advice monitor makes this call to Venus to return the
 *                    result of an ASR Invokation.
 *     ImminentDeath --  an advice monitor makes this call to Venus to inform
 *                       Venus of its impending death.
 ********************************************************************************/

long S_NewAdviceService(RPC2_Handle _cid, RPC2_String hostname, RPC2_Integer userId, RPC2_Integer port, RPC2_Integer pgrp, RPC2_Integer AdSrvVersion, RPC2_Integer AdMonVersion, RPC2_Integer *VenusMajorVersionNum, RPC2_Integer *VenusMinorVersionNum) {
  userent *u;
  int rc;

  LOG(0, ("NewAdviceService: host = %s, userId = %d, port = %d, pgrp = %d\n", (char *)hostname, (int)userId, (int)port, (int)pgrp));

 {
  PROCESS this_pid;
  int max, used;
  CODA_ASSERT( LWP_CurrentProcess(&this_pid) == LWP_SUCCESS);
  LWP_StackUsed(this_pid, &max, &used);
  LOG(00, ("NewAdviceService: stack requested = %d, stack used to date = %d \n", max, used));
 }

  if ((int)AdSrvVersion != ADSRV_VERSION) {
    LOG(0, ("Version Skew(adsrv.rpc2): AdviceServer=%d, Venus=%d.\n", (int)AdSrvVersion, ADSRV_VERSION));
    return CAEVERSIONSKEW;
  }
  if ((int)AdMonVersion != ADMON_VERSION) {
    LOG(0, ("Version Skew(admon.rpc2):  AdviceServer=%d, Venus=%d.\n", (int)AdMonVersion, ADMON_VERSION));
    return CAEVERSIONSKEW;
  }

  u = FindUser((vuid_t)userId);
  if (u == 0)
    return CAENOSUCHUSER;

  rc = u->NewConnection((char *)hostname, (int)port, (int)pgrp);

  u->InitializeProgramLog((vuid_t)userId);
  u->InitializeReplacementLog((vuid_t)userId);

  *VenusMajorVersionNum = (RPC2_Integer)VenusMajorVersion;
  *VenusMinorVersionNum = (RPC2_Integer)VenusMinorVersion;
  LOG(100, ("L NewAdviceService()\n"));
  return(rc);
}

long S_ConnectionAlive(RPC2_Handle _cid, RPC2_Integer userId) {
    userent *u;

    LOG(100, ("E ConnectionAlive\n"));
    
    /* Find this connection */
    u = FindUser((vuid_t)userId);
    if (u == 0)
        return CAENOSUCHUSER;

    /* Make sure the connection is alive... */
    LOG(100, ("L ConnectionAlive\n"));
    if (u->IsAdviceValid((InterestID)-1,0) == TRUE)
	return RPC2_SUCCESS;
    else
	return CAENOTVALID;
}

long S_RegisterInterest(RPC2_Handle _cid, RPC2_Integer userId, long numEvents, InterestValuePair events[]) {
  userent *u;
  
  LOG(0, ("E RegisterInterest\n"));

  /* Find this connection */
  u = FindUser((vuid_t)userId);
  if (u == 0)
    return CAENOSUCHUSER;

  u->RegisterInterest((vuid_t)userId, numEvents, events);

  LOG(0, ("L RegisterInterest\n"));
  return RPC2_SUCCESS;
}

long S_GetServerInformation(RPC2_Handle _cid, RPC2_Integer maxServers, RPC2_Integer *numServers, ServerEnt *servers) {
    LOG(0, ("E GetServerInformation\n"));

    if (srvent::srvtab == 0) {
      *numServers = 0;
      return(CAENOSERVERS);
    }

    LOG(0, ("GetServerInformation: maxServers = %d\n", maxServers));
    *numServers = 0;
    LOG(0, ("GetServerInformation: numServers = %d\n", *numServers));

    ServerPrint();

    //    *numServers = (long)srvent::srvtab->count();
    
    srv_iterator next;
    srvent *s;
    int i = 0;
    while (((s = next())) && (i < maxServers)) {
	if (s->name != NULL) {
	    servers[i].name = (RPC2_String)malloc(strlen(s->name)+1);
	    if (servers[i].name == NULL)
		return(ENOMEM);
	    strncpy((char *)servers[i].name, s->name, strlen(s->name)+1);
	    servers[i].bw = s->bw;
	    i++;
	}
    }
    *numServers = i;
    LOG(0, ("GetServerInformation: numServers = %d\n", numServers));

    LOG(0, ("L GetServerInformation\n"));
    return(RPC2_SUCCESS);
}

long S_GetCacheStatistics(RPC2_Handle _cid, RPC2_Integer *FilesAllocated, RPC2_Integer *FilesOccupied, RPC2_Integer *BlocksAllocated, RPC2_Integer *BlocksOccupied, RPC2_Integer *RVMAllocated, RPC2_Integer *RVMOccupied) {

    rds_stats_t rdsstats;

    LOG(0, ("E GetCacheStatistics\n"));

    FSDB->GetStats((int *)FilesAllocated, (int *)FilesOccupied, 
		   (int *)BlocksAllocated, (int *)BlocksOccupied);

    // Overwrite Blocks and Files occupied with hoard statistics
    TallySum((int *)BlocksOccupied, (int *)FilesOccupied);


    if (rds_get_stats(&rdsstats) != 0) {
      *RVMAllocated = (RPC2_Integer)(rdsstats.freebytes + rdsstats.mallocbytes);
      *RVMOccupied = (RPC2_Integer)rdsstats.mallocbytes;
    }

    LOG(0, ("L GettCacheStatistics\n"));
    return RPC2_SUCCESS;
}

long S_GetNextHoardWalk(RPC2_Handle _cid, RPC2_Integer *nextHoardWalkTimeSeconds) {
    LOG(0, ("E GetNextHoardWalk\n"));

    *nextHoardWalkTimeSeconds = HDBD_GetNextHoardWalkTime();

    LOG(0, ("L GetNextHoardWalk\n"));
    return RPC2_SUCCESS;
}

long S_OutputUsageStatistics(RPC2_Handle _cid, RPC2_Integer userId, RPC2_String pathname, RPC2_Integer DisconnectionsSinceLastUse, RPC2_Integer PercentDisconnectionsUsed, RPC2_Integer TotalDisconnectionsUsed) {
  userent *u;
  
  LOG(0, ("E OutputUsageStatistics\n"));

  /* Find this connection */
  u = FindUser((vuid_t)userId);
  if (u == 0)
    return CAENOSUCHUSER;

  u->OutputUsageStatistics((vuid_t)userId, (char *)pathname, (int)DisconnectionsSinceLastUse, (int)PercentDisconnectionsUsed, (int)TotalDisconnectionsUsed);

  LOG(0, ("L OutputUsageStatistics\n"));
  return RPC2_SUCCESS;
}

long S_HoardCommands(RPC2_Handle _cid, RPC2_Integer userId, long numCommands, HoardCmd commands[]) {
    LOG(0, ("E HoardCommands\n"));
    return RPC2_SUCCESS;
}

long S_SetParameters(RPC2_Handle _cid, RPC2_Integer userId, long numParameters, ParameterValuePair parameters[]) {
  int uid;

  LOG(0, ("E SetParameters\n"));
  uid = (int)userId;

  if (!AuthorizedUser(uid)) {
    LOG(0, ("adviceconn::SetParameters:  Unauthorized user (%d) attempted to set internal parameters\n", uid));
    return(-1);
  }

  for (int i=0; i < numParameters; i++) {
    switch (parameters[i].parameter) {
      case AgeLimit:
	LOG(100, ("adviceconn::SetParameters:  %d set age limit to %d\n", uid, parameters[i].value));
//      From venusvol.h, volent class
//      unsigned AgeLimit;                  /* min age of log records in SECONDS */

	break;
      case ReintLimit:
	LOG(100, ("adviceconn::SetParameters:  %d set reintegration limit to %d\n", uid, parameters[i].value));
//      From venusvol.h, volent class
//      unsigned ReintLimit;                /* work limit, in MILLESECONDS */
	break;
      case ReintBarrier:
	LOG(100, ("adviceconn::SetParameters:  %d set reintegration barrier to %d\n", uid, parameters[i].value));
	break;
      case WeakThreshold:
	LOG(100, ("adviceconn::SetParameters:  %d set weak threshold to %d\n", uid, parameters[i].value));
        WCThresh = (int)parameters[i].value; /* in Bytes/sec */
	break;
      default:
	LOG(0, ("adviceconn::SetParameters:  Unknown parameter %d by %d -- ignored", parameters[i].parameter, uid));
    }
  }

  LOG(0, ("L SetParameters\n"));
  return RPC2_SUCCESS;
}

//long BeginStoplightMonitor(RPC2_Handle _cid, RPC2_Integer userId) {
//  userent *u;
//
//  LOG(0, ("E BeginStoplightMonitor\n"));
//
  /* Find this connection */
//  u = FindUser((vuid_t)userId);
//  if (u == 0)
//    return CAENOSUCHUSER;

//  u->BeginStoplightMonitor();

//  LOG(0, ("L BeginStoplightMonitor\n"));
//}

//long EndStoplightMonitor(RPC2_Handle _cid, RPC2_Integer userId) {
//  userent *u;

//  LOG(0, ("E EndStoplightMonitor\n"));

  /* Find this connection */
//  u = FindUser((vuid_t)userId);
//  if (u == 0)
//    return CAENOSUCHUSER;

//  u->EndStoplightMonitor();

// LOG(0, ("L EndStoplightMonitor\n"));
//}

long S_ResultOfASR(RPC2_Handle _cid, RPC2_Integer ASRid, RPC2_Integer result) {

  LOG(0, ("ResultOfASR: ASRid = %d, result = %d\n", ASRid, result));
  // check return from ASR is pending 
  if (!ASRinProgress) {
    LOG(0, ("ResultOfASR: No pending ASR\n"));
    return (CAENOASR);
  }
  // check ASRid matches ASRinProgress
  if (ASRid != ASRinProgress) {
    LOG(0, ("ResultOfASR: Got result from unexpected ASR!\n"));
    return (CAEUNEXPECTEDASR);
  }
  // set result of ASR and wake up the right thread
  {
      ASRresult = (int)result;
      VprocSignal((char *)&ASRinProgress, 0);
  }

  // release the ASRinProgress 
  ASRinProgress = 0;

  return RPC2_SUCCESS;
}

long S_ImminentDeath(RPC2_Handle _cid, RPC2_String hostname, RPC2_Integer userId, RPC2_Integer port) {
  userent *u;
  
  LOG(0, ("ImminentDeath: host = %s, userId = %d, port = %d\n", hostname, userId, port));

  /* Find this connection */
  u = FindUser((vuid_t)userId);
  if (u == 0)
    return CAENOSUCHUSER;

  /* 
   * Don't ObtainWriteLock since we could deadlock if another LWP
   * could be waiting for advice from the user while holding a 
   * write lock.  We'll just switch states from AdviceValid to 
   * AdviceDying and clean things up under a writelock later.
   */
  u->SetAdviceState(AdviceDying);

  LOG(100, ("L ImminentDeath\n"));
  return RPC2_SUCCESS;
}


/* The advice daemon offers routines to inform all users of certain events. */

void NotifyUsersOfServerUpEvent(char *name) {
    user_iterator next;
    userent *u;

  LOG(0, ("NotifyUserOfServerUpEvent\n"));
    while ((u = next())) 
        u->ServerAccessible(name);
}

void NotifyUsersOfServerDownEvent(char *name) {
    user_iterator next;
    userent *u;

  LOG(0, ("NotifyUserOfServerDownEvent\n"));
    while ((u = next()))
        u->ServerInaccessible(name);
}

void NotifyUsersOfServerWeakEvent(char *name) {
    user_iterator next;
    userent *u;

  LOG(0, ("NotifyUserOfServerWeakEvent\n"));
    while ((u = next()))
        u->ServerConnectionWeak(name);
}

void NotifyUsersOfServerStrongEvent(char *name) {
    user_iterator next;
    userent *u;

  LOG(0, ("NotifyUserOfServerStrongEvent\n"));
    while ((u = next())) 
        u->ServerConnectionStrong(name);
}

void NotifyUsersOfServerBandwidthEvent(char *name, long bandwidth) {
    user_iterator next;
    userent *u;

  LOG(0, ("NotifyUserOfServerBandwidthEvent\n"));
    while ((u = next())) 
        u->ServerBandwidthEstimate(name, bandwidth);
}

void NotifyUsersOfHoardWalkBegin() {
    user_iterator next;
    userent *u;

    lastPercentage = 0;
    totalToFetch = 0;
    totalFetched = 0;
    while ((u = next()))
        u->HoardWalkBegin();
}

void NotifyUsersOfHoardWalkProgress(int fetched, int total) {
    user_iterator next;
    userent *u;
    int thisPercentage;

    totalFetched += fetched;
    if (totalToFetch == 0)
      totalToFetch = total;
    CODA_ASSERT(total == totalToFetch);

    if (total != 0)
	thisPercentage = (int) ((double)totalFetched*(double)100/(double)total);
    else
	thisPercentage = 100;

    if (thisPercentage < lastPercentage) {
      LOG(0, ("fetched=%d, totalFetched=%d, totalToFetch=%d, thisPercentage=%d\n", 
	      fetched, totalFetched, total, thisPercentage));
      LOG(0, ("NotifyUsersOfHoardWalkProgress: percentage decreasing!\n"));
      }
    if (thisPercentage == lastPercentage)
      return;
    lastPercentage = thisPercentage;

    LOG(0, ("NotifyUsersOfHoardWalkProgress(%d)\n", thisPercentage));
    MarinerLog("progress::hoarding %dx\n", thisPercentage);
    while ((u = next()))
        u->HoardWalkStatus(thisPercentage);
}

void NotifyUsersOfHoardWalkEnd() {
    user_iterator next;
    userent *u;

    while ((u = next()))
        u->HoardWalkEnd();
    lastPercentage = 0;
    totalToFetch = 0;
    totalFetched = 0;
}

void NotifyUsersOfHoardWalkPeriodicOn() {
    user_iterator next;
    userent *u;

    while ((u = next()))
        u->HoardWalkPeriodicOn();
}

void NotifyUsersOfHoardWalkPeriodicOff() {
    user_iterator next;
    userent *u;

    while ((u = next()))
        u->HoardWalkPeriodicOff();
}

void NotifyUsersObjectInConflict(char *path, ViceFid *key) {
    user_iterator next;
    userent *u;

    while ((u = next()))
        u->NotifyObjectInConflict(path, key);
}

void NotifyUsersObjectConsistent(char *path, ViceFid *key) {
    user_iterator next;
    userent *u;

    while ((u = next()))
        u->NotifyObjectConsistent(path, key);
}

#define MAXTASKS 88
void NotifyUsersTaskAvailability() {
    user_iterator next;
    userent *u;

    while ((u = next())) {
        TallyInfo tallyInfo[MAXTASKS];
	int ti = 0;

        dlist_iterator next_tallyent(*TallyList);
        dlink *d;
        while ((d = next_tallyent())) {
 	    tallyent *te = strbase(tallyent, d, prioq_handle);
	    if (te->vuid != u->GetUid()) continue;

	    /* This tallyent is for our user */
	    tallyInfo[ti].TaskPriority = te->priority;
	    tallyInfo[ti].AvailableBlocks = te->available_blocks;
	    tallyInfo[ti].UnavailableBlocks = te->unavailable_blocks;
	    tallyInfo[ti].IncompleteInformation = te->incomplete;
	    ti++;
        }

        u->NotifyTaskAvailability(ti, tallyInfo);
    }
}

void NotifyUsersOfKillEvent(dlist *hdb_bindings, int blocks) {
  dlist_iterator next_hdbent(*hdb_bindings);
  dlink *d;

  LOG(100, ("NotifyUsersOfKillEvent(xx, %d)\n", blocks));

  if (hdb_bindings == NULL)
    return;

  LOG(0, ("NotifyUsersOfKillEvent: hdb_bindings != NULL\n"));

  while ((d = next_hdbent())) {
    CODA_ASSERT(d != NULL);
    binding *b = strbase(binding, d, bindee_handle);
    if (b == NULL)
      LOG(0, ("b is null\n"));
    fflush(logFile);
    CODA_ASSERT(b != NULL);
    namectxt *nc = (namectxt *)b->binder;
    if (nc == NULL)
      LOG(0, ("nc is NULL\n"));
    fflush(logFile);
    CODA_ASSERT(nc != NULL);
    userent *u = FindUser(nc->vuid);

    if (u == NULL) {
      LOG(0, ("NotifyUsersOfKillEvent: u is NULL\n"));
      continue;
    }
    fflush(logFile);
    CODA_ASSERT(u != NULL);

    u->NotifyTaskUnavailable(nc->priority, blocks);
  }

}

void NotifyUserOfProgramAccess(vuid_t uid, int pid, int pgid, ViceFid *key) {
    userent *u;

    GetUser(&u, uid);
    CODA_ASSERT(u != NULL);

    if (!u->IsAdvicePGID(pgid))
	u->LogProgramAccess(pid, pgid, key);
}

void NotifyUserOfReplacement(ViceFid *fid, char *path, int status, int data) {
    userent *u;

    LOG(0, ("Replacement: <%x.%x.%x> %s %d %d\n",
	    fid->Volume, fid->Vnode, fid->Unique,
	    path, status, data));

    GetUser(&u, PrimaryUser);
    if (u != NULL)
      u->LogReplacement(path, status, data);
}

void SwapProgramLogs() {
    user_iterator next;
    userent *u;

    while ((u = next())) 
        u->SwapProgramLog();
}

void SwapReplacementLogs() {
    user_iterator next;
    userent *u;

    while ((u = next())) 
        u->SwapReplacementLog();
}
