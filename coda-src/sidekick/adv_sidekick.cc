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

#include <coda_config.h>

#include "adv_sidekick.h"

struct Lock plock;
int err = 0, reqcnt = 0;
int vmajor, vminor;
RPC2_Handle VenusCID = -1;
FILE *logfile = NULL;
int workers = 0;
RPC2_Handle cid;
SE_Descriptor *se = NULL;
struct pnode *phead = NULL;

int main(int argc, char **argv) {
    RPC2_RequestFilter reqfilter;
    RPC2_PacketBuffer *reqbuffer;
    char hostname[MAXHOSTNAMELEN];
    int rc, uid, pid, pgid;
    struct pnode *ptmp;
    struct stat sbuf;

    /* Initialization */
    Lock_Init(&plock);
    if (parse_cmd_line(argc, argv) < 0)
	quit("usage: %s [-log <filename>] [-err]", argv[0]);
    if ((logfile == NULL) && ((logfile = fopen(DEF_LOGFILE, "a")) == NULL))
	quit("%s\nCannot open %s for writing", strerror(errno), DEF_LOGFILE);
    /* create directory for ASR logs, error if it can't be created
       or it already exists but isn't a directory */
    if ((mkdir(ASRLOGDIR, 0777) < 0) && 
	((errno != EEXIST) || (stat(ASRLOGDIR, &sbuf) < 0) || (!S_ISDIR(sbuf.st_mode))))
	quit("Could not create asr log directory");
    if (chmod(ASRLOGDIR, 0777) < 0)
	quit("%s\nCould set permissions on asr log directory", strerror(errno));
    
    /* Get necessary information */
    if (gethostname(hostname, MAXHOSTNAMELEN) < 0)
	quit("%s\nCould not get hostname", strerror(errno));
    pid = getpid();
    uid = getuid();
    if (setpgid(0, 0) < 0)
	quit("%s\nCould not set pgid", strerror(errno));
    pgid = getpgrp();

    init_RPC(); /* initialize RPC and LWP packages */
    knock(hostname, uid, pgid); /* inform Venus that this advice sidekick exists */

    /* Set up request filter */
    reqfilter.FromWhom = ANY;
    reqfilter.OldOrNew = OLDORNEW;
    reqfilter.ConnOrSubsys.SubsysId = AS_SUBSYSID;

    lprintf("Waiting for RPC2_NewConnection.\n");
    rc = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, NULL, NULL, (long)0, NULL) ;
    if (rc != RPC2_SUCCESS) quit(RPC2_ErrorMsg(rc));
    rc = AdvSkk_ExecuteRequest(cid, reqbuffer, se);
    if (rc != RPC2_SUCCESS) quit(RPC2_ErrorMsg(rc));
    else lprintf("Connection Enabled.\n");

    /* Set advice interests */
    if (interests(uid) < 0)
	quit("Could not setup interests.");

    /* Loop servicing RPC2 calls */
    while (1) {
	lprintf("Listening for advice requests.\n");
	rc = RPC2_GetRequest(&reqfilter, &cid, &reqbuffer, NULL, NULL, (long)0, NULL);
	if (rc != RPC2_SUCCESS) quit(RPC2_ErrorMsg(rc));
	else {
	    reqcnt++;
	    lprintf("Received request #%d.\n", reqcnt);
	}
	ObtainWriteLock(&plock);
	if (phead == NULL) { /* no worker LWP's ready, make a new one */
	    ReleaseWriteLock(&plock);
	    ptmp = (struct pnode *)malloc(sizeof(pnode));
	    if (ptmp == NULL) quit("Malloc failed");
	    ptmp->pbuf = reqbuffer;
	    ptmp->next = NULL;
	    ptmp->req = reqcnt;
	    sprintf(ptmp->name, "worker%d", ++workers);
	    if ((rc = LWP_CreateProcess(worker, DSTACK, LWP_NORMAL_PRIORITY, (char *)ptmp, ptmp->name, &(ptmp->cpid))) != LWP_SUCCESS)
		quit("Could not create worker LWP (%d)", rc);
	}
	else { /* get first worker from queue and give it the request */
	    ptmp = phead;
	    phead = phead->next;
	    ptmp->next = NULL;
	    ReleaseWriteLock(&plock);
	    ptmp->pbuf = reqbuffer;
	    ptmp->req = reqcnt;
	    if ((rc = LWP_SignalProcess(ptmp)) != LWP_SUCCESS)
		quit("Could not signal LWP worker (%d)", rc);
	}
    }

    return(-1);
}

