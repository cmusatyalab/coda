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

static char *rcsid = "$Header: /home/braam/src/coda-src/venus/RCS/advice_daemon.cc,v 1.1 1996/11/22 19:12:03 braam Exp braam $";
#endif /*_BLURB_*/




#include "venus.private.h"
#include "venus.version.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <netinet/in.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "user.h"
#include "simulate.h"
#include "advice.h"
#include "adviceconn.h"
#include "advice_daemon.h"
#include "adsrv.h"
#include "admon.h"


const char AdviceSubsys[] = "AdviceSubsys";

int AdviceEnabled = 1;
int ASRallowed = 1;

const int AdviceDaemonStackSize = 8192;

int MaxAMServers = UNSET_MAXAMSERVERS;

int PATIENCE_ALPHA = UNSET_PATIENCE_ALPHA;
int PATIENCE_BETA = UNSET_PATIENCE_BETA;
int PATIENCE_GAMMA = UNSET_PATIENCE_GAMMA;


void AdviceInit() {
  RPC2_SubsysIdent sid;

  if (Simulating) return;
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
    Choke("AdviceInit: RPC2_Export failed");

  /* Start up the AM servers. */
  for (int i = 0; i < MaxAMServers; i++) 
    (void) new adviceserver;

  LOG(100, ("L AdviceInit()\n"));
}

adviceserver::adviceserver() : vproc("AdviceServer", (PROCBODY) &adviceserver::main, VPT_AdviceDaemon, AdviceDaemonStackSize) {

  LOG(100, ("E adviceserver::adviceserver: %-16s\n", name));

  /* Setup filter */
  filter.FromWhom = ONESUBSYS; 
  filter.OldOrNew = OLDORNEW;
  filter.ConnOrSubsys.SubsysId = ADSRVSUBSYSID; 
  handle = 0;
  packet = 0;

  /* Poke main procedure. */
  VprocSignal((char *)this, 1);

  LOG(100, ("L adviceserver::adviceserver()\n"));
}

/* 
 * we don't support assignments to objects of this type.
 * bomb in an obvious way if it inadvertently happens.
 */
adviceserver::adviceserver(adviceserver &a) : vproc(*((vproc *)&a)) {
   abort();
}


adviceserver::operator=(adviceserver& a) {
  abort();
  return(0);
}

adviceserver::~adviceserver() {
  LOG(100, ("adviceserver::~adviceserver: %-16s : lwpid = %d\n", name, lwpid));
}

