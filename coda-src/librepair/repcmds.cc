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

#include "repcmds.h"

int  compareAcl(int, resreplica *);
int  compareOwner(int, resreplica *);
int  compareQuotas(int , char **);
int  compareStatus(int, resreplica *);
int  compareVV(int, char **, struct repvol *);
int  findtype(struct repvol *);
void getremovelists(int, resreplica *, struct listhdr **);
int  getVolrepNames(struct repvol *repv, char ***names, char *msg, int msgsize);
int  isLocal(resreplica *dir);
void printAcl(struct Acl *);

/* Assumes pathname is the path of a conflict
 * Fills in repv with repvol created in beginning repair
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int BeginRepair(char *pathname, struct repvol **repv, char *msg, int msgsize)
{
    char msgbuf[DEF_BUF], space[DEF_BUF], cmd[32];
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
	    strerr(msg, msgsize, "ENABLEREPAIR %s: %s", (*repv)->rodir, strerror(errno));
	repair_finish(*repv);
	return(-1);
    }

    /* Mount the rw replicas, i.e. just insert into list in repv */
    if ((rc = repair_mountrw(*repv, (VolumeId *)space, MAXHOSTS, msgbuf, sizeof(msgbuf))) < 0) {
	strerr(msg, msgsize, "%s\nCould not allocate replica list", msgbuf);    
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
	(*repv)->local = 1; /* local/global */
    else if (rc == 1) {
	(*repv)->local = 1;
	if (EndRepair(*repv, 0, msgbuf, sizeof(msgbuf)) < 0)
	    strerr(msg, msgsize, "Local-global repair session already in progress, %s", msgbuf);
	else strerr(msg, msgsize, "Local-global repair session already in progress");
	return(-1);
    }
    else if (rc == 2)
	(*repv)->local = 0; /* server/server */
    else if (rc == 3)
	(*repv)->local = 2; /* mixed lg_ss */
    else { /* (rc < 0) || (rc > 3) */
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
int ClearInc(struct repvol *repv, char *msg, int msgsize)
{
    char msgbuf[DEF_BUF];
    ViceFid confFid, Fid[MAXHOSTS];
    vv_t vv[MAXHOSTS];
    struct ViceIoctl vioc;
    int rc, i, nreplicas;
    struct repinfo inf;
    char **names;

    memset(&inf, 0, sizeof(inf));

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
    if (!CompareDirs(repv, "/dev/null", &inf, msgbuf, sizeof(msgbuf))) {
	/* XXXX if a get fid is done between two setvv's resolve might get called 
	   - therefore get the vv for each replica  before doing the setvv */
	for (i = 0; i < nreplicas; i++) {
	    if ((rc = repair_getfid(names[i], &Fid[i], NULL, &vv[i], msgbuf,
				    sizeof(msgbuf))) < 0) {
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
	    rc = pioctl(names[i], VIOC_SETVV, &vioc, 0);
	    if (rc) {
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

/* Does the compare on repv, and writes to fixfile
 * Returns number of conflicts on success, -1 on error and fills in msg if non-NULL 
 * Returns -2 if there are name/name conflicts (in which case the caller
 * should DoRepair to fix them and then do CompareDirs again) */
int CompareDirs(struct repvol *repv, char *fixfile, struct repinfo *inf, char *msg, int msgsize)
{
    char msgbuf[DEF_BUF];
    ViceFid confFid;
    char **names;
    char realm[MAXHOSTNAMELEN];
    int ret, i, nreps, nConflicts = 0, setmode = 0, setacl = 0, setowner = 0;
    unsigned long j;
    resreplica *dirs = NULL;
    struct volrep *rwv;
    struct  listhdr *k = NULL;
    FILE *file;

    if ((repv == NULL) || (inf == NULL)) {
      strerr(msg, msgsize, "NULL %s", (repv ? "repinfo" : "repv"));
      return(-1);
    }

    if (repair_getfid(repv->rodir, &confFid, realm, NULL, msgbuf, sizeof(msgbuf))) {
	strerr(msg, msgsize, "repair_getfid(%s): %s", repv->rodir, msgbuf);
	return(-1);
    }

    if (!ISDIRVNODE(confFid.Vnode) || !(repv->dirconf)) {
	strerr(msg, msgsize, "Compare can only be performed on directory replicas");
	return(-1);
    }

    if ((nreps = getVolrepNames(repv, &names, msgbuf, sizeof(msgbuf))) <= 0) {
	strerr(msg, msgsize, "Error getting replica names: %s", msgbuf);
	return(-1);
    }
 
    /* Open the fixfile */
    if (!strcmp(fixfile, "stdout")) file = stdout;
    else if (!strcmp(fixfile, "stderr")) file = stderr;
    else {
	if ((file = fopen(fixfile, "w")) == NULL) {
	    strerr(msg, msgsize, "Could not open file %s for writing: %s", fixfile, strerror(errno));
	    goto Cleanup;
	}
    }

    /* Set the global RepVolume to the volume we are repairing */
    /* RepVolume = repv->vid;  XXXX Why do we need this??  -Remington */

    if (getunixdirreps(nreps, names, &dirs)) {
	strerr(msg, msgsize, "Could not get replica information");
	goto Cleanup;
    }

    /* Do the resolve! */
    ret = dirresolve(nreps, dirs, NULL, &k, repv->mnt, repv->vid, inf, realm);

    if (compareAcl(nreps, dirs)){
	nConflicts++;
	if ( inf->user && inf->rights ) {
	    printf("Acls will be set to %s %s.\n", inf->user, inf->rights);
	    setacl = 1;
	} 
	else printf("Acls differ: Please repair manually using setacl <user> <rights>\n");
    }

    if (compareOwner(nreps, dirs)) {
	nConflicts++;
	if ( inf->owner ) {
	    printf("owner uid will be set to %s\n", inf->owner);
	    setowner = 1;
	} 
	else printf("Owner differs: Please repair manually using setowner <uid>\n");
    }

    if (compareStatus(nreps, dirs)){
	nConflicts++;
	if ( inf->mode ) {
	    printf("Modebits will be set to %s.\n", inf->mode);
	    setmode = 1;
	} 
	else printf("Modebits differ - a repair should set the bits\n");
    }

    for (i = 0; i < nreps; i++) {
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
	    fprintf(file, "\tsetacl %s %s\n", inf->user, inf->rights);
	if ( setmode )
	    fprintf(file, "\tsetmode %s\n", inf->mode);
	if ( setowner )
	    fprintf(file, "\tsetowner %s\n", inf->owner);
    }

    /* Close the fixfile */
    if ((file != stdout) && (file != stderr))
	fclose(file);

    if (compareVV(nreps, names, repv)) {
	if (!nConflicts) {
	    // warn the user if no conflicts were detected otherwise
	    printf("The fix file may be empty but .... \n"
		   "You still need a dorepair because the "
		   "Version state is different\n");
	}
	nConflicts++;
    }

    if ((confFid.Vnode == 1) && (confFid.Unique == 1) &&
	(compareQuotas(nreps, names))) {
	nConflicts++;
	printf("Your volume quota is different on the various replicas\n");
	printf("You SHOULD talk to one of the system administrators soon\n");
    }

    /* clean up the malloced memory */
    resClean(nreps, dirs, k);
    for (i = 0; i < nreps; i++) freeif(names[i]);
    free(names);

    if (ret == NNCONFLICTS) {
      strerr(msg, msgsize, "Name/name conflicts still exist");
      return(-2);
    }
    return(nConflicts);
    
   Cleanup:
    resClean(nreps, dirs, k);
    for (i = 0; i < nreps; i++) freeif(names[i]);
    free(names);
    return(-1);
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

/* Does a repair on repv using fixfile at ufixpath
 * Fprints results to res if non-NULL
 * Returns 0 on success, -1 on error and fills in msg if non-NULL */
int DoRepair(struct repvol *repv, char *ufixpath, FILE *res, char *msg, int msgsize) {
    char space[DEF_BUF], fixpath[MAXPATHLEN], expath[MAXPATHLEN];
    VolumeId *vids;
    struct volrep *rwv;
    long *rcodes;
    int i, rc;

    if (repv == NULL) {
      strerr(msg, msgsize, "NULL repv");
      return(-1);
    }

    if (repv->dirconf) { /* directory conflict */
	if ((repv->local) &&  (repv->local != 2)) {
	    /* Expand all "global" entries into individual server replicas */
	    strcpy(expath, "/tmp/REPAIR.XXXXXX");
	    mktemp(expath);
	    copyfile_byname(ufixpath, expath);
	    if (chmod(expath, 0644) < 0) { unlink(expath); return(-1); }
	    if (glexpand(repv->rodir, expath, msg, msgsize) < 0) 
		{ unlink(expath); return(-1); }
	    if (makedff(expath, fixpath, msg, msgsize) < 0) return(-1);
	}
	/* Create internal form of fix file */
	else if (makedff(ufixpath, fixpath, msg, msgsize) < 0) return(-1);
    }
    else strncpy(fixpath, ufixpath, sizeof(fixpath));

    /* Do the repair */
    rc = dorep(repv, fixpath, space, sizeof(space));
    if (rc < 0 && errno != ETOOMANYREFS) {
	strerr(msg, msgsize, "VIOC_REPAIR %s: %s", repv->rodir, strerror(errno));
	return(-1);
    }

    /* Print out the results of VIOC_REPAIR pioctl */
    if (res != NULL) {
	vids = (VolumeId *)space;
	rcodes = (long *)(((long *)space) + MAXHOSTS);
	for (rwv = repv->rwhead; rwv != NULL; rwv = rwv->next) {
	    for (i = 0; ((i < MAXHOSTS) && (vids[i] != rwv->vid)); i++);
	    if  (i < MAXHOSTS) {
		fprintf(res, "Repair actions performed on %s have %s",
			rwv->srvname, rcodes[i] ? "failed" : "succeeded");
		if (rcodes[i]) {
		    fprintf(res, " (%ld).\n%s%s", rcodes[i], 
			    "Possible causes: disconnection, lack of authentication, lack of server space\n",
			    "Fix file contains operations that are in conflict against the server replica.\n");
		}
		else fprintf(res, ".\n");
		vids[i] = 0;
	    }
	    else fprintf(res, "No return code for actions performed on %s! (vid 0x%lx)\n", rwv->srvname, rwv->vid);
	}
	for (i = 0; (i < MAXHOSTS); i++)
	    if (vids[i]) fprintf(res, "Return code %d for unexpected vid 0x%lx!\n", rcodes[i], vids[i]);
    }

    /* Clean up */
    if (repv->dirconf) { 
	unlink(fixpath); /* ignore rc */
	if ((repv->local) &&  (repv->local != 2))
	    unlink(expath); /* ignore rc again */
    }
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
int RemoveInc(struct repvol *repv, char *msg, int msgsize)
{
    char msgbuf[DEF_BUF], tmppath[MAXPATHLEN];
    ViceFid fixfid;
    char fixrealm[MAXHOSTNAMELEN];
    ViceVersionVector confvv;
    vv_t fixvv;
    char *user = NULL, *rights = NULL, *owner = NULL, *mode = NULL, **names;
    int nreplicas, rc, i;
    struct listhdr *repairlist = NULL;
    resreplica *dirs = NULL;

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
	if ((rc = repair_getfid(repv->rodir, NULL, NULL, &confvv, msgbuf, sizeof(msgbuf))) < 0) {
	    strerr(msg, msgsize, "repair_getfid(%s): %s", repv->rodir, msgbuf);
	    goto Error;
	}
	if (!((confvv.StoreId.Host == (unsigned long)-1) && (confvv.StoreId.Uniquifier == (unsigned long)-1))) {
	    strerr(msg, msgsize, "Unexpected values (Host = %ld, Uniquifier = %ld)", 
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
	rc = repair_getfid(names[0], &fixfid, fixrealm, &fixvv, msgbuf, sizeof(msgbuf));
	if (rc) {
	    strerr(msg, msgsize, "repair_getfid(%s): %s", names[0], msgbuf);
	    goto Error;
	}

	sprintf(tmppath, "@%lx.%lx.%lx@%s", fixfid.Volume, fixfid.Vnode, fixfid.Unique, fixrealm);

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

int isLocal(resreplica *dir) {
    int i;

    /* local dirs get entry vnodes and storeID's of 0xffff */
    for (i = dir->entry1; i < (dir->entry1 + dir->nentries); i++) {
	if (~(direntriesarr[i].vno) || ~(direntriesarr[i].VV.StoreId.Host))
	    return(0);
    }
    if (~(dir->vnode)) return(0);   /* and vnode is 0xffff */
    if (dir->al != NULL) return(0); /* and NULL acl's */
    return(1);
}

int compareAcl(int nreplicas, resreplica *dirs) {
    int init, i, j;
    struct Acl *al0 = NULL, *ali;

    for (init = 0; init < nreplicas; init++) {
	if (!(isLocal(&(dirs[init]))))
	    al0 = dirs[init].al;
    }
    CODA_ASSERT(al0);

    for (i = init + 1; i < nreplicas; i++){

	if (isLocal(&(dirs[i])))
	    continue;

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

int compareOwner(int nreplicas, resreplica *dirs)
{
    int i;
    for (i = 1; i < nreplicas; i++) {
	if (dirs[i].owner != dirs[0].owner)
	    return -1;
    }
    return 0;
}

int compareQuotas(int nreplicas, char **names)
{
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
	if (rc < 0) {fflush(stdout); perror(names[i]); return(1);}
	/* Get pointers to output fields */
	vs = (VolumeStatus *)piobuf;
	if ((vs->MinQuota !=  minquota0) || (vs->MaxQuota != maxquota0)) 
	    return 1;
    }
    return 0;
}

int compareStatus(int nreplicas, resreplica *dirs) {
    int i;
    for (i = 1; i < nreplicas; i++) {
	if (dirs[i].modebits != dirs[0].modebits)
	    return -1;
    }
    return 0;
}

int compareVV(int nreplicas, char **names, struct repvol *repv)
{
    char msgbuf[DEF_BUF];
    vv_t vv[MAXHOSTS];
    vv_t *vvp[MAXHOSTS];
    int nhosts = 0;
    int i;
    int HowMany = 0;

    for (i = 0; i < MAXHOSTS; i++) 
	vvp[i] = NULL;

    for (i = 0; i < nreplicas; i++) {
	if (repair_getfid(names[i], NULL, NULL, &vv[nhosts], msgbuf, sizeof(msgbuf)))
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

/* Basically just nice padding for the VIOC_REPAIR pioctl.
 * Calls the pioctl, and passes back results in buf if non-NULL
 * Returns the pioctl return code */
int dorep(struct repvol *repv, char *fixpath, char *buf, int len) {
    char space[DEF_BUF];
    struct ViceIoctl vioc;
    int rc;

    if (buf == NULL) { 
	buf = space;
	len = sizeof(space);
    }
    vioc.in = fixpath;
    vioc.in_size = strlen(fixpath) + 1;
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
int findtype(struct repvol *repv) {
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

void getremovelists(int nreplicas, resreplica *dirs, struct listhdr **repairlist) {
    struct repair rep;
    resdir_entry *rde;
    int i, j;

    InitListHdr(nreplicas, dirs, repairlist);
    /* for each replica create the list of entries to be removed */
    for ( i = 0 ; i < nreplicas; i++) {
	for ( j = 0; j < dirs[i].nentries; j++) {
	    rde = &(direntriesarr[dirs[i].entry1 + j]);
	    if (ISDIRVNODE(rde->vno))
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
int getVolrepNames(struct repvol *repv, char ***names, char *msg, int msgsize) {
    struct volrep *rwv;
    int i, nreps;

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


static void growarray(char ***arrayaddr, int *arraysize) {
    *arraysize += 1; /* grow by one element */
    if (*arraysize > 1)
    	*arrayaddr = (char **)realloc(*arrayaddr, (*arraysize)*sizeof(char *));
    else 
	*arrayaddr = (char **)malloc(sizeof(char *));    
}

/*  Appends expanded entried to (ASCII) fixfile
 *  Returns 0 on success, -1 on failure */
int glexpand(char *rodir, char *fixfile, char *msg, int msgsize) {
    int rc, gls = 0, lineno = 0;
    unsigned int j, k, cnt;
    struct in_addr *custodians;
    VolumeId *repID, sID[VSG_MEMBERS];
    struct hostent *hent;
    struct ViceIoctl vio;
    char **snames = NULL, **glents = NULL, *tail = NULL;
    char mangle[MAXPATHLEN], line[MAXPATHLEN], gline[MAXPATHLEN];
    char piobuf[2048]; /* max size of pioctl buffer */
    struct repair rentry;
    FILE *ff;
    
    if ((ff = fopen(fixfile, "r")) == NULL) {
        strerr(msg, msgsize, "Could not open ASCII fixfile");
        return(-1);
    }

    gline[0] = '\0';
    for (fgets(line, sizeof(line), ff); !(feof(ff)); fgets(line, sizeof(line), ff)) {
	lineno++;
	strcpy(mangle, line);

	/* parse it */
	mangle[strlen(mangle)-1] = '\0'; /* nuke trailing \n */
	rc = repair_parseline(mangle, &rentry);
	switch(rc) {
	case 0:  /* good line */
	    if (rentry.opcode == REPAIR_REPLICA) { /* new replica */
		if (strlen(gline) > 0) goto Out;
		if (!strcmp(rentry.name, "global")) {
		    strcpy(gline, line);
		}
	    }
	    else if (strlen(gline) > 0) {
		growarray(&glents, &gls);
		glents[(gls - 1)] = strdup(line);
	    }
	    break; 	    
	case -2: /* blankline */
	    continue;
	case -1: /* some other bogosity */
	    strerr(msg, msgsize, "%s: Syntax error, line %d\n", fixfile, lineno);
	    fclose(ff);
	    return(-1);
	}
    }
 Out:
    if (fclose(ff) != 0) {
	strerr(msg, msgsize, "Error closing ASCII fixfile: %s", strerror(errno));
	return(-1);
    }
    for (j = 0; j < strlen(gline); j++) {
	if (strncmp(&(gline[j]), "global", 6) == 0) {
	    gline[j] = '\0';
	    tail = &(gline[j+6]);
	    break;
	}
    }
    if (tail == NULL) {
	strerr(msg, msgsize, "No \"global\" ASCII fixfile");
	return(-1);
    }

    /* Set up parms to pioctl */
    vio.in = 0;
    vio.in_size = 0;
    vio.out = piobuf;
    vio.out_size = sizeof(piobuf);

    /* Do the pioctl */
    rc = pioctl(rodir, VIOCWHEREIS, &vio, 1);
    if (rc < 0) {
        strerr(msg, msgsize, "VIOCWHEREIS failed");
        return(-1);
    }

    /* pioctl returns array of IP addrs */
    custodians = (struct in_addr *)piobuf;
    repID = (VolumeId *)(piobuf + (sizeof(struct in_addr) * VSG_MEMBERS));

    /* Count up hosts */
    for (cnt = 0, j = 0; j < 8; j++)
	if (custodians[j].s_addr == 0) cnt++;

    /* Create array of server names */
    snames = (char **)calloc(cnt, sizeof(char *));
    for (cnt = 0, j = 0; j < 8; j++) {
	if (custodians[j].s_addr == 0) continue;

	hent = gethostbyaddr((char *)&custodians[j], sizeof(long), AF_INET);
	sID[cnt] = repID[j];
	if (hent) {
	    snames[cnt] = strdup(hent->h_name);
	    for (k = 0; k < strlen(snames[cnt]); k++)
		snames[cnt][k] = tolower(snames[j][k]);
	}
	else 
	    snames[cnt] = inet_ntoa(custodians[j]);
	cnt++;
    }

    /* Append the new entries */
    if ((ff = fopen(fixfile, "a")) == NULL) {
	strerr(msg, msgsize, "Error opening ASCII fixfile: %s", strerror(errno));
	return(-1);
    }
    fprintf(ff, "\n");
    for (j = 0; j < cnt; j++) {
	//	fprintf(ff, "%s%s%s", gline, snames[j], tail);
	fprintf(ff, "%s%s %lx \n", gline, snames[j], sID[j]);
	for (k = 0; k < gls; k++)
	    fprintf(ff, "%s", glents[k]);
	fprintf(ff, "\n");
    }

    /* Clean up */
    for (j = 0; j < cnt; j++) 
	free(snames[j]);
    free(snames);
    if (fclose(ff) != 0) {
	strerr(msg, msgsize, "Error closing ASCII fixfile: %s", strerror(errno));
	return(-1);
    }
    return(0);
}

/*  extfile: external (ASCII) rep
 *  intfile: internal (binary) rep
 *  Returns 0 on success, -1 on failure */
int makedff(char *extfile, char *intfile, char *msg, int msgsize) {
    struct listhdr *hl;
    int hlc, rc;

    /* parse input file and obtain internal rep  */
    rc = repair_parsefile(extfile, &hlc, &hl);
    if (rc < 0) {
        strerr(msg, msgsize, "Error in fix file");
        return(-1);
    }

    /* generate temp file name */
    strcpy(intfile, "/tmp/REPAIR.XXXXXX");
    mktemp(intfile);

    /* write out internal rep */
    rc = repair_putdfile(intfile, hlc, hl);
    if (rc) {
        strerr(msg, msgsize, "Error in fix file");
        return(-1);
    }
    /* repair_printfile(intfile); */

    free(hl);
    return(0);
}

void printAcl(struct Acl *acl) {
    int i;
    
    printf("There are %d plus entries\n", acl->nplus);
    for (i = 0; i < acl->nplus; i++)
	printf("%s \t %ld\n", ((acl->pluslist)[i]).name, ((acl->pluslist)[i]).rights);
    printf("There are %d negative entries\n", acl->nminus);
    for (i = 0; i < acl->nminus; i++)
	printf("%s \t %ld\n", ((acl->minuslist)[i]).name, ((acl->minuslist)[i]).rights);
    printf("End of Access List\n");
}