RPC2_Handle contact_venus(const char *hostname) {
    RPC2_Handle cid;
    RPC2_HostIdent hid;
    RPC2_PortIdent portid;
    RPC2_SubsysIdent subsysid;
    RPC2_BindParms bp;
    long rc;

    hid.Tag = RPC2_HOSTBYNAME;
    if (strlen(hostname) >= 64) /* Not MAXHOSTNAMELEN because rpc2.h uses "64"! */
	quit("Machine name %s too long!", hostname);

    strcpy(hid.Value.Name, hostname);
    portid.Tag = RPC2_PORTBYINETNUMBER;
    portid.Value.InetPortNumber = htons(PORT_venus);
    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = VA_SUBSYSID; 

    bp.SecurityLevel = RPC2_OPENKIMONO;
    bp.EncryptionType = 0;
    bp.SideEffectType = 0;
    bp.ClientIdent = NULL;
    bp.SharedSecret = NULL;
    rc = RPC2_NewBinding(&hid, &portid, &subsysid, &bp, &cid);
    if (rc != RPC2_SUCCESS) quit("%s\nCannot connect to machine %s (rc = %ld)", RPC2_ErrorMsg((int)rc), hostname, rc);
    return(cid);
}

int executor(char *pathname, int vuid, int req_no)
{
    char space[DEF_BUF];
    char asr[MAXPATHLEN], asrlog[MAXPATHLEN], conf[MAXPATHLEN];
    char fixfile[MAXPATHLEN], fixed[MAXPATHLEN], parent[MAXPATHLEN], hd[MAXPATHLEN];
    ViceFid fixfid;
    char fixrealm[MAXHOSTNAMELEN];
    vv_t fixvv;
    struct stat sbuf;
    struct repinfo inf;
    char svuid[32];
    char *zargs[ASRARGS];  /* asr, fixed, lgrep, ssrep1, ssrep2, ssrep3, NULL */
    int ret, pid, status, i;
    struct repvol *repv;
    struct volrep *volr;

    if ((ret = get_homedir(vuid, hd)) < 0)
	quit("Could not get home directory");

    /* create the ASR logfile */
    if (fclose(logfile) < 0)
	quit("Error closing logfile: %s", strerror(errno));
    err = 0;
    if (strrchr((char *)pathname, (int)'/') == NULL)
	quit("Invalid pathname for conflict");
    sprintf(asrlog, "%s%s.%d.XXXXXX", ASRLOGDIR, strrchr((char *)pathname, (int)'/'), req_no);
    if ((stat(ASRLOGDIR, &sbuf) < 0)
	|| ((errno = (S_ISDIR(sbuf.st_mode)) ? 0 : ENOTDIR) != 0)
	|| (mktemp(asrlog) == NULL)
	|| ((logfile = fopen(asrlog, "a")) == NULL)
	|| (dup2(fileno(logfile), 1) < 0)
	|| (dup2(fileno(logfile), 2) < 0))
	quit("Could not create ASR log: %s", strerror(errno));

    /* parse out the parent directory and conflict name */
    if (parse_path(pathname, parent, conf) < 0)
	quit("Malformed pathname in conflict");

    /* change to the parent directory of the conflict */
    if (chdir(parent) < 0)
	quit("%s\nCould not chdir to parent of conflict", strerror(errno));

    /* begin the repair session */
    if ((ret = BeginRepair(pathname, &repv, space, sizeof(space))) < 0)
	quit("%s\nCould not begin repair session", space);

    /* get replica name arguments */
    for (volr = repv->rwhead, i = 2; volr != NULL; volr = volr->next, i++) {
	if ((zargs[i] = (char *)malloc((strlen(repv->rodir) + strlen(volr->compname) + 2) * sizeof(char))) == NULL)
	    quit("Malloc failed");
	sprintf(zargs[i], "%s/%s", repv->rodir, volr->compname);
    }
    if (i >= ASRARGS) quit("It shouldn't be possible to have %d arguments", i);
    zargs[i] = NULL;

    /* create "fixed" file (which ASR will write to) */
    sprintf(fixed, "%s/fixed.%d.XXXXXX", FIXEDDIR, req_no);
    if ((mktemp(fixed) == NULL) 
	|| ((ret = (repv->dirconf ? mkdir(fixed, 0700) : open(fixed, O_RDWR|O_CREAT|O_EXCL, 0700))) < 0)
	|| (!(repv->dirconf) && (close(ret) < 0)))
	quit("Could not created fixed file: %s", strerror(errno));
    if ((zargs[1] = strdup(fixed)) == NULL)
	quit("Malloc failed");

    /* determine pathname and arguments to ASR */
    if (parse_resolvefile(hd, (char *)repv->rodir, asr) < 0)
	quit("Could not determine from Resolvefile which ASR to run");
    if ((zargs[0] = strdup(asr)) == NULL)
	quit("Malloc failed");

    /* fork off the ASR */
    ret = fork();
    if (ret == 0) {
	sprintf(svuid, "%d", vuid);    
	lprintf("ASR invoked (%s).\n\tuid %d\tpathname %s\n", asr, vuid, (char *)repv->rodir);
	execv(asr, zargs);
	quit("Exec error: %s", strerror(errno));
    }
    else {
	for (i = 0; zargs[i] != NULL; i++)
	    freeif(zargs[i]);
	if (ret < 0)
	    quit("Could not fork to create ASR (%s)", strerror(errno));
	else {
	    pid = ret;
	    if ((ret = waitpid(pid, &status, 0)) < 0)
		quit("Error waiting for ASR to finish: %s", strerror(errno));
	    if (WIFEXITED(status)) ret = WEXITSTATUS(status);
	    else ret = -1;
	}
    }
    if (ret == 0) { /* if ASR exited normally */

	/* repair the conflict using "fixed" file */
	lprintf("ASR finished, now repairing conflict...\n");

	if (repv->dirconf) { /* directory conflict */

	    if (repv->local) { /* local/global conflict */

		lprintf("Sorry, can't handle local/global directory conflicts, yet.\n");

	    }
	    else { /* server/server conflict */
		memset(&inf, 0, sizeof(inf));
		inf.fixed = fixed;

		sprintf(fixfile, "%s/fix.%d.XXXXXX", FIXEDDIR, req_no);
		if (mktemp(fixfile) == NULL)
		    quit("Could not created fixfile: %s", strerror(errno));

		/* do the compare */
		while ((ret = CompareDirs(repv, fixfile, &inf, space, sizeof(space))) == -2) {
		    if (DoRepair(repv, fixfile, logfile, space, sizeof(space)) < 0) {
			ret = -1;
			break;
		    }
		}
		lprintf("CompareDirs finished, ret = %d\n", ret);
		/* then the repair */
		if ((ret < 0) || (DoRepair(repv, fixfile, logfile, space, sizeof(space)) < 0)) {
		    lprintf("%s\nError repairing conflicts\n", space);
		    if (EndRepair(repv, 0, space, sizeof(space)) < 0)
			lprintf("Error ending repair: %s\n", space);
		    quit("Repair failed");
		}
		lprintf("DoRepair finished\n");
	    }

	    if (EndRepair(repv, 0, space, sizeof(space)) < 0)
		lprintf("Error ending repair: %s\n", space);
	    lprintf("EndRepair finished\n");

	}
	else { /* file conflict */ 
	    if (repv->local) { /* local/global conflict */
		/* "repair" it by discarding all local mutations, 
		   then move the fixed file into place */
		if (DiscardAllLocal(repv, space, sizeof(space)) < 0) {
		    lprintf("Error discarding local mutations: %s\n", space);
		    if (EndRepair(repv, 1, space, sizeof(space)) < 0)
			lprintf("Error ending repair: %s\n", space);
		    quit("Could not repair local-global conflict");
		}

		if (EndRepair(repv, 1, space, sizeof(space)) < 0)
		  quit("Error ending repair: %s", space);

		/* move the fixed file in */
		if (copyfile_byname(fixed, repv->rodir) < 0)
		  quit("Error moving fixed file: %s", strerror(errno));
		if (unlink(fixed) < 0)
		  quit("Could not remove fixed file: %s", strerror(errno));
	    }
	    else { /* server/server conflict */

		if (stat(fixed, &sbuf) != 0)
		    quit("Couldn't find fixed file %s: %s", fixed, strerror(errno));

		if (!(sbuf.st_mode & S_IFREG))
		    quit("File %s is not a regular file (and hence cannot be used for repair)", fixed);

		if (!repair_getfid(fixed, &fixfid, fixrealm, &fixvv, space, sizeof(space)) && (fixvv.StoreId.Host != (unsigned long)-1))
		    sprintf(fixfile, "@%lx.%lx.%lx@%s", fixfid.Volume, fixfid.Vnode, fixfid.Unique, fixrealm);
		else strcpy(fixfile, fixed);

		if (EndRepair(repv, 1, space, sizeof(space)) < 0)
		    quit("Error ending repair: %s", space);

		if ((dorep(repv, fixfile, NULL, 0) < 0) && (errno != ETOOMANYREFS))
		    quit("Error repairing conflict: %s", strerror(errno));
	    }
	}
	return(0);
    }
    else {
	if (EndRepair(repv, 0, space, sizeof(space)) < 0)
	    lprintf("Error ending repair: %s\n", space);
	quit("ASR failed (exit code %d)", ret);
    }
}

