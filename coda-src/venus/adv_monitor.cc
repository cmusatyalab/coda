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

#include "adv_monitor.h"

adv_monitor adv_mon;

const char *InterestToString(InterestID);

adv_monitor::adv_monitor() {
  LOG(100, ("adv_monitor::adv_monitor()\n"));
  Lock_Init(&userLock);
  Reset(1);
}

adv_monitor::~adv_monitor() {
  LOG(100, ("adv_monitor::~adv_monitor()\n"));
}

void adv_monitor::TokensAcquired(int expirationTime) {
  long rc;

  InterestID callType = TokensAcquiredID;
  if (!ConnValid() || (!InterestArray[callType])) return;

  LOG(100, ("E adv_monitor::TokensAcquired(%d)\n", expirationTime));
  ObtainWriteLock(&userLock);
  rc = C_TokensAcquired(handle, (RPC2_Integer)expirationTime);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);
  LOG(100, ("L adv_monitor::TokensAcquired()\n"));
}

void adv_monitor::TokensExpired() {
  long rc;

  InterestID callType = TokensExpiredID;
  if (!ConnValid() || (!InterestArray[callType])) return;

  LOG(100, ("E adv_monitor::TokensExpired()\n"));
  ObtainWriteLock(&userLock);
  rc = C_TokensExpired(handle);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);
  LOG(100, ("L adv_monitor::TokensExpired()\n"));
}

void adv_monitor::ServerAccessible(char *name) {
  long rc;

  InterestID callType = ServerAccessibleID;
  if (!ConnValid() || (!InterestArray[callType])) return;

  LOG(10, ("E adv_monitor::ServerAccessible()\n"));
  ObtainWriteLock(&userLock);
  rc = C_ServerAccessible(handle, (RPC2_String)name);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);
  LOG(10, ("L adv_monitor::ServerAccessible()\n"));
}

void adv_monitor::ServerInaccessible(char *name) {
  long rc;
  InterestID callType = ServerInaccessibleID;
  if (!ConnValid() || (!InterestArray[callType])) return;

  LOG(10, ("E adv_monitor::ServerInaccessible()\n"));
  ObtainWriteLock(&userLock);
  rc = C_ServerInaccessible(handle, (RPC2_String)name);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);

  LOG(10, ("L adv_monitor::ServerInaccessible()\n"));
}

void adv_monitor::ServerConnectionWeak(char *name) {
  long rc;

  InterestID callType = ServerConnectionWeakID;
  if (!ConnValid() || (!InterestArray[callType])) return;

  LOG(10, ("E adv_monitor::ServerConnectionWeak()\n"));
  ObtainWriteLock(&userLock);
  rc = C_ServerConnectionWeak(handle, (RPC2_String)name);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);
  LOG(10, ("L adv_monitor::ServerConnectionWeak()\n"));
}

void adv_monitor::ServerConnectionStrong(char *name) {
  long rc;

  InterestID callType = ServerConnectionStrongID;
  if (!ConnValid() || (!InterestArray[callType])) return;

  LOG(10, ("E adv_monitor::ServerConnectionStrong()\n"));
  ObtainWriteLock(&userLock);
  rc = C_ServerConnectionStrong(handle, (RPC2_String)name);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);
  LOG(10, ("L adv_monitor::ServerConnectionStrong()\n"));
}

void adv_monitor::ServerBandwidthEstimate(char *name, long bandwidth) {
  long rc;

  QualityEstimate serverList[1];
  InterestID callType = NetworkQualityEstimateID;
  if (!ConnValid() || (!InterestArray[callType])) return;

  serverList[0].ServerName = (RPC2_String)name;
  serverList[0].BandwidthEstimate = (RPC2_Integer)bandwidth;
  serverList[0].Intermittent = False;
  LOG(10, ("E adv_monitor::ServerBandwidthEstimate()\n"));
  ObtainWriteLock(&userLock);
  rc = C_NetworkQualityEstimate(handle, 1, serverList);
  ReleaseWriteLock(&userLock);

  CheckError(rc, callType);
  LOG(10, ("L adv_monitor::ServerBandwidthEstimate()\n"));
}

