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

#include "repcmds.h"

static int  compareAcl(int, resreplica *);
static int  compareOwner(int, resreplica *);
static int  compareQuotas(int , char **);
static int  compareStatus(int, resreplica *);
static int  compareVV(int, char **, struct repvol *);
static int  doCompare(int, struct repvol *, char **, char *, char *, ViceFid *, char *, char *, char *, char *);
static int  dorep(struct repvol *repv, char *path, char *buf, int len);
static int  findtype(struct repvol *);
static void getremovelists(int, resreplica *, struct listhdr **);
static int  getVolrepNames(struct repvol *repv, char ***names, char *msg, int msgsize);
static int  makedff(char *extfile, char *intfile);
static void printAcl(struct Acl *);

/* Assumes pathname is the path of a conflict
 * Fills in repv with repvol created in beginning repair
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int BeginRepair(char *pathname, struct repvol **repv, char *msg, int msgsize) {
    char msgbuf[DEF_BUF], space[DEF_BUF], cmd[32];
    VolumeId vid;
    struct ViceIoctl vioc;
    int rc;

    /* Create a new rep vol entry */
    if (repair_newrep(pathname, repv, space, sizeof(space)) < 0) {
	strerr(msg, msgsize, "Could not allocate new repvol: %s", space);
	return(-1);
    }

    /* Obtain names of rw replicas */
    vioc.in = NULL;
    vioc.in_size = 0;
    vioc.out = space;
    vioc.out_size = (short)sizeof(space);
    memset(space, 0, sizeof(space));
    rc = pioctl((*repv)->rodir, VIOC_ENABLEREPAIR, &vioc, 0);
    if (rc < 0) {
	if (errno == EWOULDBLOCK)
	    strerr(msg, msgsize, "Repair in progress on volume at \"%s\"", (*repv)->mnt);
	else 
	    strerr(msg, msgsize, "ENABLEREPAIR %s: %s", (*repv)->mnt, strerror(errno));
	repair_finish(*repv);
	return(-1);
    }

    /* Mount the rw replicas, i.e. just insert into list in repv */
    if ((rc = repair_mountrw(*repv, (VolumeId *)space, MAXHOSTS, msgbuf, sizeof(msgbuf))) < 0) {
	strerr(msg, msgsize, "Could not allocate replica list");    
	repair_finish(*repv);
	return(-1);
    }

    /* Begin the repair */
    sprintf(cmd, "%d 1", REP_CMD_BEGIN);
    vioc.in = cmd;
    vioc.in_size = (short)(strlen(cmd) + 1);
    vioc.out = space;
    vioc.out_size = (short)sizeof(space);
    if ((rc = pioctl((*repv)->rodir, VIOC_REP_CMD, &vioc, 0)) < 0) {
	strerr(msg, msgsize, "REP_CMD_BEGIN failed: %s", strerror(errno));
	repair_finish(*repv);
	return(-1);
    }

    /* Determine conflict type */
    sscanf(vioc.out, "%d", &rc);
    if (rc == 0)
	(*repv)->local = 1; /* LOCAL_GLOBAL */
    else if (rc == 1) {
	(*repv)->local = 1;
	if (EndRepair(*repv, 0, msgbuf, sizeof(msgbuf)) < 0)
	    strerr(msg, msgsize, "Local-global repair session already in progress, %s", msgbuf);
	else strerr(msg, msgsize, "Local-global repair session already in progress");
	return(-1);
    }
    else if (rc == 2)
	(*repv)->local = 0; /* SERVER_SERVER */
    else { /* (rc < 0) || (rc > 2) */
	strerr(msg, msgsize, "Bogus return code from venus (%d)", rc);
	repair_finish(*repv);
	return(-1);
    }
    if (findtype(*repv) < 0) {
	if (EndRepair(*repv, 0, msgbuf, sizeof(msgbuf)) < 0)
	    strerr(msg, msgsize, "Could not determine conflict type, %s", msgbuf);
	else strerr(msg, msgsize, "Could not determine conflict type");
	return(-1);
    }

    return(0);
}