int get_homedir(int uid, char *homedir) {
    /* assumes homedir is valid and can hold up for MAXPATHLEN characters */
    struct passwd *passwd_ent;

    passwd_ent = getpwuid(uid);
    if (passwd_ent == NULL) {
	lprintf("%s\nCould not get passwd entry for uid %d.\n", strerror(errno), uid);
	return(-1);
    }
    if ((strlen(passwd_ent->pw_name) + strlen(HOMEDIR_PREFIX) + 1) >= MAXPATHLEN)
	{
	    lprintf("%s\nReturned name %s for uid %d is too long.\n", strerror(errno), passwd_ent->pw_name, uid);
	    return(-1);
	}
    sprintf(homedir, "%s%s", HOMEDIR_PREFIX, passwd_ent->pw_name);
    return(0);
}

void init_RPC(void) {
    PROCESS lwpid;
    RPC2_PortIdent portid;
    RPC2_SubsysIdent subsysid;
    int rc;

    /* Initialize LWP package */
    if ((rc = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &lwpid)) != LWP_SUCCESS)
	quit("Could not initialize LWP (%d)", rc);

    /* Initialize RPC2 package */
    portid.Value.InetPortNumber = 0;
    rc = RPC2_Init(RPC2_VERSION, NULL, &portid, -1, NULL);
    /*  portid.Tag = RPC2_PORTBYINETNUMBER;
     *  portid.Value.InetPortNumber = htons(AS_PORTAL); */
    rc = RPC2_Init(RPC2_VERSION, NULL, &portid, 1, NULL);
    if (rc != RPC2_SUCCESS)
	quit("%s\nCould not Initialize RPC2", RPC2_ErrorMsg(rc));

    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = AS_SUBSYSID;
    rc = RPC2_Export(&subsysid);
    if (rc != RPC2_SUCCESS)
	quit("%s\nCould not export the AdvSkk subsystem", RPC2_ErrorMsg((int)rc));
}