int adv_monitor::RequestASRInvokation(repvol *vol, char *pathname, vuid_t vuid)
{
  long rc;
  RPC2_Integer ASRid;
  RPC2_Integer ASRrc;

  LOG(1, ("adv_monitor::RequestASRInvokation(%s, %d)\n", pathname, vuid));
  InterestID callType = InvokeASRID;
  if (!InterestArray[callType]) return(0);

  vol->lock_asr();
#warning "ASR realm"
  rc = C_InvokeASR(handle, (RPC2_String)pathname, vol->GetVolumeId(), vuid, &ASRid, &ASRrc);
  CheckError(rc, callType); /* resets connection and unlocks asr if RPC2 fails */

  if (rc == RPC2_SUCCESS) {
    if (ASRrc != SKK_SUCCESS) {
      vol->unlock_asr();
      LOG(1, ("adv_monitor ASR could not be invoked (%d)\n", ASRrc));
      return(-1);
    }
    vol->asr_id(ASRid); /* give ASR permission to mess with volume in conflict */
  }
  return(rc);
}

int adv_monitor::Spike(int cmd) {
  long rc;
  
  if (cmd) { /* ping the sidekick */
    rc = C_Spike(handle, cmd);
    if (rc == RPC2_SUCCESS)
      return(1);
  }
  else /* tell the sidekick to die */
    rc = C_Spike(handle, cmd);

  Reset(); /* sidekick is dead, reset the connection to it */
  return(0);
}

int adv_monitor::NewConnection(char *hostName, int portNumber, int pgrp) {
  char myHostName[MAXHOSTNAMELEN+1];

  CODA_ASSERT(strlen(hostName) <= MAXHOSTNAMELEN);

  LOG(100, ("adv_monitor::NewConnection(%s, %d,  %d)\n", hostName, portNumber, pgrp));
  if (AdviceOutstanding()) {
    LOG(0, ("Cannot start a new advice monitor while a request for advice is outstanding!\n"));
    return(CAEADVICEPENDING);
  }

  ObtainWriteLock(&userLock);

  switch (cstate) {
    case Valid:
      if (Spike(1)) {
	LOG(0, ("adv_monitor::NewConnection:  denied since old sidekick is responsive\n"));
	ReleaseWriteLock(&userLock);
	return(EBUSY);
      }
      break;
    case Init:
    case Dead:
      LOG(0, ("adv_monitor::NewConnection:  denied since old sidekick is transitioning\n"));
      ReleaseWriteLock(&userLock);
      return(EAGAIN);
      break;
    case Nil:
    default:
      LOG(0, ("adv_monitor::NewConnection:  old sidekick is dead, making new connection\n"));
      break;
  }

  strcpy(hostname, hostName);
  int hostlen = gethostname(myHostName, MAXHOSTNAMELEN+1);
  CODA_ASSERT(hostlen != -1);
  CODA_ASSERT(myHostName != NULL);
  CODA_ASSERT(hostname != NULL);
  CODA_ASSERT((strncmp(hostname, "localhost", strlen("localhost")) == 0) ||
	 (strncmp(hostname, myHostName, MAXHOSTNAMELEN)) == 0);

  port = (unsigned short) portNumber;
  pgid = pgrp;

  cstate = Init;
  ReleaseWriteLock(&userLock);

  return RPC2_SUCCESS;
}

int adv_monitor::RegisterInterest(vuid_t uid, long numEvents, InterestValuePair events[])
{
    char formatString[MAXEVENTLEN];

    LOG(0, ("adv_monitor::RegisterInterest: %d is interested in the following %d items:\n", 
	    uid, numEvents));
    sprintf(formatString, "    %%%ds:  <argument=%%d, value=%%d>\n", MAXEVENTLEN);

    for (int i = 0; i < numEvents; i++) {
        LOG(0, (formatString, InterestToString(events[i].interest), 
		events[i].argument, events[i].value));
	InterestArray[events[i].interest] = events[i].value;
	if (events[i].interest == HoardWalkAdviceRequestID) {
	    if (events[i].value == 0) 
	      HDB->SetSolicitAdvice(-1);
	    else if (events[i].value == 1)
	      HDB->SetSolicitAdvice(uid);
	    else
	      LOG(0, ("adv_monitor::RegisterInterest:  Unknown value (%d) for HoardWalkAdviceRequest -- ignored\n", events[i].value));
	}
    }

    return(0);
}

