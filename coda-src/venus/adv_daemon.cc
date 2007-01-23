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

#include <venusvol.h>
#include "adv_monitor.h"
#include "adv_daemon.h"

int SkkEnabled = 1;
int ASRallowed = 1;

int lastPercentage = 0;

const int AdviceDaemonStackSize = 0x4000; /* 16k */
const int DFLT_MAXAMSERVERS = 1;
const int UNSET_MAXAMSERVERS = -1;

int max_daemons = UNSET_MAXAMSERVERS;

void AdviceInit() {
  RPC2_SubsysIdent sid;

  if (!SkkEnabled) return;

  LOG(100, ("E AdviceInit()\n"));

  /* Initialize Variables */
  if (max_daemons == UNSET_MAXAMSERVERS)
    max_daemons = DFLT_MAXAMSERVERS;

  /* Export the advice service. */
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = VA_SUBSYSID; /* VenusAdv subsystem */
  if (RPC2_Export(&sid) != RPC2_SUCCESS)
    CHOKE("AdviceInit: RPC2_Export failed");

  /* Start up the AM servers. */
  for (int i = 0; i < max_daemons; i++) 
      new adv_daemon;

  LOG(100, ("L AdviceInit()\n"));
}

adv_daemon::adv_daemon(void) :
    vproc("AdviceServer", NULL, VPT_AdviceDaemon, AdviceDaemonStackSize)
{

  LOG(0, ("E adv_daemon::adv_daemon: %-16s\n", name));

  /* Setup filter */
  filter.FromWhom = ONESUBSYS; 
  filter.OldOrNew = OLDORNEW;
  filter.ConnOrSubsys.SubsysId = VA_SUBSYSID; /* VenusAdv subsystem */
  handle = 0;
  packet = 0;

  /* Poke main procedure. */
  start_thread();

  LOG(100, ("L adv_daemon::adv_daemon()\n"));
}

adv_daemon::~adv_daemon() {
  LOG(100, ("adv_daemon::~adv_daemon: %-16s : lwpid = %d\n", name, lwpid));
}

void adv_daemon::main(void)
{
  long code;

  LOG(0, ("adv_daemon::main()\n"));

  while (1) {
    idle = 1;
    LOG(100, ("adv_daemon::GetRequest\n"));
    code = RPC2_GetRequest(&filter, &handle, &packet, NULL, NULL, 0, NULL);
    idle = 0;

    /* Handle RPC2 errors. */
    if (code <= RPC2_WLIMIT)
      LOG(0, ("adv_daemon::main: GetRequest: %s\n", RPC2_ErrorMsg((int) code)));
    if (code <= RPC2_ELIMIT) {
      adv_mon.Reset();
      continue;
    }

    LOG(100, ("Executing the request!\n"));
    code = VenusAdv_ExecuteRequest(handle, packet, NULL);
    if (code <= RPC2_WLIMIT)
      LOG(0, ("adv_daemon::main: ExecuteRequest: %s\n", RPC2_ErrorMsg((int) code)));

    adv_mon.CheckConnection();

    /* Bump sequence number. */
    seq++; 
  }
}

/*  RPC Calls accepted by the advice daemon:
 *
 *  Advice sidekick makes these calls to Venus to
 *
 *     NewAdviceService --  inform Venus of its existance
 *     ConnectionAlive -- confirm that its connection is alive
 *     RegisterInterest -- register its interest (or disinterest) in certain events
 *     GetServerNames -- request a list of server names and their bandwidth estimates
 *     GetCacheStatistics -- request cache statistics information
 *     GetNextHoardWalk -- determine the time of the next scheduled hoard walk
 *     OutputUsageStatistics -- obtain statistics on fsobj usage during discos
 *     HoardCommands -- hand off a list of hoard commands
 *     ResultOfASR -- return the result of an ASR Invokation.
 *     ImminentDeath --  inform Venus of its impending death.
 */

