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

#include "adv_sidekick.h"

struct Lock plock;
int err = 0, reqcnt = 0;
int vmajor, vminor;
RPC2_Handle VenusCID = -1;
FILE *logfile = NULL;
int workers = 0, lwp_ready = 0;
RPC2_Handle cid;
SE_Descriptor *se = NULL;
struct pnode *phead = NULL;

int main(int argc, char **argv) {
  RPC2_RequestFilter reqfilter;
  RPC2_PacketBuffer *reqbuffer;
  char hostname[MAXHOSTNAMELEN];
  int rc, uid, pid, pgid;
  PROCESS lwpid;
  struct pnode *ptmp;
  struct stat sbuf;


  /* Initialization */
  Lock_Init(&plock);
  if (parse_cmd_line(argc, argv) < 0)
    quit("usage: %s [-log <filename>] [-err]", argv[0])
  if ((logfile == NULL) && ((logfile = fopen(DEF_LOGFILE, "a")) == NULL))
    quit("%s\nCannot open %s for writing", strerror(errno), DEF_LOGFILE)
  if ((rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &lwpid)) != LWP_SUCCESS)
    quit("Could not initialize LWP (%d)", rc)
  if ((mkdir("/tmp/.asrlogs", 0755) < 0) && 
      ((errno != EEXIST) || (stat("/tmp/.asrlogs", &sbuf) < 0) || (!S_ISDIR(sbuf.st_mode))))
    quit("Could not create asr log directory");

  /* Get necessary information */
  if (gethostname(hostname, MAXHOSTNAMELEN) < 0)
    quit("%s\nCould not get hostname", strerror(errno))
  pid = getpid();
  uid = getuid();
  if (setpgrp() < 0) quit("%s\nCould not set pgid", strerror(errno))
  pgid = getpgrp();

  init_RPC(); /* initialize RPC and LWP packages */
  knock(hostname, uid, pgid); /* inform Venus that this advice sidekick exists */

  /* Set up request filter */
  reqfilter.FromWhom = ANY;
  reqfilter.OldOrNew = OLDORNEW;
  reqfilter.ConnOrSubsys.SubsysId = AS_SUBSYSID;

  lprintf("Waiting for RPC2_NewConnection.\n")
  rc = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, NULL, NULL, (long)0, NULL) ;
  if (rc != RPC2_SUCCESS) quit(RPC2_ErrorMsg(rc))
  rc = AdvSkk_ExecuteRequest(cid, reqbuffer, se);
  if (rc != RPC2_SUCCESS) quit(RPC2_ErrorMsg(rc))
  else lprintf("Connection Enabled.\n")

  /* Set advice interests */
  if (interests(uid) < 0)
    quit("Could not setup interests.")

  /* Loop servicing RPC2 calls */
  while (1) {
    lprintf("Listening for advice requests.\n")
    rc = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, NULL, NULL, (long)0, NULL);
    if (rc != RPC2_SUCCESS) quit(RPC2_ErrorMsg(rc))
    else {
      reqcnt++;
      lprintf("Received request #%d.\n", reqcnt)
    }
    ObtainWriteLock(&plock);
    if (phead == NULL) { /* no worker LWP's ready, make a new one */
      ReleaseWriteLock(&plock);
      ptmp = (struct pnode *)malloc(sizeof(pnode));
      if (ptmp == NULL) quit("Malloc failed")
      ptmp->pbuf = reqbuffer;
      ptmp->next = NULL;
      ptmp->req = reqcnt;
      sprintf(ptmp->name, "worker%d", ++workers);
      if ((rc = LWP_CreateProcess(worker, DSTACK, LWP_NORMAL_PRIORITY, (char *)ptmp, ptmp->name, &(ptmp->cpid))) != LWP_SUCCESS)
	quit("Could not create worker LWP (%d)", rc)
    }
    else { /* get first worker from queue and give it the request */
      ptmp = phead;
      phead = phead->next;
      ptmp->next = NULL;
      ReleaseWriteLock(&plock);
      ptmp->pbuf = reqbuffer;
      ptmp->req = reqcnt;
      if ((rc = LWP_SignalProcess(ptmp)) != LWP_SUCCESS)
	quit("Could not signal LWP worker (%d)", rc)
    }
  }

  return(-1);
}