void adviceserver::main(void *parm) {
  /* Wait for ctor to poke us. */
  VprocWait((char *)this);

  for (;;) {
    idle = 1;
    LOG(100, ("adviceserver::GetRequest\n"));
    long code = RPC2_GetRequest(&filter, &handle, &packet,
				NULL, NULL, NULL, NULL);
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

  while (u = next()) 
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
 *     SetParameters -- an advice monitor makes this call to Venus to set internal
 *                      Venus parameters.  (for wizards only)
 *     ResultOfASR -- an advice monitor makes this call to Venus to return the
 *                    result of an ASR Invokation.
 *     ImminentDeath --  an advice monitor makes this call to Venus to inform
 *                       Venus of its impending death.
 ********************************************************************************/

long NewAdviceService(RPC2_Handle _cid, RPC2_String hostname, RPC2_Integer userId, RPC2_Integer port, RPC2_Integer pgrp, RPC2_Integer AdSrvVersion, RPC2_Integer AdMonVersion, RPC2_Integer *VenusMajorVersionNum, RPC2_Integer *VenusMinorVersionNum) {
  char versionstring[8];
  userent *u;
  int rc;

  LOG(0, ("NewAdviceService: host = %s, userId = %d, port = %d, pgrp = %d\n", (char *)hostname, (int)userId, (int)port, (int)pgrp));

 {
  PROCESS this_pid;
  int max, used;
  assert( LWP_CurrentProcess(&this_pid) == LWP_SUCCESS);
  LWP_StackUsed(this_pid, &max, &used);
  LOG(10, ("NewAdviceService: stack requested = %d, stack used to date = %d \n", max, used));
 }

  if ((int)AdSrvVersion != ADSRV_VERSION) {
    LOG(0, ("Version Skew(adsrv.rpc2): AdviceServer=%d, Venus=%d.\n", (int)AdSrvVersion, ADSRV_VERSION));
    return RPC2_FAIL;
  }
  if ((int)AdMonVersion != ADMON_VERSION) {
    LOG(0, ("Version Skew(admon.rpc2):  AdviceServer=%d, Venus=%d.\n", (int)AdMonVersion, ADMON_VERSION));
    return RPC2_FAIL;
  }

  u = FindUser((vuid_t)userId);
  if (u == 0)
    return RPC2_FAIL;
  LOG(100, ("FindUser succeeded!\n"));

  rc = u->NewConnection((char *)hostname, (int)port, (int)pgrp);

  *VenusMajorVersionNum = (RPC2_Integer)VenusMajorVersion;
  *VenusMinorVersionNum = (RPC2_Integer)VenusMinorVersion;
  LOG(100, ("L NewAdviceService()\n"));
  if (rc == 0)
    return RPC2_SUCCESS;
  else
    return RPC2_FAIL;
}

long ConnectionAlive(RPC2_Handle _cid, RPC2_Integer userId) {
    userent *u;

    LOG(100, ("E ConnectionAlive\n"));
    
    /* Find this connection */
    u = FindUser((vuid_t)userId);
    if (u == 0)
        return RPC2_FAIL;

    /* Make sure the connection is alive... */
    LOG(100, ("L ConnectionAlive\n"));
    if (u->IsAdviceValid(0) == TRUE)
	return RPC2_SUCCESS;
    else
	return RPC2_FAIL;
}

long RegisterInterest(RPC2_Handle _cid, RPC2_Integer userId, long numEvents, InterestValuePair events[]) {
  userent *u;
  
  LOG(0, ("E RegisterInterest\n"));

  /* Find this connection */
  u = FindUser((vuid_t)userId);
  if (u == 0)
    return RPC2_FAIL;

  u->RegisterInterest((vuid_t)userId, numEvents, events);

  LOG(0, ("L RegisterInterest\n"));
  return RPC2_SUCCESS;
}

long SetParameters(RPC2_Handle _cid, RPC2_Integer userId, long numParameters, ParameterValuePair parameters[]) {
  userent *u;
  int uid;

  LOG(0, ("E SetParameters\n"));
  uid = (int)userId;

  if (!AuthorizedUser(uid)) {
    LOG(0, ("adviceconn::SetParameters:  Unauthorized user (%d) attempted to set internal parameters\n", uid));
    return(-1);
  }

  for (int i=0; i < numParameters; i++) {
    switch (parameters[i].parameter) {
      AgeLimit:
	LOG(100, ("adviceconn::SetParameters:  %d set age limit to %d\n", uid, parameters[i].value));
//      From venusvol.h, volent class
//      unsigned AgeLimit;                  /* min age of log records in SECONDS */

	break;
      ReintLimit:
	LOG(100, ("adviceconn::SetParameters:  %d set reintegration limit to %d\n", uid, parameters[i].value));
//      From venusvol.h, volent class
//      unsigned ReintLimit;                /* work limit, in MILLESECONDS */
	break;
      ReintBarrier:
	LOG(100, ("adviceconn::SetParameters:  %d set reintegration barrier to %d\n", uid, parameters[i].value));
	break;
      WeakThreshold:
	LOG(100, ("adviceconn::SetParameters:  %d set weak threshold to %d\n", uid, parameters[i].value));
        extern long WCThresh;
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
//    return RPC2_FAIL;

//  u->BeginStoplightMonitor();

//  LOG(0, ("L BeginStoplightMonitor\n"));
//}

//long EndStoplightMonitor(RPC2_Handle _cid, RPC2_Integer userId) {
//  userent *u;

//  LOG(0, ("E EndStoplightMonitor\n"));

  /* Find this connection */
//  u = FindUser((vuid_t)userId);
//  if (u == 0)
//    return RPC2_FAIL;

//  u->EndStoplightMonitor();

// LOG(0, ("L EndStoplightMonitor\n"));
//}

long ResultOfASR(RPC2_Handle _cid, RPC2_Integer ASRid, RPC2_Integer result) {

  LOG(0, ("ResultOfASR: ASRid = %d, result = %d\n", ASRid, result));
  // check return from ASR is pending 
  if (!ASRinProgress) {
    LOG(0, ("ResultOfASR: No pending ASR\n"));
    return (RPC2_FAIL);
  }
  // check ASRid matches ASRinProgress
  if (ASRid != ASRinProgress) {
    LOG(0, ("ResultOfASR: Got result from unexpected ASR!\n"));
    return (RPC2_FAIL);
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

long ImminentDeath(RPC2_Handle _cid, RPC2_String hostname, RPC2_Integer userId, RPC2_Integer port) {
  userent *u;
  
  LOG(0, ("ImminentDeath: host = %s, userId = %d, port = %d\n", hostname, userId, port));

  /* Find this connection */
  u = FindUser((vuid_t)userId);
  if (u == 0)
    return RPC2_FAIL;

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