void adv_monitor::InitializeProgramLog(vuid_t uid) {
    char UserSpoolDir[MAXPATHLEN];

    if (programFILE != NULL)
      return;

    MakeUserSpoolDir(UserSpoolDir, uid);
    snprintf(programLogName, MAXPATHLEN, "%s/%s", UserSpoolDir, PROGRAMLOG);
    unlink(programLogName);
    LOG(0, ("Opening %s\n", programLogName));

    programFILE = fopen(programLogName, "a");
    if (programFILE == NULL) 
      LOG(0, ("InitializeProgramLog(%d) failed\n", uid));

    numLines = 0;
    return;
}

void adv_monitor::SwapProgramLog() {
    char oldName[MAXPATHLEN];

    if (programFILE == NULL) return;

    fflush(programFILE);
    snprintf(oldName, MAXPATHLEN, "%s.old", programLogName);
    LOG(0, ("Moving %s to %s\n", programLogName, oldName));

    if (rename(programLogName, oldName) < 0) {
        LOG(0, ("rename(%s, %s) failed (%d)\n",
		programLogName, oldName, errno));
        return;
    }
    freopen(programLogName, "a", programFILE);
    resetpid();
    numLines = 0;
}

void adv_monitor::LogProgramAccess(int pid, int pgid, VenusFid *fid) {
    if (programFILE != NULL) {
      /*       outputcommandname(programFILE, pgid);     
	   s/pgid/pid/   Why was the pgid being passed here???  -Remi */
        outputcommandname(programFILE, pid);
        fprintf(programFILE, "%d %d %s\n", pid, pgid, FID_(fid));

	if (++numLines > MAX_LOGFILE_LINES) 
	    SwapProgramLog();
    }
}

void adv_monitor::InitializeReplacementLog(vuid_t uid) {
    char UserSpoolDir[MAXPATHLEN];

    if (replacementFILE != NULL) return;

    MakeUserSpoolDir(UserSpoolDir, uid);
    snprintf(replacementLogName, MAXPATHLEN, "%s/%s", 
	     UserSpoolDir, REPLACEMENTLOG);
    (void) unlink(replacementLogName);
    LOG(0, ("Opening %s\n", replacementLogName));

    replacementFILE = fopen(replacementLogName, "a");
    if (replacementFILE == NULL) 
      LOG(0, ("InitializeReplacementLog(%d) failed\n", uid));

    numLines = 0;
}

void adv_monitor::SwapReplacementLog() {
    char oldName[MAXPATHLEN];

    if (replacementFILE == NULL) return;

    fflush(replacementFILE);
    snprintf(oldName, MAXPATHLEN, "%s.old", replacementLogName);
    LOG(0, ("Moving %s to %s\n", replacementLogName, oldName));

    if (rename(replacementLogName, oldName) < 0) {
        LOG(0, ("rename(%s, %s) failed (%d)\n",
	        replacementLogName, oldName, errno));
        return;
    }

    freopen(replacementLogName, "a", replacementFILE);
    resetpid();
    numLines = 0;
}

void adv_monitor::LogReplacement(char *path, int status, int data) {
    if (replacementFILE != NULL) {
        fprintf(replacementFILE, "%s %d %d\n", path, status, data);

	if (++numLines > MAX_REPLACEMENTLOG_LINES) 
	    SwapReplacementLog();
    }
}

void adv_monitor::CheckConnection() {
  LOG(100, ("adv_monitor::CheckConnection(): state = %d\n", cstate));
  if (cstate == Dead) DestroyConnection();
  else if (cstate == Init) ReturnConnection();
}

void adv_monitor::ReturnConnection() {
  RPC2_HostIdent hid;
  RPC2_PortIdent pid;
  RPC2_SubsysIdent sid;
  RPC2_Handle cid;
  long rc;
  RPC2_BindParms bp;

  LOG(100, ("E adv_monitor:ReturnConnection:  return connection to %s on port %d\n", hostname, port));

  CODA_ASSERT(cstate == Init);
  CODA_ASSERT(strlen(hostname) < 64);

  ObtainWriteLock(&userLock);

  hid.Tag = RPC2_HOSTBYNAME;
  strcpy(hid.Value.Name, hostname);
  pid.Tag = RPC2_PORTBYINETNUMBER;
  pid.Value.InetPortNumber = port;
  sid.Tag = RPC2_SUBSYSBYID;
  sid.Value.SubsysId = AS_SUBSYSID; /* AdvSkk subsystem */

  bp.SecurityLevel = RPC2_OPENKIMONO;
  bp.EncryptionType = 0;
  bp.SideEffectType = 0;
  bp.ClientIdent = NULL;
  bp.SharedSecret = NULL;
  rc = RPC2_NewBinding(&hid, &pid, &sid, &bp, &cid);
  if (rc != RPC2_SUCCESS) {
    LOG(0, ("%s: Cannot connect to machine %s on port %d\n",
	    RPC2_ErrorMsg((int)rc), hostname, port));
    Reset();
  }
  else {
    cstate = Valid;
    handle = cid;
  }

  ReleaseWriteLock(&userLock);
  LOG(100, ("L adv_monitor::ReturnConnection()\n"));
}