RPC2_Handle contact_venus(const char *hostname)
{
  RPC2_Handle cid;
  RPC2_HostIdent hid;
  RPC2_PortIdent portid;
  RPC2_SubsysIdent subsysid;
  RPC2_BindParms bp;
  long rc;

  hid.Tag = RPC2_HOSTBYNAME;
  if (strlen(hostname) >= 64) /* Not MAXHOSTNAMELEN because rpc2.h uses "64"! */
    quit("Machine name %s too long!", hostname)

  strcpy(hid.Value.Name, hostname);
  portid.Tag = RPC2_PORTBYINETNUMBER;
  portid.Value.InetPortNumber = htons(PORT_venus);
  subsysid.Tag = RPC2_SUBSYSBYID;
  subsysid.Value.SubsysId = VA_SUBSYSID; 

  bp.SecurityLevel = RPC2_OPENKIMONO;
  bp.EncryptionType = (int)NULL;
  bp.SideEffectType = (int)NULL;
  bp.ClientIdent = NULL;
  bp.SharedSecret = NULL;
  rc = RPC2_NewBinding(&hid, &portid, &subsysid, &bp, &cid);
  if (rc != RPC2_SUCCESS) quit("%s\nCannot connect to machine %s (rc = %d)", RPC2_ErrorMsg((int)rc), hostname, rc)
  return(cid);
}

int get_homedir(int uid, char *homedir)
{
  /* assumes homedir is valid and can hold up for MAX_PATHLEN characters */
  struct passwd *passwd_ent;

  passwd_ent = getpwuid(uid);
  if (passwd_ent == NULL) {
    lprintf("%s\nCould not get passwd entry for uid %d.\n", strerror(errno), uid)
    return(-1);
  }
  if ((strlen(passwd_ent->pw_name) + strlen(HOMEDIR_PREFIX) + 1) >= MAX_PATHLEN)
  {
    lprintf("%s\nReturned name %s for uid %d is too long.\n", passwd_ent->pw_name, uid)
    return(-1);
  }
  sprintf(homedir, "%s%s", HOMEDIR_PREFIX, passwd_ent->pw_name);
  return(0);
}

void init_RPC(void)
{
  PROCESS pid;
  RPC2_PortIdent portid;
  RPC2_SubsysIdent subsysid;
  int rc;

  /* Initialize LWP package */
  if (LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &pid) != LWP_SUCCESS)
    quit("Cannot Initialize LWP")

  /* Initialize RPC2 package */
  portid.Value.InetPortNumber = 0;
  rc = RPC2_Init(RPC2_VERSION, NULL, &portid, -1, NULL);
  /*  portid.Tag = RPC2_PORTBYINETNUMBER;
   *  portid.Value.InetPortNumber = htons(AS_PORTAL); */
  rc = RPC2_Init(RPC2_VERSION, NULL, &portid, 1, NULL);
  if (rc != RPC2_SUCCESS)
    quit("%s\nCannot Initialize RPC2", RPC2_ErrorMsg(rc))

  subsysid.Tag = RPC2_SUBSYSBYID;
  subsysid.Value.SubsysId = AS_SUBSYSID;
  rc = RPC2_Export(&subsysid);
  if (rc != RPC2_SUCCESS)
    quit("Cannot export the AdvSkk subsystem")
}