/* Clears inconsistencies on repv
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int ClearInc(struct repvol *repv, char *msg, int msgsize) {
    char msgbuf[DEF_BUF];
    ViceFid confFid, Fid[MAXHOSTS];
    vv_t vv[MAXHOSTS];
    struct ViceIoctl vioc;
    int rc, i, nreplicas;
    char **names, *user = NULL, *rights = NULL, *owner = NULL, *mode = NULL;

    if (repv == NULL) {
	strerr(msg, msgsize, "NULL repv");
	return(-1);
    }

    /* get replica names for doCompare */
    if ((nreplicas = getVolrepNames(repv, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	strerr(msg, msgsize, "Error getting replica names: %s\n", msgbuf);
	return(-1);
    }

    /* set the fid to 0.0 so docompare will not check quotas */
    confFid.Volume = repv->vid;
    confFid.Vnode = confFid.Unique = 0;
    /* do the compare */
    if (!doCompare(nreplicas, repv, names, "/dev/null", 
		   repv->mnt, &confFid, user, rights, owner, mode)) {
	/* XXXX if a get fid is done between two setvv's resolve might get called 
	   - therefore get the vv for each replica  before doing the setvv */
	for (i = 0; i < nreplicas; i++) {
	    if ((rc = repair_getfid(names[i], &Fid[i], &vv[i], msgbuf, sizeof(msgbuf))) < 0) {
		strerr(msg, msgsize, "repair_getfid(%s): %s", names[i], msgbuf);
		goto CLEANUP;
	    }
	}

	if ((Fid[0].Vnode == 1) && (Fid[0].Unique == 1) 
	    && compareQuotas(nreplicas, names)) {
	    strerr(msg, msgsize, "Volume Quotas are different");
	    goto CLEANUP;
	}

	for (i = 0; i < nreplicas; i++) {
	    ClearIncon(vv[i]);
	    vioc.in = (char *)&vv[i];
	    vioc.in_size = sizeof(vv_t);
	    vioc.out = NULL;
	    vioc.out_size = 0;
	    if (rc = pioctl(names[i], VIOC_SETVV, &vioc, 0)) {
		strerr(msg, msgsize, "SETVV %s: %s", names[i], strerror(errno));
		goto CLEANUP;
	    }
	}

	for (i = 0; i < nreplicas; i++) 
	    freeif(names[i]);
	free(names);
	return(0);
    }
    else strerr(msg, msgsize, "Replicas not identical, can't clear inconsistency");

 CLEANUP:
    for (i = 0; i < nreplicas; i++) 
	freeif(names[i]);
    free(names);
    return(-1);
}

int CompareDirs(struct repvol *repv, char *fixfile, char *user, char *rights, 
		char *owner, char *mode, char *msg, int msgsize) {
    char msgbuf[DEF_BUF], space[DEF_BUF], tmppath[MAXPATHLEN];
    VolumeId vid;
    ViceFid confFid;
    vv_t confvv;
    struct ViceIoctl vioc;
    struct stat buf;
    char **names;
    int rc, i, nreps;

    if (repair_getfid(repv->rodir, &confFid, &confvv, msgbuf, sizeof(msgbuf))) {
	strerr(msg, msgsize, "repair_getfid(%s): %s", repv->rodir, msgbuf);
	return(-1);
    }
    if (!ISDIR(confFid.Vnode) || !(repv->dirconf)) {
	strerr(msg, msgsize, "Compare can only be performed on directory replicas");
	return(-1);
    }

    /* get replica names for doCompare */
    if ((nreps = getVolrepNames(repv, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	strerr(msg, msgsize, "Error getting replica names: %s", msgbuf);
	return(-1);
    }

 redocompare:
    /* do the compare */
    rc = doCompare(nreps, repv, names, fixfile, repv->mnt, &confFid, user, rights, owner, mode);
    if (rc == 0) printf("No conflicts detected \n");
    else if (rc > 0) printf("Operations to resolve conflicts are in %s\n", fixfile);	
    else if (rc == NNCONFLICTS) {
	if (Parser_getbool("Do you want to repair the name/name conflicts", 1)) {
	    /* repair the name/name conflicts */
	    rc = makedff(fixfile, tmppath);
	    rc = dorep(repv, tmppath, NULL, 0);
	    if (!rc) { 
		/* name/name conflicts were repaired 
		 * if that was the only thing wrong, then the object is not inconsistent anymore.
		 * try to stat the individual replica - if it fails then just exit */
		if (stat(names[0], &buf)) {
		    /* object doesn't exist anymore */
		    printf("The directory was repaired and is no longer inconsistent\n");
		    printf("You can quit now :-)\n");
		    goto Exit;
		}
		else /* the object is still there so the dir is still inconsistent */
		    goto redocompare;
	    }
	    else  {
		printf("Couldn't remove name/name conflicts successfully");
		printf(" - Try another compare\n");
	    }
	}
    }
 Exit:
    /* free the malloced space */
    for (i = 0; i < nreps; i++) freeif(names[i]);
    free(names);
}

/* Discards all local mutations to volume under repair
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int DiscardAllLocal(struct repvol *repv, char *msg, int msgsize) {
    char space[DEF_BUF];
    char buf[BUFSIZ];
    struct ViceIoctl vioc;
    int rc;

    if ((repv == NULL) || !(repv->local)) { 
	strerr(msg, msgsize, "Invalid repv");
	return(-1);
    }

    sprintf(buf, "%d", REP_CMD_DISCARD_ALL);
    vioc.in = buf;
    vioc.in_size = (short)strlen(buf) + 1;
    vioc.out = space;
    vioc.out_size = sizeof(space);
    rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0);
    if (rc < 0) strerr(msg, msgsize, "%s", space);
    return(rc);
}

/* Performs the repair on repv using fixfile at ufixpath
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int DoRepair(struct repvol *repv, char *ufixpath, char *msg, int msgsize) {
    char space[DEF_BUF], fixpath[MAXPATHLEN];
    VolumeId *vids;
    long *rcodes;
    int i, rc;
    struct volrep *rwv;

    if (repv->dirconf) { /* directory conflict */
	if (makedff(ufixpath, fixpath) < 0) { /* Create internal form of fix file */
	    strerr(msg, msgsize, "Error in fix file");
	    return(-1);
	}
    }
    else strncpy(fixpath, ufixpath, sizeof(fixpath));

    /* Do the repair */
    rc = dorep(repv, fixpath, space, sizeof(space));
    if (rc < 0 && errno != ETOOMANYREFS) {
	strerr(msg, msgsize, "VIOC_REPAIR %s: %s", repv->rodir, strerror(errno));
	return(-1);
    }

    vids = (VolumeId *)space;
    rcodes = (long *)(((long *)space) + MAXHOSTS);

    for (rwv = repv->rwhead; rwv != NULL; rwv = rwv->next) {
	for (i = 0; ((i < MAXHOSTS) && (vids[i] != rwv->vid)); i++);
	if  (i < MAXHOSTS) {
	    printf("Repair actions performed on %s have %s",
		   rwv->srvname, rcodes[i] ? "failed" : "succeeded");
	    if (rcodes[i]) {
		printf(" (%ld).\n", rcodes[i]); 
		printf("Possible causes: disconnection, lack of authentication, lack of server space\n");
		printf("Fix file contains operations that are in conflict against the server replica.\n");
	    }
	    else printf(".\n");
	    vids[i] = 0;
	}
	else printf("No return code for actions performed on %s! (vid 0x%lx)\n", rwv->srvname, rwv->vid);
    }
    for (i = 0; (i < MAXHOSTS); i++)
	if (vids[i]) printf("Return code %d for unexpected vid 0x%lx!\n", rcodes[i], vids[i]);

    /* Clean up */
    if (repv->dirconf) unlink(fixpath); /* ignore rc */
    return(0);
}

/* Ends the repair session on repv and frees up memory associated with repv
 * Only commits changes in for a local/global conflict if commit == 1
 * Returns 0 on success, -1 on error and fills in msg if non-NULL 
                          (but still frees memory) */
int EndRepair(struct repvol *repv, int commit, char *msg, int msgsize) {
    char space[DEF_BUF], cmd[32];
    struct ViceIoctl vioc;
    int rc;

    if (repv->local) { /* local/global conflict */
	sprintf(cmd, "%d %d", REP_CMD_END, commit);
	vioc.in = cmd;
	vioc.in_size = (short)(strlen(cmd) + 1);
	vioc.out = space;
	vioc.out_size = sizeof(space);
	if ((rc = pioctl("/coda", VIOC_REP_CMD, &vioc, 0)) < 0)
	    strerr(msg, msgsize, "REP_CMD_END failed: %s", strerror(errno));
	else if (strcmp(vioc.out, "repair session completed") != 0) {
	    strerr(msg, msgsize, "%s", vioc.out);
	    rc = -1;
	}
    }
    else { /* server/server conflict */
	vioc.in = NULL;
	vioc.in_size = 0;
	vioc.out = NULL;
	vioc.out_size = 0;
	errno = 0;
	if ((rc = pioctl(repv->mnt, VIOC_DISABLEREPAIR, &vioc, 0)) < 0)
	    strerr(msg, msgsize, "DISABLEREPAIR %s: %s", repv->mnt, strerror(errno));
    }
    repair_finish(repv);
    return(rc);
}

/* Removes inconsistencies on repv (first does a repair, then clears the inc)
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int RemoveInc(struct repvol *repv, char *msg, int msgsize) {
    char msgbuf[DEF_BUF], tmppath[MAXPATHLEN];
    struct stat buf;
    ViceFid fixfid, confFid;
    ViceVersionVector confvv;
    vv_t fixvv;
    char *user = NULL, *rights = NULL, *owner = NULL, *mode = NULL, **names;
    int nreplicas, rc, i, j;
    struct volrep  *rwv;
    struct listhdr *repairlist;
    resreplica *dirs;

    user = rights = owner = mode = NULL;

    if ((nreplicas = getVolrepNames(repv, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	strerr(msg, msgsize, "Error getting replica names: %s", msgbuf);
	return(-1);
    }

    if (repv->dirconf) { /* directory conflict */
	/* get the directory entries and create list of children to be removed */
	if (getunixdirreps(nreplicas, names, &dirs)) {
	    strerr(msg, msgsize, "Could not get needed replica information");
	    rc = -1;
	    goto Error;	    
	}
	getremovelists(nreplicas, dirs, &repairlist);

	/* convert list to internal format */
	strcpy(tmppath, "/tmp/REPAIR.XXXXXX");
	mktemp(tmppath);

	/* write out internal rep */
	rc = repair_putdfile(tmppath, nreplicas, repairlist);
	if (rc) {
	    strerr(msg, msgsize, "Coudn't put repair list into file %s", tmppath);
	    goto Error;
	}

	rc = dorep(repv, tmppath, NULL, 0); /* do the repair */
	if (rc < 0 && errno != ETOOMANYREFS) {
	    strerr(msg, msgsize, "REPAIR %s: %s", repv->rodir, strerror(errno));
	    unlink(tmppath); /* Clean up */
	    goto Error;
	}    
	unlink(tmppath); /* Clean up */

	/* clear the inconsistency if needed and possible */ 
	if ((rc = repair_getfid(repv->rodir, &confFid, &confvv, msgbuf, sizeof(msgbuf))) < 0) {
	    strerr(msg, msgsize, "repair_getfid(%s): %s", repv->rodir, msgbuf);
	    goto Error;
	}
	if (!((confvv.StoreId.Host == -1) && (confvv.StoreId.Uniquifier == -1))) {
	    strerr(msg, msgsize, "Unexpected values (Host = %d, Uniquifier = %d)", 
		   confvv.StoreId.Host, confvv.StoreId.Uniquifier);
	    rc = -1;
	    goto Error;
	}
	/* object is still inconsistent -- try to clear it */
	if ((rc = ClearInc(repv, msgbuf, sizeof(msgbuf))) < 0) {
	    strerr(msg, msgsize, "Error clearing inconsistency: %s", msgbuf);
	    goto Error;
	}
    }
    else { /* file conflict */
	if (rc = repair_getfid(names[0], &fixfid, &fixvv, msgbuf, sizeof(msgbuf))) {
	    strerr(msg, msgsize, "repair_getfid(%s): %s", names[0], msgbuf);
	    goto Error;
	}

	sprintf(tmppath, "@%lx.%lx.%lx", fixfid.Volume, fixfid.Vnode, fixfid.Unique);

	rc = dorep(repv, tmppath, NULL, 0); /* do the repair */
	if ((rc < 0) && (errno != ETOOMANYREFS))
	    strerr(msg, msgsize, "REPAIR %s: %s", repv->rodir, strerror(errno));
    }

 Error:
    /* clean up malloced memory */
    if (repv->dirconf) /* directory conflict */
	resClean(nreplicas, dirs, repairlist);
    for (i = 0; i < nreplicas; i++)
	freeif(names[i]);
    free(names);

    return(rc);
}