void adv_monitor::DestroyConnection() {
  CODA_ASSERT(cstate == Dead);
  LOG(100, ("adv_monitor::TearDownConnection()\n"));
  Reset();
}

void adv_monitor::CheckError(long rpc_code, InterestID callType) {

    switch (rpc_code) {
      case RPC2_SUCCESS:
      case RPC2_CONNBUSY:
        break;
      case RPC2_FAIL:
      case RPC2_NOCONNECTION:
      case RPC2_TIMEOUT:
      case RPC2_DEAD:
      default:
        LOG(0, ("ADV_SKK call failed: %s\n", RPC2_ErrorMsg((int)rpc_code)));
        Print(logFile);
        Reset();
        break;
    }
}

void adv_monitor::Reset(int init) {

  LOG(100, ("adv_monitor::Reset()\n"));
  cstate = Nil;
  if (handle != -1) {
    (void) RPC2_Unbind(handle);
    handle = -1;
  }
  port = 0;
  memset(hostname, 0, MAXHOSTNAMELEN);
  for (int i = 0; i < MAXEVENTS; i++)
    InterestArray[i] = 0;
  programFILE = NULL;
  replacementFILE = NULL;

  if (!init) { /* clear all asr_running flags */
    repvol_iterator next;
    repvol *v;
    while ((v = next())) {
      if (v->asr_running())
	v->unlock_asr();
    }
  }
}

void adv_monitor::Print() {
  Print(stdout);
}

void adv_monitor::Print(FILE *fp) {
  fflush(fp);
  Print(fileno(fp));
  fflush(fp);
}

void adv_monitor::Print(int afd) {

  if (LogLevel < 100) return;

  LOG(100, ("adv_monitor::Print()\n")); fflush(logFile);

  fdprint(afd, "%#08x : Connection = %d, Host = %s, Port = %d, Handle = %d\n", 
          (long)this, cstate, hostname, port, handle);
}

const char *InterestStrings[] = {
/* 0*/  "TokensAcquired",
        "TokensExpired",
        "ActivityPendingTokens",
        "SpaceInformation",
	"ServerAccessible",
/* 5*/  "ServerInaccesible",
        "ServerConnectionStrong",
        "ServerConnectionWeak",
        "NetworkQualityEstimate",
        "VolumeTransitionEvent",
/*10*/  "Reconnection",
        "DataFetchEvent",
        "ReadDisconnectedCacheMissEvent",
        "WeaklyConnectedCacheMissEvent",
        "DisconnectedCacheMissEvent",
/*15*/  "HoardWalkAdviceRequest",
        "HoardWalkBegin",
        "HoardWalkStatus",
        "HoardWalkEnd",
        "HoardWalkPeriodicOn",
/*20*/  "HoardWalkPeriodicOff",
        "ObjectInConflict",
        "ObjectConsistent",
        "ReintegrationPendingTokens",
        "ReintegrationEnabled",
/*25*/  "ReintegrationActive",
        "ReintegrationCompleted",
        "TaskAvailability",
        "TaskUnavailable",
        "ProgramAccessLogs",
/*30*/  "ReplacementLogs",
        "InvokeASR",
        "UnknownEventString"
};

const char *InterestToString(InterestID interest)
{
    if (interest < 0 || interest >= MAXEVENTS) {
        LOG(0, ("InterestToString: Unrecognized Event ID = %d\n", interest));
        interest = (InterestID)MAXEVENTS; /* MAXEVENTS = "UnknownEventString" */
    }
    return(InterestStrings[interest]);
}