int interests(int uid)
{
  FILE *intfile;
  char line[256], tmp[256];
  int tmpV, tmpA, ln, i = 0;
  long rc = 0;
  InterestValuePair interests[MAXEVENTS];

  intfile = fopen(INTEREST_FILE, "r");
  if (intfile == NULL) {
    lprintf("Cannot open %s for reading.\n", INTEREST_FILE)
    return(-1);
  }

  for (ln = 0; (fgets(line, 256, intfile) != NULL); ln++) {
    if (sscanf(line, "%s", tmp) < 1) {
      lprintf("Error in %s: unexpected end of input on line %d.\n", INTEREST_FILE, ln+1)
      if (fclose(intfile) < 0)
	lprintf("%s\nError closing %s.\n", INTEREST_FILE)
      return(-1);
    }
    else if ((tmp[0] == '#') || (tmp[0] == '\n')) continue;
    if (sscanf(line, "%s%d%d", tmp, &tmpV, &tmpA) < 3) {
      lprintf("Error in %s: unexpected end of input on line %d.\n", INTEREST_FILE, ln+1)
      if (fclose(intfile) < 0)
	lprintf("%s\nError closing %s.\n", INTEREST_FILE)
      return(-1);
    }
    if (strcmp(tmp, InterestNames[i]) != 0) {
      lprintf("Error in %s: expected %s, got %s.\n", INTEREST_FILE, InterestNames[i], tmp)
      if (fclose(intfile) < 0)
	lprintf("%s\nError closing %s.\n", INTEREST_FILE)
      return(-1);
    }
    interests[i].interest = (InterestID)i;
    interests[i].value = tmpV;
    interests[i].argument = tmpA;
    i++;
  }
  interests[i].interest = (InterestID)i;
  interests[i].value = tmpV;
  interests[i].argument = tmpA;
  if (fclose(intfile) < 0) {
    lprintf("%s\nError closing %s.\n", INTEREST_FILE)
    return(-1);
  }

  /* Do we really need this lock? */
  rc = C_RegisterInterest(VenusCID, (RPC2_Integer)uid, i, interests);
  if (rc != RPC2_SUCCESS) {
    lprintf("Could not register interests with Venus.\n")
    return(-1);
  }
  return(0);
}

void knock(const char *hostname, int uid, int pgid) {
  long rc;

  VenusCID = contact_venus(hostname);
  lprintf("\t**** New advice sidekick! ****\nConnected to Venus:\t%s\tport %d\n\tuid %d\tpgid %d\n", 
	  hostname, rpc2_LocalPort.Value.InetPortNumber, uid, pgid)

  rc = C_NewAdviceService(VenusCID, (RPC2_String)hostname, (RPC2_Integer)uid, 
     (RPC2_Integer)rpc2_LocalPort.Value.InetPortNumber, (RPC2_Integer)pgid, 
     (RPC2_Integer *)&vmajor, (RPC2_Integer *)&vminor);

  if (rc != RPC2_SUCCESS) {
    if (rc == EBUSY) quit("Error:  another advice sidekick is already running!")
    else if (rc == EAGAIN) quit("Error:  another advice sidekick is still transitioning!")
    else quit("Error:  cannot establish connection to Venus.")
  }
}

int parse_cmd_line(int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-log") == 0) {
      if (++i >= argc) return(-1);
      logfile = fopen(argv[i], "a");
      if (logfile == NULL)
	quit("%s\nCannot open %s for writing", strerror(errno), argv[i])
    }
    else if (strcmp(argv[i], "-err") == 0)
      err = 1;
    else return(-1);
  }

  return(0);
}

int parse_resolvefile(const char *homedir, const char *pathname, char *asrpath) {
  /* assumes asrpath is valid and can hold up to MAX_PATHLEN characters */
  FILE *rfile;
  char line[256], tmp[256], rfilename[MAX_PATHLEN], tpath[MAX_PATHLEN], tasr[MAX_PATHLEN];
  int ln;

  if (strlen(pathname) > MAX_PATHLEN) {
    lprintf("Pathname in conflict too long.\n")
    return(-1);
  }
  else if ((strlen(homedir) + 12) > MAX_PATHLEN) {
    lprintf("Home directory pathname too long.\n")
    return(-1);
  }
  else sprintf(rfilename, "%s/Resolvefile", homedir);

  rfile = fopen(rfilename, "r");
  if (rfile == NULL) {
    lprintf("Cannot open %s for reading.\n", rfilename)
    return(-1);
  }

  for (ln = 0; (fgets(line, 256, rfile) != NULL); ln++) {
    if ((sscanf(line, "%s", tmp) < 1) || (tmp[0] == '#')) continue;
    if (sscanf(line, "%s%s", tpath, tasr) < 2) {
      lprintf("Error in %s: unexpected end of input on line %d.\n", rfilename, ln+1)
      goto ResolveExit;
    }
    if (tpath[(strlen(tpath) - 1)] != ':') {
      lprintf("Error in %s: parse error at '%c' on line %d.\n", rfilename, (strlen(tpath) - 1), ln+1)
      goto ResolveExit;
    }
    tpath[(strlen(tpath) - 1)] = '\0';
    if ((strlen(tpath) > MAX_PATHLEN) || (strlen(tasr) > MAX_PATHLEN)) {
      lprintf("Error in %s: pathname too long on line %d.\n", rfilename, ln+1)
      goto ResolveExit;
    }
    if (strncmp(tpath, pathname, strlen(tpath)) == 0) {
      if (fclose(rfile) < 0) { 
	lprintf("%s\nError closing %s.\n", rfilename)
	return(-1);
      }
      strcpy(asrpath, tasr);
      return(0);
    }
  }

  lprintf("No ASR specified for path %s.\n", rfilename)

 ResolveExit:
  if (fclose(rfile) < 0) lprintf("%s\nError closing %s.\n", rfilename)
  return(-1);
}