static int compareAcl(int nreplicas, resreplica *dirs) {
    int i, j;
    struct Acl *al0, *ali;
    al0 = dirs[0].al;

    for (i = 1; i < nreplicas; i++){
	ali = dirs[i].al;

	if (ali->nplus != al0->nplus || ali->nminus != al0->nminus)
	    return -1;
	for (j = 0; j < al0->nplus; j++){
	    if (strcmp((al0->pluslist)[j].name, (ali->pluslist)[j].name))
		return -1;
	    if ((al0->pluslist)[j].rights != (ali->pluslist)[j].rights)
		return -1;
	}
	for (j = 0; j < al0->nminus; j++){
	    if (strcmp((al0->minuslist)[j].name, (ali->minuslist)[j].name))
		return -1;
	    if ((al0->minuslist)[j].rights != (ali->minuslist)[j].rights)
		return -1;
	}
    }
    return 0;
}

static int compareOwner(int nreplicas, resreplica *dirs) {
    int i;
    for (i = 1; i < nreplicas; i++) {
	if (dirs[i].owner != dirs[0].owner)
	    return -1;
    }
    return 0;
}

static int compareQuotas(int nreplicas, char **names) {
    if (nreplicas <= 1) {
	printf("Comparing Quotas: Not enough replicas to compare\n");
	return 0;
    }
    char piobuf[DEF_BUF];    
    struct ViceIoctl vio;
    vio.in = NULL;
    vio.in_size = 0;
    vio.out = piobuf;
    vio.out_size = sizeof(piobuf);

    /* Do the pioctl */
    int rc = pioctl(names[0], VIOCGETVOLSTAT, &vio, 1);
    if (rc <0) {fflush(stdout); perror(names[0]); return(1);}
    /* Get pointers to output fields */
    VolumeStatus *vs = (VolumeStatus *)piobuf;
    int minquota0 = (int) vs->MinQuota;
    int maxquota0 = (int) vs->MaxQuota;
    for (int i = 1; i < nreplicas; i++) {
	vio.in = NULL;
	vio.in_size = 0;
	vio.out = piobuf;
	vio.out_size = sizeof(piobuf);

	/* Do the pioctl */
	rc = pioctl(names[i], VIOCGETVOLSTAT, &vio, 1);
	if (rc <0) {fflush(stdout); perror(names[i]); return(1);}
	/* Get pointers to output fields */
	vs = (VolumeStatus *)piobuf;
	if ((vs->MinQuota !=  minquota0) || (vs->MaxQuota != maxquota0)) 
	    return 1;
    }
    return 0;
}