int interests(int uid) {
    FILE *intfile;
    char line[256], tmp[256];
    int tmpV, tmpA, ln, i = 0;
    long rc = 0;
    InterestValuePair interests[MAXEVENTS];

    intfile = fopen(INTEREST_FILE, "r");
    if (intfile == NULL) {
	lprintf("Could not open %s for reading.\n", INTEREST_FILE);
	return(-1);
    }

    for (ln = 0; (fgets(line, 256, intfile) != NULL); ln++) {
	if (sscanf(line, "%s", tmp) < 1) {
	    lprintf("Error in %s: unexpected end of input on line %d.\n", INTEREST_FILE, ln+1);
	    if (fclose(intfile) < 0)
		lprintf("%s\nError closing %s.\n", strerror(errno), INTEREST_FILE);
	    return(-1);
	}
	else if ((tmp[0] == '#') || (tmp[0] == '\n')) continue;
	if (sscanf(line, "%s%d%d", tmp, &tmpV, &tmpA) < 3) {
	    lprintf("Error in %s: unexpected end of input on line %d.\n", INTEREST_FILE, ln+1);
	    if (fclose(intfile) < 0)
		lprintf("%s\nError closing %s.\n", strerror(errno), INTEREST_FILE);
	    return(-1);
	}
	if (strcmp(tmp, InterestNames[i]) != 0) {
	    lprintf("Error in %s: expected %s, got %s.\n", INTEREST_FILE, InterestNames[i], tmp);
	    if (fclose(intfile) < 0)
		lprintf("%s\nError closing %s.\n", strerror(errno), INTEREST_FILE);
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
	lprintf("%s\nError closing %s.\n", strerror(errno), INTEREST_FILE);
	return(-1);
    }

    rc = C_RegisterInterest(VenusCID, (RPC2_Integer)uid, i, interests);
    if (rc != RPC2_SUCCESS) {
	lprintf("%s\nCould not register interests with Venus.\n", RPC2_ErrorMsg((int)rc));
	return(-1);
    }
    return(0);
}

void knock(const char *hostname, int uid, int pgid) {
    long rc;

    VenusCID = contact_venus(hostname);
    lprintf("\t**** New advice sidekick! ****\nConnected to Venus:\t%s\tport %d\n\tuid %d\tpgid %d\n", 
	    hostname, rpc2_LocalPort.Value.InetPortNumber, uid, pgid);

    rc = C_NewAdviceService(VenusCID, (RPC2_String)hostname, (RPC2_Integer)uid, 
			    (RPC2_Integer)rpc2_LocalPort.Value.InetPortNumber, (RPC2_Integer)pgid, 
			    (RPC2_Integer *)&vmajor, (RPC2_Integer *)&vminor);

    if (rc != RPC2_SUCCESS) {
	if (rc == EBUSY) quit("Error:  another advice sidekick is already running!");
	else if (rc == EAGAIN) quit("Error:  another advice sidekick is still transitioning!");
	else quit("%s\nCould not establish connection to Venus.", RPC2_ErrorMsg((int)rc));
    }
}

int parse_cmd_line(int argc, char **argv) {
    int i;

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-log") == 0) {
	    if (++i >= argc) return(-1);
	    logfile = fopen(argv[i], "a");
	    if (logfile == NULL)
		quit("%s\nCould not open %s for writing", strerror(errno), argv[i]);
	}
	else if (strcmp(argv[i], "-err") == 0)
	    err = 1;
	else return(-1);
    }

    return(0);
}