int worker(void *arg) {
  int rc, status, code;
  struct pnode *pinfo, *ptmp;

  pinfo = (struct pnode *)arg;
  pinfo->kid = 0;
  if ((rc = LWP_NewRock(1, (char *)pinfo)) != LWP_SUCCESS)
    quit("Could not hide LWP node under rock (%d)", rc)

  while (1) {
    /* Let the scheduling thread wait for more requests */
    if ((rc = LWP_DispatchProcess()) != LWP_SUCCESS)
      quit("Error dispatching to scheduler (%d)")
    /* Execute the request */
    rc = AdvSkk_ExecuteRequest(cid, pinfo->pbuf, se);
    if (rc != RPC2_SUCCESS) quit(RPC2_ErrorMsg(rc))
    else {
      if (pinfo->kid) {
	if ((rc = waitpid(pinfo->kid, &status, 0)) < 0)
	  quit("Error waiting for ASR to finish: %s", strerror(errno))
	if (WIFEXITED(status)) code = WEXITSTATUS(status);
	else code = -1;
	if ((rc = C_ResultOfASR(VenusCID, pinfo->tmp, pinfo->kid, code)) != RPC2_SUCCESS)
	  quit(RPC2_ErrorMsg(rc))
	pinfo->kid = 0;
      }
      lprintf("Request #%d executed.\n", pinfo->req)
    }

    /* Put self in queue and wait */
    ObtainWriteLock(&plock);
    if (phead == NULL) 
      phead = pinfo;
    else {
      ptmp = phead;
      while (ptmp->next != NULL)
	ptmp = ptmp->next;
      ptmp->next = pinfo;
    }
    ReleaseWriteLock(&plock);
    LWP_WaitProcess(pinfo);
  }

  return(-1);
}

/* AdvSkk RPC2 calls */

long S_Spike(RPC2_Handle _cid, RPC2_Integer Cmd) {

  if (Cmd) {
    lprintf("Ping.\n")
    return(RPC2_SUCCESS);
  }
  quit("Received terminate command from Venus")
}

long S_TokensAcquired(RPC2_Handle _cid, RPC2_Integer EndTimestamp) {

  return(RPC2_SUCCESS);
}

long S_TokensExpired(RPC2_Handle _cid) {

  return(RPC2_SUCCESS);
}

long S_ActivityPendingTokens(RPC2_Handle _cid, ActivityID activityType,
			   RPC2_String argument) {

  return(RPC2_SUCCESS);
}

long S_SpaceInformation(RPC2_Handle _cid, RPC2_Integer PercentFilesFilledByHoardedData,
		      RPC2_Integer PercentBlocksFilledByHoardedData,
		      RPC2_Integer PercentRVMFull,
		      Boolean RVMFragmented) {

  return(RPC2_SUCCESS);
}

long S_ServerAccessible(RPC2_Handle _cid, RPC2_String ServerName) {

  return(RPC2_SUCCESS);
}

long S_ServerInaccessible(RPC2_Handle _cid, RPC2_String ServerName) {

  return(RPC2_SUCCESS);
}