static int compareStatus(int nreplicas, resreplica *dirs) {
    int i;
    for (i = 1; i < nreplicas; i++) {
	if (dirs[i].modebits != dirs[0].modebits)
	    return -1;
    }
    return 0;
}

int compareVV(int nreplicas, char **names, struct repvol *repv) {
    char msgbuf[DEF_BUF];
    vv_t vv[MAXHOSTS];
    vv_t *vvp[MAXHOSTS];
    ViceFid fid;
    int nhosts = 0;
    int i;
    int HowMany = 0;

    for (i = 0; i < MAXHOSTS; i++) 
	vvp[i] = NULL;

    for (i = 0; i < nreplicas; i++) {
	if (repair_getfid(names[i], &fid, &vv[nhosts], msgbuf, sizeof(msgbuf)))
	    printf("Couldn't get vv for %s: %s\n", names[i], msgbuf);
	else
	    nhosts++;
    }
    for (i = 0; i < nhosts; i++)
	vvp[i] = &vv[i];
    if (VV_Check_IgnoreInc(&HowMany, vvp, 1) != 1)
	return(1);
    return(0);
}

static int doCompare(int nreplicas, struct repvol *repv, char **names, char *filepath, char *volmtpt, 
		     ViceFid *incfid, char *user, char *rights, char *owner, char *mode) {
    char compOutputFile[MAXPATHLEN];
    struct stat sbuf;
    resreplica *dirs;
    struct  listhdr *k;
    int i, setmode = 0, setacl = 0, setowner = 0;
    unsigned long j;
    FILE *file;
    struct volrep *rwv;

    if (nreplicas == 1) {
	printf("DoCompare: Since there is only one replica, nothing to compare\n");
	return(0);
    }
    compOutputFile[0] = '\0';
    if (!strcmp(filepath, "stdout"))
	file = stdout;
    else {
	file = fopen(filepath, "w");
	if (!file) {
	    printf("Couldn't open file %s for writing\n", filepath);
	    return(-2);
	}
	if (strcmp(filepath, "/dev/null"))
	    sprintf(compOutputFile, "%s", filepath);
    }
    /* set the global RepVolume to the volume we are repairing */
    RepVolume = repv->vid;
    
    if (getunixdirreps(nreplicas, names, &dirs)) {
	printf("compare failed because of failure in getting needed replica information\n");
	return -1;
    }
    int rc = dirresolve(nreplicas, dirs, NULL, &k, volmtpt);

    if (compareAcl(nreplicas, dirs)){
	nConflicts ++;
	if ( user && rights ) {
	    printf("Acls will be set to %s %s.\n", user, rights);
	    setacl = 1;
	} 
	else printf("Acls differ: Please repair manually using setacl <user> <rights>\n");
    }
    if (compareStatus(nreplicas, dirs)){
	nConflicts++;
	if ( mode ) {
	    printf("Modebits will be set to %s.\n", mode);
	    setmode = 1;
	} 
	else printf("Modebits differ - a repair should set the bits\n");
    }
    if (compareOwner(nreplicas, dirs)) {
	nConflicts++;
	if ( owner ) {
	    printf("owner uid will be set to %s\n", owner);
	    setowner = 1;
	} 
	else printf("Owner differs: Please repair manually using setowner <uid>\n");
    }

    for (i = 0; i < nreplicas; i++){
	/* find the server name */
	{
	    rwv = repv->rwhead;
	    while (rwv && k[i].replicaId != rwv->vid)
		rwv = rwv->next;
	    CODA_ASSERT(k[i].replicaId == rwv->vid);
	}
	fprintf(file,"\nreplica %s %lx \n", rwv->srvname, k[i].replicaId);
	for (j = 0; j < k[i].repairCount; j++)
	    repair_printline(&(k[i].repairList[j]), file);
	if ( setacl )
	    fprintf(file, "\tsetacl %s %s\n", user, rights);
	if ( setmode )
	    fprintf(file, "\tsetmode %s\n", mode);
	if ( setowner )
	    fprintf(file, "\tsetowner %s\n", owner);
    }

    if (file != stdout) {
	fclose(file);
	if (compOutputFile[0]) {
	    // save the information about the compare file produced
	    if (stat(compOutputFile, &sbuf)) {
		printf("Stat of %s failed\n", compOutputFile);
		compOutputFile[0] = '\0';
	    }
	}
    }

    if (compareVV(nreplicas, names, repv)) {
	if (!nConflicts) {
	    // warn the user if no conflicts were detected otherwise
	    printf("The fix file may be empty but .... \n"
		   "You still need a dorepair because the "
		   "Version state is different\n");
	}
	nConflicts++;
    }
    if ((incfid->Vnode == 1) && (incfid->Unique == 1) &&
	(compareQuotas(nreplicas, names))) {
	nConflicts++;
	printf("Your volume quota is different on the various replicas\n");
	printf("You SHOULD talk to one of the system administrators soon\n");
    }
    /* clean up the malloced memory */
    resClean(nreplicas, dirs, k);
    if (rc == NNCONFLICTS) return(NNCONFLICTS);
    return(nConflicts);
}