long S_NewAdviceService(RPC2_Handle _cid, RPC2_String hostname, RPC2_Integer userId, RPC2_Integer port, RPC2_Integer pgrp, RPC2_Integer *VenusMajorVersionNum, RPC2_Integer *VenusMinorVersionNum) {
  int rc, max, used;
  PROCESS this_pid;

  LOG(0, ("NewAdviceService: host = %s, userId = %d, port = %d, pgrp = %d\n", (char *)hostname, (int)userId, (int)port, (int)pgrp));

  CODA_ASSERT( LWP_CurrentProcess(&this_pid) == LWP_SUCCESS);
  LWP_StackUsed(this_pid, &max, &used);
  LOG(0, ("NewAdviceService: stack requested = %d, stack used to date = %d \n", max, used));

  rc = adv_mon.NewConnection((char *)hostname, (int)port, (int)pgrp);

  if (rc == RPC2_SUCCESS) {
    adv_mon.InitializeReplacementLog((uid_t)userId);

    static char version[] = PACKAGE_VERSION, *p;

    *VenusMajorVersionNum = strtoul(version, &p, 10);
    *VenusMinorVersionNum = p ? strtoul(++p, NULL, 10) : 0;
  }

  return(rc);
}

long S_ConnectionAlive(RPC2_Handle _cid, RPC2_Integer userId) {
    if (adv_mon.ConnValid()) return RPC2_SUCCESS;
    else return CAENOTVALID;
}

long S_RegisterInterest(RPC2_Handle _cid, RPC2_Integer userId, RPC2_Integer numEvents, InterestValuePair events[]) {
  LOG(0, ("RegisterInterest\n"));
  adv_mon.RegisterInterest((uid_t)userId, numEvents, events);
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
  /*
  userent *u;
  
  LOG(0, ("E OutputUsageStatistics\n"));
  u = FindUser((uid_t)userId);
  if (u == 0)
    return CAENOSUCHUSER;
  u->OutputUsageStatistics((uid_t)userId, (char *)pathname, (int)DisconnectionsSinceLastUse, (int)PercentDisconnectionsUsed, (int)TotalDisconnectionsUsed);
  LOG(0, ("L OutputUsageStatistics\n"));
  */
  return RPC2_SUCCESS;
}

long S_HoardCommands(RPC2_Handle _cid, RPC2_Integer userId, RPC2_Integer numCommands, HoardCmd commands[]) {
    LOG(0, ("E HoardCommands\n"));
    return RPC2_SUCCESS;
}

long S_ResultOfASR(RPC2_Handle _cid, RPC2_Integer realm, VolumeId volume, RPC2_Integer ASRid, RPC2_Integer result) {
#if 0
  volent *vol = NULL;

  LOG(0, ("ResultOfASR: VID = %x.%x, ASRid = %d, result = %d\n",
	  realm, volume, ASRid, result));

  Volid vid;
  vid.Realm = realm;
  vid.Volume = volume;

  vol = VDB->Find(&vid);
  CODA_ASSERT(vol && vol->IsReplicated());
  repvol *vp = (repvol *)vol;

  /* check return from ASR is pending */
  if (!(vp->asr_running())) {
    LOG(0, ("ResultOfASR: No pending ASR\n"));
    return (CAENOASR);
  }
  /* check ASRid matches pid of finished ASR */
  if (ASRid != vp->asr_id()) {
    LOG(0, ("ResultOfASR: Got result from unexpected ASR!\n"));
    return (CAEUNEXPECTEDASR);
  }
  /* unlock the volume */
  vp->unlock_asr();
  vol->release();

#endif

  return RPC2_SUCCESS;
}

long S_ImminentDeath(RPC2_Handle _cid, RPC2_String hostname, RPC2_Integer port) {
  
  LOG(0, ("ImminentDeath: host = %s, userId = %d, port = %d\n", hostname, port));

  /* Don't ObtainWriteLock since we could deadlock if another LWP could be waiting for 
   * advice from the user while holding a write lock.  We'll just switch states from 
   * and clean things up under a writelock later.
   */
  adv_mon.cstate = Dead;

  LOG(100, ("L ImminentDeath\n"));
  return RPC2_SUCCESS;
}