long S_ServerConnectionStrong(RPC2_Handle _cid, RPC2_String ServerName) {

  return(RPC2_SUCCESS);
}

long S_ServerConnectionWeak(RPC2_Handle _cid, RPC2_String ServerName) {

  return(RPC2_SUCCESS);
}

long S_NetworkQualityEstimate(RPC2_Handle _cid, long numEstimates, QualityEstimate serverList[]) {

  return(RPC2_SUCCESS);
}

long S_VolumeTransitionEvent(RPC2_Handle _cid, RPC2_String VolumeName,
			   RPC2_Integer vol_id,
			   VolumeStateID NewState,
			   VolumeStateID OldState) {

  return(RPC2_SUCCESS);
}

long S_Reconnection(RPC2_Handle _cid, ReconnectionQuestionnaire *Questionnaire, RPC2_Integer *ReturnCode) {

  return(RPC2_SUCCESS);
}

long S_DataFetchEvent(RPC2_Handle _cid, RPC2_String Pathname, RPC2_Integer Size, RPC2_String Vfile) {

  return(RPC2_SUCCESS);
}

long S_ReadDisconnectedCacheMissEvent(RPC2_Handle _cid, ObjectInformation *objInfo,
				    ProcessInformation *processInfo,
				    RPC2_Unsigned TimeOfMiss,
				    CacheMissAdvice *Advice,
				    RPC2_Integer *ReturnCode) {

  return(RPC2_SUCCESS);
}

long S_WeaklyConnectedCacheMissEvent(RPC2_Handle _cid, ObjectInformation *objInfo,
				   ProcessInformation *processInfo,
				   RPC2_Unsigned TimeOfMiss,
				   RPC2_Integer Length,
				   RPC2_Integer EstimatedBandwidth,
				   RPC2_String Vfile,
				   CacheMissAdvice *Advice,
				   RPC2_Integer *ReturnCode) {

  return(RPC2_SUCCESS);
}

long S_DisconnectedCacheMissEvent(RPC2_Handle _cid, ObjectInformation *objInfo,
				 ProcessInformation *processInfo,
				 RPC2_Unsigned TimeOfMiss,
				 RPC2_Unsigned TimeOfDisconnection,
				 RPC2_Integer *ReturnCode) {

  return(RPC2_SUCCESS);
}

long S_HoardWalkAdviceRequest(RPC2_Handle _cid, RPC2_String InputPathname, RPC2_String OutputPathname, RPC2_Integer *ReturnCode) {

  return(RPC2_SUCCESS);
}

long S_HoardWalkBegin(RPC2_Handle _cid) {

  return(RPC2_SUCCESS);
}

long S_HoardWalkStatus(RPC2_Handle _cid, RPC2_Integer Percentage) {

  return(RPC2_SUCCESS);
}

long S_HoardWalkEnd(RPC2_Handle _cid) {

  return(RPC2_SUCCESS);
}

long S_HoardWalkPeriodicOn(RPC2_Handle _cid) {

  return(RPC2_SUCCESS);
}

long S_HoardWalkPeriodicOff(RPC2_Handle _cid) {

  return(RPC2_SUCCESS);
}

long S_ObjectInConflict(RPC2_Handle _cid, RPC2_String Pathname, ViceFid *fid) {

  return(RPC2_SUCCESS);
}

long S_ObjectConsistent(RPC2_Handle _cid, RPC2_String Pathname, ViceFid *fid){

  return(RPC2_SUCCESS);
}

long S_ReintegrationPendingTokens(RPC2_Handle _cid, RPC2_String volumeID) {

  return(RPC2_SUCCESS);
}

long S_ReintegrationEnabled(RPC2_Handle _cid, RPC2_String volumeID) {

  return(RPC2_SUCCESS);
}

long S_ReintegrationActive(RPC2_Handle _cid, RPC2_String volumeID) {

  return(RPC2_SUCCESS);
}

long S_ReintegrationCompleted(RPC2_Handle _cid, RPC2_String volumeID) {

  return(RPC2_SUCCESS);
}

long S_TaskAvailability(RPC2_Handle _cid, RPC2_Integer numTasks, TallyInfo taskList[]) {

  return(RPC2_SUCCESS);
}