int parse_path(const char *pathname, char *parent, char *conf) {
    char tmp[MAXPATHLEN];
    char *mark;

    strcpy(tmp, pathname); /* make a mutable copy */
    /* chop off trailing /'s */
    while (tmp[(strlen(tmp) - 1)] == '/')
	tmp[(strlen(tmp) - 1)] = '\0';
    if ((mark = strrchr(tmp, '/')) == NULL) return(-1);
    *mark = '\0'; /* clobber the slash */
    strcpy(parent, tmp);
    strcpy(conf, mark + sizeof(char));
    return(0);
}

int parse_resolvefile(const char *homedir, const char *pathname, char *asrpath) {
    /* assumes asrpath is valid and can hold up to MAXPATHLEN characters */
    FILE *rfile;
    char line[256], tmp[256], rfilename[MAXPATHLEN], tpath[MAXPATHLEN], tasr[MAXPATHLEN];
    int ln;

    if (strlen(pathname) > MAXPATHLEN) {
	lprintf("Pathname in conflict too long.\n");
	return(-1);
    }
    else if ((strlen(homedir) + 12) > MAXPATHLEN) {
	lprintf("Home directory pathname too long.\n");
	return(-1);
    }
    else sprintf(rfilename, "%s/Resolvefile", homedir);

    rfile = fopen(rfilename, "r");
    if (rfile == NULL) {
	lprintf("Could not open %s for reading.\n", rfilename);
	return(-1);
    }

    for (ln = 0; (fgets(line, 256, rfile) != NULL); ln++) {
	if ((sscanf(line, "%s", tmp) < 1) || (tmp[0] == '#')) continue;
	if (sscanf(line, "%s%s", tpath, tasr) < 2) {
	    lprintf("Error in %s: unexpected end of input on line %d.\n", rfilename, ln+1);
	    goto ResolveExit;
	}
	if (tpath[(strlen(tpath) - 1)] != ':') {
	    lprintf("Error in %s: parse error at '%c' on line %d.\n", rfilename, (strlen(tpath) - 1), ln+1);
	    goto ResolveExit;
	}
	tpath[(strlen(tpath) - 1)] = '\0';
	if ((strlen(tpath) > MAXPATHLEN) || (strlen(tasr) > MAXPATHLEN)) {
	    lprintf("Error in %s: pathname too long on line %d.\n", rfilename, ln+1);
	    goto ResolveExit;
	}
	if (strncmp(tpath, pathname, strlen(tpath)) == 0) {
	    if (fclose(rfile) < 0) { 
		lprintf("%s\nError closing %s.\n", strerror(errno), rfilename);
		return(-1);
	    }
	    strcpy(asrpath, tasr);
	    return(0);
	}
    }

    lprintf("No ASR specified for path %s.\n", rfilename);

 ResolveExit:
    if (fclose(rfile) < 0) lprintf("%s\nError closing %s.\n", strerror(errno), rfilename);
    return(-1);
}