static int dorep(struct repvol *repv, char *path, char *buf, int len) {
    char space[DEF_BUF];
    struct ViceIoctl vioc;
    int rc;

    if (buf == NULL) { 
	buf = space;
	len = sizeof(space);
    }
    vioc.in = path;
    vioc.in_size = strlen(path) + 1;
    vioc.out = buf;
    vioc.out_size = len;
    memset(buf, 0, len);
    rc = pioctl(repv->rodir, VIOC_REPAIR, &vioc, 0);
    return(rc);
}

/* Assumes rw replicas are all of the same type
 *    (must be true if this is a leftmost conflict)
 * Fills in dirconf field in repv
 * Returns 0 on success, -1 on failure */
static int findtype(struct repvol *repv) {
    char tmppath[MAXPATHLEN];
    struct stat sbuf;
    struct volrep *tmp;

    tmp = repv->rwhead;
    if (tmp == NULL) return(-1); /* need at least 1 replica */

    /* do the first one manually */
    sprintf(tmppath, "%s/%s", repv->rodir, tmp->compname);
    if (lstat(tmppath, &sbuf) < 0) return(-1);
    repv->dirconf = ((sbuf.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0;
    tmp = tmp->next;

    while (tmp != NULL) {
	sprintf(tmppath, "%s/%s", repv->rodir, tmp->compname);
	if (lstat(tmppath, &sbuf) < 0) return(-1);
	if (repv->dirconf != (((sbuf.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0))
	    return(-1);
	tmp = tmp->next;
    }
    return(0);
}

static void getremovelists(int nreplicas, resreplica *dirs, struct listhdr **repairlist) {
    struct repair rep;
    resdir_entry *rde;
    int i, j;

    InitListHdr(nreplicas, dirs, repairlist);
    /* for each replica create the list of entries to be removed */
    for ( i = 0 ; i < nreplicas; i++) {
	for ( j = 0; j < dirs[i].nentries; j++) {
	    rde = &(direntriesarr[dirs[i].entry1 + j]);
	    if (ISDIR(rde->vno)) 
		rep.opcode = REPAIR_REMOVED;
	    else 
		rep.opcode = REPAIR_REMOVEFSL;
	    strcpy(&(rep.name[0]), &(rde->name[0]));
	    rep.parms[0] = 0;
	    rep.parms[1] = 0;
	    rep.parms[2] = 0;
	    InsertListHdr(&rep, repairlist, i);
	}
    }
}

/* Allocates and returns an array of replica names (complete paths) in ***names
 * Returns number of replicas in array on success, -1 on failure (after cleaning up) */
static int getVolrepNames(struct repvol *repv, char ***names, char *msg, int msgsize) {
    struct volrep *rwv;
    int i, nreps, len;

    /* count and allocate replicas */
    for (nreps = 0, rwv = repv->rwhead; rwv != NULL; rwv = rwv->next, nreps++);
    if (nreps == 0) {
	strerr(msg, msgsize, "No accessible replicas");
	return(-1);
    }
    if ((*names = (char **)malloc(nreps * sizeof(char *))) == NULL) {
	strerr(msg, msgsize, "Malloc failed");
	return(-1);
    }
    for (i = 0; i < nreps; (*names)[i++] = NULL); /* initialize all to NULL */

    for (i = 0, rwv = repv->rwhead; rwv != NULL; rwv = rwv->next, i++) {
	/* 3 is for middle slash, trailing slash (if directory) and closing '\0' */
	(*names)[i] = (char *)malloc((strlen(rwv->compname) + strlen(repv->rodir) + 3) * sizeof(char));
	if ((*names)[i] == NULL) {
	    for (i = 0; i < nreps; nreps++) freeif((*names)[i]);
	    free(*names);
	    strerr(msg, msgsize, "Malloc failed");
	    return(-1);      
	}

	if (repv->dirconf) sprintf((*names)[i], "%s/%s/", repv->rodir, rwv->compname);
	else sprintf((*names)[i], "%s/%s" , repv->rodir, rwv->compname);	
    }

    return (nreps);
}

/* extfile: external (ASCII) rep
   intfile: internal (binary) rep
   Returns 0 on success, -1 on failures
*/
static int makedff(char *extfile, char *intfile /* OUT */) {
    struct listhdr *hl;
    int hlc, rc;

    /* parse input file and obtain internal rep  */
    rc = repair_parsefile(extfile, &hlc, &hl);
    if (rc < 0) return(-1);

    /* generate temp file name */
    strcpy(intfile, "/tmp/REPAIR.XXXXXX");
    mktemp(intfile);

    /* write out internal rep */
    rc = repair_putdfile(intfile, hlc, hl);
    if (rc) return (-1);
    // repair_printfile(intfile);
  
    return(0);
}

static void printAcl(struct Acl *acl) {
    int i;
    
    printf("There are %d plus entries\n", acl->nplus);
    for (i = 0; i < acl->nplus; i++)
	printf("%s \t %ld\n", ((acl->pluslist)[i]).name, ((acl->pluslist)[i]).rights);
    printf("There are %d negative entries\n", acl->nminus);
    for (i = 0; i < acl->nminus; i++)
	printf("%s \t %ld\n", ((acl->minuslist)[i]).name, ((acl->minuslist)[i]).rights);
    printf("End of Access List\n");
}