long S_TaskUnavailable(RPC2_Handle _cid, RPC2_Integer TaskPriority, RPC2_Integer ElementSize) {

  return(RPC2_SUCCESS);
}

long S_ProgramAccessLogAvailable(RPC2_Handle _cid, RPC2_String LogFile) {

  return(RPC2_SUCCESS);
}

long S_ReplacementLogAvailable(RPC2_Handle _cid, RPC2_String LogFile) {

  return(RPC2_SUCCESS);
}

long S_InvokeASR(RPC2_Handle _cid, RPC2_String pathname, RPC2_Integer vol_id, RPC2_Integer vuid, RPC2_Integer *ASRid, RPC2_Integer *ASRrc) {
  int ret, rc, tm;
  struct pnode *pinfo;
  char hd[MAX_PATHLEN], svuid[32], asr[MAX_PATHLEN], asrlog[MAX_PATHLEN];
  struct stat sbuf;

  if ((rc = LWP_GetRock(DEF_ROCK, (char **)&pinfo)) != LWP_SUCCESS)
    quit("Could not get LWP node from under rock (%d)", rc)

  ret = fork();

  if (ret == 0) {
    if (fclose(logfile) < 0)
      quit("Error closing logfile: %s", strerror(errno))
    err = 0;
    if (strrchr((char *)pathname, (int)'/') == NULL)
      quit("Invalid pathname for conflict")
    sprintf(asrlog, "/tmp/.asrlogs%s.%d.XXXXXX", strrchr((char *)pathname, (int)'/'), pinfo->req);
    if ((stat("/tmp/.asrlogs", &sbuf) < 0) || ((errno = (S_ISDIR(sbuf.st_mode)) ? 0 : ENOTDIR) != 0)
        || (mktemp(asrlog) == NULL) || ((logfile = fopen(asrlog, "a")) == NULL) 
	|| (dup2(fileno(logfile), 1) < 0) || (dup2(fileno(logfile), 2) < 0))
      quit("Could not create ASR log: %s", strerror(errno))
    if ((ret = setpgrp()) || (ret = setuid(vuid)) || (ret = get_homedir(vuid, hd)))
      exit(ret);
    if (parse_resolvefile(hd, (char *)pathname, asr) < 0)
      quit("Could not determine from Resolvefile which ASR to run")
    lprintf("Hello, I am an ASR (%s).\n\tuid %d\tpathname %s\n", asr, vuid, (char *)pathname)
    sprintf(svuid, "%d", vuid);    
    execl(asr, asr, (char *)pathname, svuid, hd, NULL);
    quit("Exec error: %s", strerror(errno))
  }
  else if (ret < 0) {
    lprintf("Could not fork to create ASR (%s)\n", strerror(errno))
    *ASRrc = SKK_FAIL;
  }
  else {
    *ASRid = ret;
    *ASRrc = SKK_SUCCESS;
  }
  pinfo->kid = ret;
  pinfo->tmp = vol_id;

  return(RPC2_SUCCESS);
}

char *InterestNames[MAXEVENTS] = {
       "TokensAcquired",
       "TokensExpired",
       "ActivityPendingTokens",
       "SpaceInformation",
       "ServerAccessible",
       "ServerInaccessible",
       "ServerConnectionStrong",
       "ServerConnectionWeak",
       "NetworkQualityEstimate",
       "VolumeTransitionEvent",
       "Reconnection",
       "DataFetchEvent",
       "ReadDisconnectedCacheMissEvent",
       "WeaklyConnectedCacheMissEvent",
       "DisconnectedCacheMissEvent",
       "HoardWalkAdviceRequest",
       "HoardWalkBegin",
       "HoardWalkStatus",
       "HoardWalkEnd",
       "HoardWalkPeriodicOn",
       "HoardWalkPeriodicOff",
       "ObjectInConflict",
       "ObjectConsistent",
       "ReintegrationPendingTokens",
       "ReintegrationEnabled",
       "ReintegrationActive",
       "ReintegrationCompleted",
       "TaskAvailability",
       "TaskUnavailable",
       "ProgramAccessLogs",
       "ReplacementLogs",
       "InvokeASR"
};