/* Checks if linkpath is a symlink which points to target
 * Returns whether link points to target
 * Returns -1 on error (e.g. linkpath isn't a symlink) */
int point(char *linkpath, char *target) {
    char lnpath[MAXPATHLEN];

    memset(lnpath, 0, MAXPATHLEN);
    if (readlink(linkpath, lnpath, MAXPATHLEN - 1) < 0)
	return(-1);
    else return(strcmp(lnpath, linkpath) == 0);
}

int worker(void *arg) {
    int rc, status, code;
    struct pnode *pinfo, *ptmp;

    pinfo = (struct pnode *)arg;
    pinfo->kid = 0;
    if ((rc = LWP_NewRock(1, (char *)pinfo)) != LWP_SUCCESS)
	quit("Could not hide LWP node under rock (%d)", rc);

    while (1) {
	/* Let the scheduling thread wait for more requests */
	if ((rc = LWP_DispatchProcess()) != LWP_SUCCESS)
	    quit("Error dispatching to scheduler (%d)", rc);
	/* Execute the request */
	rc = AdvSkk_ExecuteRequest(cid, pinfo->pbuf, se);
	if (rc != RPC2_SUCCESS) quit(RPC2_ErrorMsg(rc));
	else {
	    if (pinfo->kid) {
		if ((rc = waitpid(pinfo->kid, &status, 0)) < 0)
		    quit("Error waiting for ASR to finish: %s", strerror(errno));
		if (WIFEXITED(status)) code = WEXITSTATUS(status);
		else code = -1;
		if ((rc = C_ResultOfASR(VenusCID, pinfo->realm, pinfo->volume, pinfo->kid, code)) != RPC2_SUCCESS)
		    quit(RPC2_ErrorMsg(rc));
		pinfo->kid = 0;
	    }
	    lprintf("Request #%d executed.\n", pinfo->req);
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
	lprintf("Ping.\n");
	return(RPC2_SUCCESS);
    }
    quit("Received terminate command from Venus");
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

long S_InvokeASR(RPC2_Handle _cid, RPC2_String pathname, RPC2_Integer realmid,
		 RPC2_Integer volid, RPC2_Integer vuid, RPC2_Integer *ASRid,
		 RPC2_Integer *ASRrc)
{
    int ret;
    struct pnode *pinfo, *ptmp;
    RPC2_SubsysIdent subsysid;

    if ((ret = LWP_GetRock(DEF_ROCK, (char **)&pinfo)) != LWP_SUCCESS)
	quit("Could not get LWP node from under rock (%d)", ret);

  /* fork off the executor */
    ret = fork();
    if (ret == 0) {
	/* cleanup RPC2 stuff to avoid messing up the parent process */
	subsysid.Tag = RPC2_SUBSYSBYID;
	subsysid.Value.SubsysId = AS_SUBSYSID;
	if (((ret = RPC2_DeExport(&subsysid)) != RPC2_SUCCESS) 
	    || ((ret = RPC2_Unbind(_cid)) != RPC2_SUCCESS))
	    quit(RPC2_ErrorMsg(ret));

    /* set the process group and uid */
	if ((ret = setpgid(0, 0)) < 0)
	    quit("Setpgid failed: %s", strerror(errno));
	if ((ret = setuid(vuid)) < 0)
	    quit("Setuid failed: %s", strerror(errno));

    /* free all the LWP lists (can't LWP_TerminateProcessSupport since
       this isn't the init thread */
	while ((ptmp = phead) != NULL) {
	    phead = phead->next;
	    free(ptmp);
	}
    
	/* run the executor */
	ret = executor((char *)pathname, vuid, pinfo->req);
	exit(ret);
    }
    else if (ret < 0) {
	lprintf("Could not fork to create executor (%s)\n", strerror(errno));
	*ASRrc = SKK_FAIL;
    }
    else {
	pinfo->kid = ret;
	pinfo->realm = realmid;
	pinfo->volume = volid;
	*ASRid = ret;
	*ASRrc = SKK_SUCCESS;
    }

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
